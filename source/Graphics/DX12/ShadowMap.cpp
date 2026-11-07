#include "Graphics/DX12/ShadowMap.hpp"
#include "Core/Logger.hpp"

namespace RL
{
    ShadowMap::~ShadowMap()
    {
        Destroy();
    }

    void ShadowMap::Destroy() noexcept
    {
        if (m_descriptorManager)
        {
            for (u32 i = 0; i < CascadeCount; ++i)
            {
                if (m_cascadeSrvIndices[i] != DescriptorManager::InvalidIndex)
                {
                    m_descriptorManager->Free(m_cascadeSrvIndices[i]);
                    m_cascadeSrvIndices[i] = DescriptorManager::InvalidIndex;
                }
            }
            if (m_pointSrvIndex != DescriptorManager::InvalidIndex)
            {
                m_descriptorManager->Free(m_pointSrvIndex);
                m_pointSrvIndex = DescriptorManager::InvalidIndex;
            }
            m_descriptorManager = nullptr;
        }

        for (u32 i = 0; i < CascadeCount; ++i)
        {
            m_cascadeShadowMaps[i].Reset();
        }
        m_pointShadowMap.Reset();
        m_dsvHeap.Reset();
    }

    std::expected<void, std::string> ShadowMap::Initialize(ID3D12Device* device, DescriptorManager* descriptorManager)
    {
        if (!device || !descriptorManager)
        {
            return std::unexpected("Invalid device or descriptor manager pointer passed to ShadowMap::Initialize.");
        }

        Destroy();
        m_descriptorManager = descriptorManager;

        m_dirViewport = { 0.0f, 0.0f, static_cast<f32>(DirectionalResolution), static_cast<f32>(DirectionalResolution), 0.0f, 1.0f };
        m_dirScissorRect = { 0, 0, static_cast<LONG>(DirectionalResolution), static_cast<LONG>(DirectionalResolution) };

        m_pointViewport = { 0.0f, 0.0f, static_cast<f32>(PointResolution), static_cast<f32>(PointResolution), 0.0f, 1.0f };
        m_pointScissorRect = { 0, 0, static_cast<LONG>(PointResolution), static_cast<LONG>(PointResolution) };

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.NumDescriptors = CascadeCount + 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        RL_CHECK_HR(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        const u32 dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        for (u32 i = 0; i < CascadeCount; ++i)
        {
            m_cascadeDsvHandles[i] = OffsetCPUDescriptorHandle(heapStart, i, dsvDescriptorSize);
        }
        m_pointDsvHandle = OffsetCPUDescriptorHandle(heapStart, CascadeCount, dsvDescriptorSize);

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_CLEAR_VALUE optClear{};
        optClear.Format = DXGI_FORMAT_D32_FLOAT;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;

        D3D12_RESOURCE_DESC dirTexDesc{};
        dirTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dirTexDesc.Alignment = 0;
        dirTexDesc.Width = DirectionalResolution;
        dirTexDesc.Height = DirectionalResolution;
        dirTexDesc.DepthOrArraySize = 1;
        dirTexDesc.MipLevels = 1;
        dirTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        dirTexDesc.SampleDesc.Count = 1;
        dirTexDesc.SampleDesc.Quality = 0;
        dirTexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        dirTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        srvDesc.Texture2D.PlaneSlice = 0;

        for (u32 i = 0; i < CascadeCount; ++i)
        {
            RL_CHECK_HR(device->CreateCommittedResource(
                &defaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &dirTexDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &optClear,
                IID_PPV_ARGS(&m_cascadeShadowMaps[i])
            ));
            m_cascadeStates[i] = D3D12_RESOURCE_STATE_DEPTH_WRITE;

            device->CreateDepthStencilView(m_cascadeShadowMaps[i].Get(), &dsvDesc, m_cascadeDsvHandles[i]);

            m_cascadeSrvIndices[i] = m_descriptorManager->Allocate();
            if (m_cascadeSrvIndices[i] == DescriptorManager::InvalidIndex)
            {
                return std::unexpected("Failed to allocate descriptor slot for cascade shadow map SRV.");
            }

            device->CreateShaderResourceView(m_cascadeShadowMaps[i].Get(), &srvDesc, m_descriptorManager->GetCPUHandle(m_cascadeSrvIndices[i]));
        }

        D3D12_RESOURCE_DESC pointTexDesc = dirTexDesc;
        pointTexDesc.Width = PointResolution;
        pointTexDesc.Height = PointResolution;

        RL_CHECK_HR(device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &pointTexDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optClear,
            IID_PPV_ARGS(&m_pointShadowMap)
        ));
        m_pointState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        device->CreateDepthStencilView(m_pointShadowMap.Get(), &dsvDesc, m_pointDsvHandle);

        m_pointSrvIndex = m_descriptorManager->Allocate();
        if (m_pointSrvIndex == DescriptorManager::InvalidIndex)
        {
            return std::unexpected("Failed to allocate descriptor slot for point shadow map SRV.");
        }

        device->CreateShaderResourceView(m_pointShadowMap.Get(), &srvDesc, m_descriptorManager->GetCPUHandle(m_pointSrvIndex));

        RL_LOG_INFO("ShadowMap subsystem initialized: 4 Cascades [{}x{} SRVs: {}, {}, {}, {}], Point [{}x{} SRV: {}]",
            DirectionalResolution, DirectionalResolution,
            m_cascadeSrvIndices[0], m_cascadeSrvIndices[1], m_cascadeSrvIndices[2], m_cascadeSrvIndices[3],
            PointResolution, PointResolution, m_pointSrvIndex);

        return {};
    }

