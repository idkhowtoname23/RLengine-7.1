#include "Graphics/DX11/DX11Device.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"
#include "Core/Logger.hpp"
#include "Core/HResult.hpp"

#include <d3dcompiler.h>
#include <format>

namespace RL
{
    struct DX11Vertex
    {
        float p[3];
        float n[3];
        float uv[2];
        float c[4];
    };

    void DX11CommandBuffer::SetViewport(f32 x, f32 y, f32 width, f32 height)
    {
        if (!m_context) return;
        D3D11_VIEWPORT vp{};
        vp.TopLeftX = x;
        vp.TopLeftY = y;
        vp.Width = width;
        vp.Height = height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);
    }

    void DX11CommandBuffer::SetScissorRect(i32 left, i32 top, i32 right, i32 bottom)
    {
        if (!m_context) return;
        D3D11_RECT rect{ left, top, right, bottom };
        m_context->RSSetScissorRects(1, &rect);
    }

    void DX11CommandBuffer::SetPipelineState(RHI::IRHIPipelineState*)
    {
    }

    void DX11CommandBuffer::SetConstantBuffer(u32, RHI::IRHIBuffer*)
    {
    }

    void DX11CommandBuffer::SetVertexBuffer(RHI::IRHIBuffer*, u32, u32)
    {
    }

    void DX11CommandBuffer::SetIndexBuffer(RHI::IRHIBuffer*, u32)
    {
    }

    void DX11CommandBuffer::DrawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex)
    {
        if (!m_context) return;
        m_context->DrawIndexed(indexCount, startIndex, baseVertex);
    }

    void DX11CommandBuffer::DrawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndex, i32 baseVertex, u32 startInstance)
    {
        if (!m_context) return;
        m_context->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
    }

    DX11Device::DX11Device()
    {
        m_commandBuffer = std::make_unique<DX11CommandBuffer>(nullptr);
    }

    DX11Device::~DX11Device()
    {
        Shutdown();
    }

    std::expected<void, std::string> DX11Device::Initialize(HWND hwnd, u32 width, u32 height)
    {
        m_width = width;
        m_height = height;

        RL_LOG_INFO("Initializing DirectX 11 graphics subsystem (Legacy & Multi-backend support)...");

        auto devRes = CreateDeviceAndSwapChain(hwnd, width, height);
        if (!devRes) return devRes;

        if (m_commandBuffer)
        {
            m_commandBuffer->SetContext(m_context.Get());
        }

        auto rtvRes = CreateRenderTargetAndDepthViews();
        if (!rtvRes) return rtvRes;

        auto shaderRes = InitBasicShaders();
        if (!shaderRes)
        {
            RL_LOG_WARN("DX11 basic shaders initialization warning: {}", shaderRes.error());
        }

        RL_LOG_INFO("DirectX 11 subsystem initialized successfully on '{}' [Feature Level 11_0+, VRAM: {:.2f} GB]",
                    m_gpuDescription,
                    static_cast<f64>(m_dedicatedVramBytes) / (1024.0 * 1024.0 * 1024.0));
        return {};
    }

    void DX11Device::Shutdown() noexcept
    {
        if (m_context)
        {
            m_context->ClearState();
        }

        m_gridInputLayout.Reset();
        m_gridPS.Reset();
        m_gridVS.Reset();
        m_gridVertexBuffer.Reset();
        m_gridVertexCount = 0;

        m_cubeIndexBuffer.Reset();
        m_cubeVertexBuffer.Reset();
        m_constantBuffer.Reset();
        m_inputLayout.Reset();
        m_ps.Reset();
        m_vs.Reset();

        m_rasterState.Reset();
        m_depthState.Reset();
        m_dsv.Reset();
        m_depthBuffer.Reset();
        m_rtv.Reset();
        m_swapChain.Reset();
        m_context.Reset();
        m_device.Reset();

        RL_LOG_INFO("DirectX 11 subsystem shut down cleanly.");
    }

    void DX11Device::Resize(u32 width, u32 height)
    {
        if (!m_swapChain || width == 0 || height == 0) return;

        m_width = width;
        m_height = height;

        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_rtv.Reset();
        m_depthBuffer.Reset();
        m_dsv.Reset();

        HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            RL_LOG_ERROR("DX11Device: failed to resize swap chain buffers. HRESULT: 0x{:08X}", static_cast<u32>(hr));
            return;
        }

        auto rtvRes = CreateRenderTargetAndDepthViews();
        if (!rtvRes)
        {
            RL_LOG_ERROR("DX11Device: failed to recreate views after resize: {}", rtvRes.error());
        }
    }

    void DX11Device::BeginFrame()
    {
        if (!m_context || !m_rtv) return;

        ID3D11RenderTargetView* rtvs[] = { m_rtv.Get() };
        m_context->OMSetRenderTargets(1, rtvs, m_dsv.Get());
        if (m_depthState)
        {
            m_context->OMSetDepthStencilState(m_depthState.Get(), 1);
        }
        if (m_rasterState)
        {
            m_context->RSSetState(m_rasterState.Get());
        }

        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<f32>(m_width);
        vp.Height = static_cast<f32>(m_height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        m_context->RSSetViewports(1, &vp);
    }

    void DX11Device::ClearRenderTarget(f32 r, f32 g, f32 b, f32 a)
    {
        if (!m_context || !m_rtv) return;
        const float clearColor[4] = { r, g, b, a };
        m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
        if (m_dsv)
        {
            m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        }
    }

    void DX11Device::EndFrame()
    {
    }

    void DX11Device::Present(bool vsync)
    {
        if (!m_swapChain) return;
        m_swapChain->Present(vsync ? 1 : 0, 0);
    }

    void DX11Device::WaitForGPU()
    {
        if (m_context)
        {
            m_context->Flush();
        }
    }

    std::expected<void, std::string> DX11Device::CreateDeviceAndSwapChain(HWND hwnd, u32 width, u32 height)
    {
        DXGI_SWAP_CHAIN_DESC scd{};
        scd.BufferCount = 2;
        scd.BufferDesc.Width = width;
        scd.BufferDesc.Height = height;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hwnd;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
#if defined(_DEBUG)
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &scd,
            &m_swapChain,
            &m_device,
            &m_featureLevel,
            &m_context
        );

        if (FAILED(hr))
        {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                createDeviceFlags,
                featureLevels,
                static_cast<UINT>(std::size(featureLevels)),
                D3D11_SDK_VERSION,
                &scd,
                &m_swapChain,
                &m_device,
                &m_featureLevel,
                &m_context
            );
        }

        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create DX11 Device and SwapChain: 0x{:08X}", static_cast<u32>(hr)));
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(m_device.As(&dxgiDevice)))
        {
            ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
            {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc)))
                {
                    char descStr[128];
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, descStr, sizeof(descStr), nullptr, nullptr);
                    m_gpuDescription = descStr;
                    m_dedicatedVramBytes = desc.DedicatedVideoMemory;
                }
            }
        }

        return {};
    }

    std::expected<void, std::string> DX11Device::CreateRenderTargetAndDepthViews()
    {
        ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to get back buffer from DX11 swap chain: 0x{:08X}", static_cast<u32>(hr)));
        }

        hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv);
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create DX11 RenderTargetView: 0x{:08X}", static_cast<u32>(hr)));
        }

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = m_width;
        depthDesc.Height = m_height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthBuffer);
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create DX11 Depth Buffer: 0x{:08X}", static_cast<u32>(hr)));
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = depthDesc.Format;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

        hr = m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, &m_dsv);
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create DX11 DepthStencilView: 0x{:08X}", static_cast<u32>(hr)));
        }

        D3D11_DEPTH_STENCIL_DESC dsDesc{};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

        hr = m_device->CreateDepthStencilState(&dsDesc, &m_depthState);
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create DX11 DepthStencilState: 0x{:08X}", static_cast<u32>(hr)));
        }

        D3D11_RASTERIZER_DESC rastDesc{};
        rastDesc.FillMode = D3D11_FILL_SOLID;
        rastDesc.CullMode = D3D11_CULL_NONE;
        rastDesc.FrontCounterClockwise = FALSE;
        rastDesc.DepthClipEnable = TRUE;

        hr = m_device->CreateRasterizerState(&rastDesc, &m_rasterState);
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create DX11 RasterizerState: 0x{:08X}", static_cast<u32>(hr)));
        }

        return {};
    }

    std::expected<void, std::string> DX11Device::InitBasicShaders()
    {
        const char* shaderSource = R"(
            cbuffer CBuf : register(b0)
            {
                row_major float4x4 WorldViewProj;
                row_major float4x4 World;
                float4 ObjectColor;
                float4 LightDir;
                float4 LightColor;
                float4 AmbientColor;
            };

            struct VSIn
            {
                float3 Pos : POSITION;
                float3 Normal : NORMAL;
                float2 UV : TEXCOORD;
                float4 Color : COLOR;
            };

            struct PSIn
            {
                float4 Pos : SV_Position;
                float4 Color : COLOR;
                float3 Normal : NORMAL;
            };

            PSIn VSMain(VSIn input)
            {
                PSIn output;
                output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProj);
                output.Color = input.Color * ObjectColor;
                output.Normal = mul(input.Normal, (float3x3)World);
                return output;
            }

            float4 PSMain(PSIn input) : SV_Target
            {
                float3 N = normalize(input.Normal);
                float3 L = normalize(-LightDir.xyz);
                float NdotL = max(dot(N, L), 0.0f);
                float3 lit = input.Color.rgb * (AmbientColor.rgb + LightColor.rgb * NdotL);
                return float4(lit, input.Color.a);
            }

            struct GridVSIn
            {
                float3 Pos : POSITION;
                float3 Normal : NORMAL;
                float2 UV : TEXCOORD;
                float4 Color : COLOR;
            };

            struct GridPSIn
            {
                float4 Pos : SV_Position;
                float4 Color : COLOR;
            };

            GridPSIn GridVSMain(GridVSIn input)
            {
                GridPSIn output;
                output.Pos = mul(float4(input.Pos, 1.0f), WorldViewProj);
                output.Color = input.Color;
                return output;
            }

            float4 GridPSMain(GridPSIn input) : SV_Target
            {
                return input.Color;
            }
        )";

        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> psBlob;
        ComPtr<ID3DBlob> errorBlob;

        HRESULT hr = D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr))
        {
            return std::unexpected("DX11 VS Compile failed");
        }

        hr = D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr))
        {
            return std::unexpected("DX11 PS Compile failed");
        }

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs);
        if (FAILED(hr)) return std::unexpected("CreateVertexShader failed");

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps);
        if (FAILED(hr)) return std::unexpected("CreatePixelShader failed");

        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = m_device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);
        if (FAILED(hr)) return std::unexpected("CreateInputLayout failed");

        ComPtr<ID3DBlob> gridVsBlob;
        ComPtr<ID3DBlob> gridPsBlob;

        hr = D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "GridVSMain", "vs_5_0", 0, 0, &gridVsBlob, &errorBlob);
        if (FAILED(hr))
        {
            return std::unexpected("DX11 Grid VS Compile failed");
        }

        hr = D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "GridPSMain", "ps_5_0", 0, 0, &gridPsBlob, &errorBlob);
        if (FAILED(hr))
        {
            return std::unexpected("DX11 Grid PS Compile failed");
        }

        hr = m_device->CreateVertexShader(gridVsBlob->GetBufferPointer(), gridVsBlob->GetBufferSize(), nullptr, &m_gridVS);
        if (FAILED(hr)) return std::unexpected("CreateVertexShader for grid failed");

        hr = m_device->CreatePixelShader(gridPsBlob->GetBufferPointer(), gridPsBlob->GetBufferSize(), nullptr, &m_gridPS);
        if (FAILED(hr)) return std::unexpected("CreatePixelShader for grid failed");

        hr = m_device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), gridVsBlob->GetBufferPointer(), gridVsBlob->GetBufferSize(), &m_gridInputLayout);
        if (FAILED(hr)) return std::unexpected("CreateInputLayout for grid failed");

        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.ByteWidth = 256;
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
        if (FAILED(hr)) return std::unexpected("CreateBuffer constant buffer failed");

        const f32 h = 0.5f;
        const DX11Vertex cubeVertices[24] = {
            { { -h, -h, -h }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h, -h }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h, -h }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h, -h }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { {  h, -h,  h }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h,  h }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h,  h }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h,  h }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { { -h,  h, -h }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h,  h }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h,  h }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h, -h }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { { -h, -h,  h }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h, -h }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h, -h }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h,  h }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { { -h,  h,  h }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h,  h, -h }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h, -h }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { -h, -h,  h }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

            { {  h, -h, -h }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h, -h }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h,  h,  h }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
            { {  h, -h,  h }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
        };

        const u32 cubeIndices[36] = {
             0,  1,  2,  0,  2,  3,
             4,  5,  6,  4,  6,  7,
             8,  9, 10,  8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };

        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = sizeof(cubeVertices);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vbInit{ cubeVertices, 0, 0 };
        hr = m_device->CreateBuffer(&vbDesc, &vbInit, &m_cubeVertexBuffer);
        if (FAILED(hr)) return std::unexpected("CreateBuffer cube vertex buffer failed");

        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = sizeof(cubeIndices);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA ibInit{ cubeIndices, 0, 0 };
        hr = m_device->CreateBuffer(&ibDesc, &ibInit, &m_cubeIndexBuffer);
        if (FAILED(hr)) return std::unexpected("CreateBuffer cube index buffer failed");

        struct Color4 { float r, g, b, a; };
        std::vector<DX11Vertex> gridVertices;
        const f32 halfSize = 20.0f;
        const u32 lines = 40;
        const f32 step = (halfSize * 2.0f) / static_cast<f32>(lines);
        gridVertices.reserve((lines + 1) * 4);

        for (u32 i = 0; i <= lines; ++i)
        {
            const f32 pos = -halfSize + static_cast<f32>(i) * step;
            const bool isCenter = (std::abs(pos) < 0.001f);

            const Color4 colorZ = isCenter
                ? Color4{ 0.25f, 0.5f, 1.0f, 1.0f }
                : Color4{ 0.35f, 0.35f, 0.35f, 1.0f };

            gridVertices.push_back({ { pos, 0.0f, -halfSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { colorZ.r, colorZ.g, colorZ.b, colorZ.a } });
            gridVertices.push_back({ { pos, 0.0f,  halfSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { colorZ.r, colorZ.g, colorZ.b, colorZ.a } });

            const Color4 colorX = isCenter
                ? Color4{ 1.0f, 0.25f, 0.25f, 1.0f }
                : Color4{ 0.35f, 0.35f, 0.35f, 1.0f };

            gridVertices.push_back({ { -halfSize, 0.0f, pos }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { colorX.r, colorX.g, colorX.b, colorX.a } });
            gridVertices.push_back({ {  halfSize, 0.0f, pos }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { colorX.r, colorX.g, colorX.b, colorX.a } });
        }

        m_gridVertexCount = static_cast<u32>(gridVertices.size());

        D3D11_BUFFER_DESC gvbDesc{};
        gvbDesc.ByteWidth = static_cast<UINT>(gridVertices.size() * sizeof(DX11Vertex));
        gvbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        gvbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA gvbInit{ gridVertices.data(), 0, 0 };
        hr = m_device->CreateBuffer(&gvbDesc, &gvbInit, &m_gridVertexBuffer);
        if (FAILED(hr)) return std::unexpected("CreateBuffer grid vertex buffer failed");

        return {};
    }

    void DX11Device::RenderScene(Scene& scene)
    {
        if (!m_context || !m_vs || !m_ps || !m_cubeVertexBuffer) return;

        DirectX::XMMATRIX viewProj = scene.GetCamera().GetViewProjectionMatrix();

        const auto* transformPool = scene.GetPool<TransformComponent>();
        const auto* meshPool = scene.GetPool<MeshComponent>();
        if (!transformPool) return;

        DirectX::XMFLOAT4 lightDir{ 0.5f, -0.8f, 0.7f, 0.0f };
        DirectX::XMFLOAT4 lightColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 ambientColor{ 0.15f, 0.18f, 0.22f, 1.0f };

        const auto* lightPool = scene.GetPool<LightComponent>();
        if (lightPool)
        {
            const auto lightEntities = lightPool->GetEntities();
            const auto lights = lightPool->GetData();
            for (usize i = 0; i < lights.size(); ++i)
            {
                if (lights[i].Type == LightType::Directional)
                {
                    const auto* tr = transformPool->TryGet(lightEntities[i]);
                    if (tr)
                    {
                        if (tr->Position.x != 0.0f || tr->Position.y != 0.0f || tr->Position.z != 0.0f)
                        {
                            lightDir = { tr->Position.x, tr->Position.y, tr->Position.z, 0.0f };
                        }
                    }
                    lightColor = { lights[i].Color.x * lights[i].Intensity,
                                   lights[i].Color.y * lights[i].Intensity,
                                   lights[i].Color.z * lights[i].Intensity,
                                   1.0f };
                    break;
                }
            }
        }

        struct CBLayout
        {
            DirectX::XMFLOAT4X4 WorldViewProj;
            DirectX::XMFLOAT4X4 World;
            DirectX::XMFLOAT4 ObjectColor;
            DirectX::XMFLOAT4 LightDir;
            DirectX::XMFLOAT4 LightColor;
            DirectX::XMFLOAT4 AmbientColor;
        };

        if (m_showGrid && m_gridVertexBuffer && m_gridVertexCount > 0 && m_gridVS && m_gridPS && m_gridInputLayout)
        {
            DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX gridWvp = identity * viewProj;

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                auto* cb = static_cast<CBLayout*>(mapped.pData);
                DirectX::XMStoreFloat4x4(&cb->WorldViewProj, gridWvp);
                DirectX::XMStoreFloat4x4(&cb->World, identity);
                cb->ObjectColor = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                cb->LightDir = lightDir;
                cb->LightColor = lightColor;
                cb->AmbientColor = ambientColor;
                m_context->Unmap(m_constantBuffer.Get(), 0);
            }

            m_context->IASetInputLayout(m_gridInputLayout.Get());
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

            const UINT gridStride = sizeof(DX11Vertex);
            const UINT gridOffset = 0;
            ID3D11Buffer* gridVbs[] = { m_gridVertexBuffer.Get() };
            m_context->IASetVertexBuffers(0, 1, gridVbs, &gridStride, &gridOffset);
            m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

            m_context->VSSetShader(m_gridVS.Get(), nullptr, 0);
            m_context->PSSetShader(m_gridPS.Get(), nullptr, 0);

            ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
            m_context->VSSetConstantBuffers(0, 1, cbs);
            m_context->PSSetConstantBuffers(0, 1, cbs);

            m_context->Draw(m_gridVertexCount, 0);
        }

        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const UINT stride = 48;
        const UINT offset = 0;
        ID3D11Buffer* vbs[] = { m_cubeVertexBuffer.Get() };
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        m_context->IASetIndexBuffer(m_cubeIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        m_context->VSSetShader(m_vs.Get(), nullptr, 0);
        m_context->PSSetShader(m_ps.Get(), nullptr, 0);

        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        m_context->VSSetConstantBuffers(0, 1, cbs);
        m_context->PSSetConstantBuffers(0, 1, cbs);

        if (meshPool)
        {
            const auto entities = meshPool->GetEntities();
            const auto meshes = meshPool->GetData();

            for (usize i = 0; i < meshes.size(); ++i)
            {
                if (!meshes[i].IsVisible) continue;
                Entity entity = scene.GetEntityFromIndex(entities[i]);
                if (!scene.IsValid(entity)) continue;

                DirectX::XMMATRIX world = scene.GetWorldMatrix(entity);
                DirectX::XMMATRIX wvp = world * viewProj;

                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                    auto* cb = static_cast<CBLayout*>(mapped.pData);
                    DirectX::XMStoreFloat4x4(&cb->WorldViewProj, wvp);
                    DirectX::XMStoreFloat4x4(&cb->World, world);
                    cb->ObjectColor = meshes[i].Color;
                    cb->LightDir = lightDir;
                    cb->LightColor = lightColor;
                    cb->AmbientColor = ambientColor;
                    m_context->Unmap(m_constantBuffer.Get(), 0);
                }

                m_context->DrawIndexed(36, 0, 0);
            }
        }
    }
}
