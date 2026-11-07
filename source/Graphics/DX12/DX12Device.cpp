#include "Graphics/DX12/DX12Device.hpp"
#include "Graphics/RHI/GpuCapabilities.hpp"

#include <vector>
#include <format>

namespace RL
{
    namespace
    {
        std::string ConvertWideToUtf8(const wchar_t* wideStr)
        {
            if (!wideStr || wideStr[0] == L'\0')
            {
                return {};
            }

            const int len = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
            if (len <= 0)
            {
                return {};
            }

            std::string result(static_cast<usize>(len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, result.data(), len, nullptr, nullptr);
            return result;
        }
    }

    DX12Device::DX12Device()
    {
        m_commandBuffer = std::make_unique<DX12CommandBuffer>(nullptr);
    }

    DX12Device::~DX12Device()
    {
        Shutdown();
    }

    std::expected<void, std::string> DX12Device::Initialize(HWND hwnd, u32 width, u32 height)
    {
        m_width = width;
        m_height = height;

        RL_LOG_INFO("Initializing DirectX 12 graphics subsystem...");

        auto devRes = CreateDeviceAndFactory();
        if (!devRes) return devRes;

        auto qRes = CreateCommandQueue();
        if (!qRes) return qRes;

        auto scRes = CreateSwapChain(hwnd, width, height);
        if (!scRes) return scRes;

        auto rtvHeapRes = CreateRtvDescriptorHeap();
        if (!rtvHeapRes) return rtvHeapRes;

        auto dsvHeapRes = CreateDsvDescriptorHeap();
        if (!dsvHeapRes) return dsvHeapRes;

        auto dsvRes = CreateDepthStencilView();
        if (!dsvRes) return dsvRes;

        auto frRes = CreateFrameResources();
        if (!frRes) return frRes;

        auto syncRes = CreateSyncObjects();
        if (!syncRes) return syncRes;

        RL_LOG_INFO("DirectX 12 subsystem initialized successfully. [Triple Buffering: ON, VRAM: {:.2f} GB]",
                    static_cast<f64>(m_dedicatedVramBytes) / (1024.0 * 1024.0 * 1024.0));
        return {};
    }

    void DX12Device::Shutdown() noexcept
    {
        if (!m_device)
        {
            return;
        }

        WaitForGPU();

        if (m_fenceEvent)
        {
            CloseHandle(m_fenceEvent);
            m_fenceEvent = nullptr;
        }

        for (u32 i = 0; i < NUM_FRAMES; ++i)
        {
            m_renderTargets[i].Reset();
            m_frameContexts[i].commandAllocator.Reset();
        }

        m_commandList.Reset();
        m_fence.Reset();
        m_depthStencilBuffer.Reset();
        m_dsvHeap.Reset();
        m_rtvHeap.Reset();
        m_swapChain.Reset();
        m_commandQueue.Reset();
        m_device.Reset();
        m_dxgiAdapter.Reset();
        m_dxgiFactory.Reset();

        RL_LOG_INFO("DirectX 12 subsystem shut down cleanly.");
    }

    std::expected<void, std::string> DX12Device::CreateDeviceAndFactory()
    {
        UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)

        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            RL_LOG_INFO("DirectX 12 Debug Layer enabled");
        }
        else
        {
            RL_LOG_WARN("Failed to enable DirectX 12 Debug Layer");
        }
#endif