    void ShadowMap::BeginCascade(ID3D12GraphicsCommandList* cmdList, u32 cascadeIndex)
    {
        if (!cmdList || cascadeIndex >= CascadeCount || !m_cascadeShadowMaps[cascadeIndex]) return;

        if (m_cascadeStates[cascadeIndex] != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        {
            const auto barrier = CreateTransitionBarrier(m_cascadeShadowMaps[cascadeIndex].Get(), m_cascadeStates[cascadeIndex], D3D12_RESOURCE_STATE_DEPTH_WRITE);
            cmdList->ResourceBarrier(1, &barrier);
            m_cascadeStates[cascadeIndex] = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }

        cmdList->RSSetViewports(1, &m_dirViewport);
        cmdList->RSSetScissorRects(1, &m_dirScissorRect);
        cmdList->OMSetRenderTargets(0, nullptr, FALSE, &m_cascadeDsvHandles[cascadeIndex]);
        cmdList->ClearDepthStencilView(m_cascadeDsvHandles[cascadeIndex], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    void ShadowMap::EndCascade(ID3D12GraphicsCommandList* cmdList, u32 cascadeIndex)
    {
        if (!cmdList || cascadeIndex >= CascadeCount || !m_cascadeShadowMaps[cascadeIndex]) return;

        if (m_cascadeStates[cascadeIndex] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            const auto barrier = CreateTransitionBarrier(m_cascadeShadowMaps[cascadeIndex].Get(), m_cascadeStates[cascadeIndex], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmdList->ResourceBarrier(1, &barrier);
            m_cascadeStates[cascadeIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }

    void ShadowMap::BeginDirectional(ID3D12GraphicsCommandList* cmdList)
    {
        BeginCascade(cmdList, 0);
    }

    void ShadowMap::EndDirectional(ID3D12GraphicsCommandList* cmdList)
    {
        EndCascade(cmdList, 0);
    }

    void ShadowMap::BeginPoint(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList || !m_pointShadowMap) return;

        if (m_pointState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        {
            const auto barrier = CreateTransitionBarrier(m_pointShadowMap.Get(), m_pointState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            cmdList->ResourceBarrier(1, &barrier);
            m_pointState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }

        cmdList->RSSetViewports(1, &m_pointViewport);
        cmdList->RSSetScissorRects(1, &m_pointScissorRect);
        cmdList->OMSetRenderTargets(0, nullptr, FALSE, &m_pointDsvHandle);
        cmdList->ClearDepthStencilView(m_pointDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    void ShadowMap::EndPoint(ID3D12GraphicsCommandList* cmdList)
    {
        if (!cmdList || !m_pointShadowMap) return;

        if (m_pointState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        {
            const auto barrier = CreateTransitionBarrier(m_pointShadowMap.Get(), m_pointState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmdList->ResourceBarrier(1, &barrier);
            m_pointState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }

    u32 ShadowMap::GetCascadeSrvIndex(u32 cascadeIndex) const noexcept
    {
        if (cascadeIndex < CascadeCount)
        {
            return m_cascadeSrvIndices[cascadeIndex];
        }
        return DescriptorManager::InvalidIndex;
    }
}