        RL_CHECK_HR(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory)));

        BOOL allowTearing = FALSE;
        if (SUCCEEDED(m_dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        {
            m_tearingSupported = (allowTearing == TRUE);
        }
        RL_LOG_INFO("Hardware Variable Refresh Rate (Tearing): {}", m_tearingSupported ? "SUPPORTED" : "UNSUPPORTED");

        ComPtr<IDXGIAdapter1> bestAdapter;
        SIZE_T maxDedicatedVram = 0;

        for (UINT adapterIndex = 0;; ++adapterIndex)
        {
            ComPtr<IDXGIAdapter1> adapter;
            if (m_dxgiFactory->EnumAdapterByGpuPreference(
                    adapterIndex,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                if (desc.DedicatedVideoMemory >= maxDedicatedVram)
                {
                    maxDedicatedVram = desc.DedicatedVideoMemory;
                    bestAdapter = adapter;
                }
            }
        }

        if (!bestAdapter)
        {
            return std::unexpected("No suitable DirectX 12 (Feature Level 12_0+) hardware GPU adapter found.");
        }

        RL_CHECK_HR(bestAdapter.As(&m_dxgiAdapter));

        DXGI_ADAPTER_DESC1 adapterDesc{};
        m_dxgiAdapter->GetDesc1(&adapterDesc);

        m_gpuDescription = ConvertWideToUtf8(adapterDesc.Description);
        m_dedicatedVramBytes = adapterDesc.DedicatedVideoMemory;

        RL_LOG_INFO("Selected GPU Adapter: '{}' [Dedicated VRAM: {:.2f} MB]",
                    m_gpuDescription,
                    static_cast<f64>(m_dedicatedVramBytes) / (1024.0 * 1024.0));

        m_gpuInfo = RHI::GpuCapabilities::DetectPrimaryGpu();

        D3D_FEATURE_LEVEL featureLevelsToTest[] = {
            static_cast<D3D_FEATURE_LEVEL>(0xcc00),
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        for (auto fl : featureLevelsToTest)
        {
            if (SUCCEEDED(D3D12CreateDevice(m_dxgiAdapter.Get(), fl, IID_PPV_ARGS(&m_device))))
            {
                break;
            }
        }

        if (!m_device)
        {
            RL_CHECK_HR(D3D12CreateDevice(m_dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
        }

        RL_LOG_INFO("DirectX 12 Device initialized: '{}' [{}] [{}] [VRAM: {:.2f} GB | Shader Model: {}]",
                    m_gpuDescription,
                    m_gpuInfo.architectureName,
                    m_gpuInfo.GetFeatureLevelString(),
                    static_cast<f64>(m_dedicatedVramBytes) / (1024.0 * 1024.0 * 1024.0),
                    m_gpuInfo.GetShaderModelString());

#if defined(_DEBUG)

        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_device.As(&infoQueue)))
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

            D3D12_MESSAGE_ID hideMessages[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
            };

            D3D12_INFO_QUEUE_FILTER filter{};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hideMessages));
            filter.DenyList.pIDList = hideMessages;
            infoQueue->AddStorageFilterEntries(&filter);
        }
#endif

        return {};
    }

    std::expected<void, std::string> DX12Device::CreateCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        RL_CHECK_HR(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
        return {};
    }

    std::expected<void, std::string> DX12Device::CreateSwapChain(HWND hwnd, u32 width, u32 height)
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.Width              = width;
        swapChainDesc.Height             = height;
        swapChainDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo             = FALSE;
        swapChainDesc.SampleDesc.Count   = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount        = FrameBufferCount;
        swapChainDesc.Scaling            = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags              = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        RL_CHECK_HR(m_dxgiFactory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        ));

        RL_CHECK_HR(swapChain1.As(&m_swapChain));
        RL_CHECK_HR(m_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        return {};
    }

    std::expected<void, std::string> DX12Device::CreateRtvDescriptorHeap()
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = FrameBufferCount;
        rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask       = 0;

        RL_CHECK_HR(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        return {};
    }

    std::expected<void, std::string> DX12Device::CreateDsvDescriptorHeap()
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask       = 0;

        RL_CHECK_HR(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        return {};
    }

    std::expected<void, std::string> DX12Device::CreateDepthStencilView()
    {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask     = 1;
        heapProps.VisibleNodeMask      = 1;

        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Alignment          = 0;
        depthDesc.Width              = m_width;
        depthDesc.Height             = m_height;
        depthDesc.DepthOrArraySize   = 1;
        depthDesc.MipLevels          = 1;
        depthDesc.Format             = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count   = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear{};
        optClear.Format               = DXGI_FORMAT_D32_FLOAT;
        optClear.DepthStencil.Depth   = 1.0f;
        optClear.DepthStencil.Stencil = 0;

        RL_CHECK_HR(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optClear,
            IID_PPV_ARGS(&m_depthStencilBuffer)
        ));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice = 0;

        m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        return {};
    }

    void DX12Device::CreateRenderTargetViews()
    {
        const auto rtvHeapStart = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (u32 i = 0; i < FrameBufferCount; ++i)
        {
            RL_ASSERT_HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));

            const auto rtvHandle = OffsetCPUDescriptorHandle(rtvHeapStart, i, m_rtvDescriptorSize);
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        }
    }

    std::expected<void, std::string> DX12Device::CreateFrameResources()
    {
        CreateRenderTargetViews();

        for (u32 i = 0; i < NUM_FRAMES; ++i)
        {
            RL_CHECK_HR(m_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_frameContexts[i].commandAllocator)
            ));
        }

        RL_CHECK_HR(m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_frameContexts[m_frameIndex].commandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_commandList)
        ));

        RL_CHECK_HR(m_commandList->Close());
        return {};
    }

    std::expected<void, std::string> DX12Device::CreateSyncObjects()
    {
        RL_CHECK_HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

        for (u32 i = 0; i < NUM_FRAMES; ++i)
        {
            m_frameContexts[i].fenceValue = 0;
        }
        m_currentFenceValue = 0;

        m_fenceEvent = CreateEventExW(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        if (!m_fenceEvent)
        {
            return std::unexpected(std::format("Failed to create D3D12 fence synchronization event. Error: {}", GetLastError()));
        }

        return {};
    }

    void DX12Device::WaitForGPU()
    {
        if (!m_commandQueue || !m_fence || !m_fenceEvent)
        {
            return;
        }

        const u64 fenceValueToWait = ++m_currentFenceValue;
        if (SUCCEEDED(m_commandQueue->Signal(m_fence.Get(), fenceValueToWait)))
        {
            if (m_fence->GetCompletedValue() < fenceValueToWait)
            {
                if (SUCCEEDED(m_fence->SetEventOnCompletion(fenceValueToWait, m_fenceEvent)))
                {
                    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
                }
            }
        }
    }

    void DX12Device::ExecuteSingleTimeCommands(std::function<void(ID3D12GraphicsCommandList*)> action)
    {
        if (!action || !m_device || !m_commandQueue)
        {
            return;
        }

        ComPtr<ID3D12CommandAllocator> cmdAlloc;
        RL_ASSERT_HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        RL_ASSERT_HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList)));

        action(cmdList.Get());

        RL_ASSERT_HR(cmdList->Close());

        ID3D12CommandList* ppCommandLists[] = { cmdList.Get() };
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        WaitForGPU();
    }

    void DX12Device::Resize(u32 width, u32 height)
    {
        if (width == 0 || height == 0 || (width == m_width && height == m_height))
        {
            return;
        }

        RL_LOG_INFO("Resizing swap chain to {}x{}", width, height);
        m_width = width;
        m_height = height;

        WaitForGPU();

        for (u32 i = 0; i < NUM_FRAMES; ++i)
        {
            m_renderTargets[i].Reset();
            m_frameContexts[i].fenceValue = m_frameContexts[m_frameIndex].fenceValue;
        }

        m_depthStencilBuffer.Reset();

        const UINT flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        RL_ASSERT_HR(m_swapChain->ResizeBuffers(
            NUM_FRAMES,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            flags
        ));

        CreateRenderTargetViews();
        static_cast<void>(CreateDepthStencilView());
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    void DX12Device::BeginFrame()
    {
        if (m_isFrameActive) return;

        RL_ASSERT_HR(m_frameContexts[m_frameIndex].commandAllocator->Reset());

        RL_ASSERT_HR(m_commandList->Reset(m_frameContexts[m_frameIndex].commandAllocator.Get(), nullptr));

        const auto barrier = CreateTransitionBarrier(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        m_commandList->ResourceBarrier(1, &barrier);

        if (m_commandBuffer)
        {
            m_commandBuffer->SetCommandList(m_commandList.Get());
        }

        m_isFrameActive = true;
    }

    void DX12Device::ClearRenderTarget(f32 r, f32 g, f32 b, f32 a)
    {
        const auto rtvHandle = OffsetCPUDescriptorHandle(
            m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_frameIndex,
            m_rtvDescriptorSize
        );
        const auto dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        const f32 clearColor[4] = { r, g, b, a };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    void DX12Device::EndFrame()
    {
        if (!m_isFrameActive) return;

        const auto barrier = CreateTransitionBarrier(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );
        m_commandList->ResourceBarrier(1, &barrier);

        RL_ASSERT_HR(m_commandList->Close());

        ID3D12CommandList* const commandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, commandLists);

        m_isFrameActive = false;
    }

    void DX12Device::Present(bool vsync)
    {
        const UINT flags = (!vsync && m_tearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        RL_ASSERT_HR(m_swapChain->Present(vsync ? 1 : 0, flags));

        MoveToNextFrame();
    }

    void DX12Device::MoveToNextFrame()
    {
        const u64 currentFenceVal = ++m_currentFenceValue;
        RL_ASSERT_HR(m_commandQueue->Signal(m_fence.Get(), currentFenceVal));
        m_frameContexts[m_frameIndex].fenceValue = currentFenceVal;

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        if (m_fence->GetCompletedValue() < m_frameContexts[m_frameIndex].fenceValue)
        {
            RL_ASSERT_HR(m_fence->SetEventOnCompletion(m_frameContexts[m_frameIndex].fenceValue, m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        }
    }
}
