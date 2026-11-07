#pragma once

#include "Core/Types.hpp"
#include "Graphics/RHI/RHICommon.hpp"
#include "Graphics/RHI/RHIInterfaces.hpp"

#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <string>
#include <string_view>
#include <expected>
#include <memory>

namespace RL
{
    using Microsoft::WRL::ComPtr;
    class Scene;

    class DX11CommandBuffer final : public RHI::IRHICommandBuffer
    {
    public:
        explicit DX11CommandBuffer(ID3D11DeviceContext* context) : m_context(context) {}

        void SetContext(ID3D11DeviceContext* context) noexcept { m_context = context; }

        void SetViewport(f32 x, f32 y, f32 width, f32 height) override;
        void SetScissorRect(i32 left, i32 top, i32 right, i32 bottom) override;
        void SetPipelineState(RHI::IRHIPipelineState* pipeline) override;
        void SetConstantBuffer(u32 slot, RHI::IRHIBuffer* buffer) override;
        void SetVertexBuffer(RHI::IRHIBuffer* buffer, u32 stride, u32 offset = 0) override;
        void SetIndexBuffer(RHI::IRHIBuffer* buffer, u32 offset = 0) override;
        void DrawIndexed(u32 indexCount, u32 startIndex = 0, i32 baseVertex = 0) override;
        void DrawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndex = 0, i32 baseVertex = 0, u32 startInstance = 0) override;

    private:
        ID3D11DeviceContext* m_context = nullptr;
    };

    class DX11Device final : public RHI::IRHIDevice
    {
    public:
        DX11Device();
        ~DX11Device() override;

        DX11Device(const DX11Device&) = delete;
        DX11Device& operator=(const DX11Device&) = delete;
        DX11Device(DX11Device&&) noexcept = default;
        DX11Device& operator=(DX11Device&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(HWND hwnd, u32 width, u32 height);
        void Shutdown() noexcept;

        [[nodiscard]] RHI::GraphicsAPI GetAPI() const noexcept override { return RHI::GraphicsAPI::DirectX11; }
        [[nodiscard]] std::string_view GetGpuDescription() const noexcept override { return m_gpuDescription; }
        [[nodiscard]] u64 GetDedicatedVramBytes() const noexcept override { return m_dedicatedVramBytes; }
        [[nodiscard]] bool IsInitialized() const noexcept override { return m_device != nullptr; }

        void Resize(u32 width, u32 height) override;
        void BeginFrame() override;
        void ClearRenderTarget(f32 r = 0.04f, f32 g = 0.06f, f32 b = 0.09f, f32 a = 1.0f) override;
        void EndFrame() override;
        void Present(bool vsync = true) override;
        void WaitForGPU() override;

        [[nodiscard]] RHI::IRHICommandBuffer* GetActiveCommandBuffer() noexcept override { return m_commandBuffer.get(); }

        void RenderScene(Scene& scene);

        void SetShowGrid(bool show) noexcept { m_showGrid = show; }
        [[nodiscard]] bool IsGridVisible() const noexcept { return m_showGrid; }

        [[nodiscard]] ID3D11Device*        GetDevice()        const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D11DeviceContext* GetContext()       const noexcept { return m_context.Get(); }
        [[nodiscard]] IDXGISwapChain*      GetSwapChain()     const noexcept { return m_swapChain.Get(); }
        [[nodiscard]] ID3D11RenderTargetView* GetRTV()        const noexcept { return m_rtv.Get(); }
        [[nodiscard]] ID3D11DepthStencilView* GetDSV()        const noexcept { return m_dsv.Get(); }
        [[nodiscard]] D3D_FEATURE_LEVEL GetFeatureLevel() const noexcept { return m_featureLevel; }
        [[nodiscard]] ID3D11Buffer* GetGridVertexBuffer() const noexcept { return m_gridVertexBuffer.Get(); }
        [[nodiscard]] u32 GetGridVertexCount() const noexcept { return m_gridVertexCount; }

    private:
        std::expected<void, std::string> CreateDeviceAndSwapChain(HWND hwnd, u32 width, u32 height);
        std::expected<void, std::string> CreateRenderTargetAndDepthViews();
        std::expected<void, std::string> InitBasicShaders();

    private:
        ComPtr<ID3D11Device>           m_device;
        ComPtr<ID3D11DeviceContext>    m_context;
        ComPtr<IDXGISwapChain>         m_swapChain;
        ComPtr<ID3D11RenderTargetView> m_rtv;
        ComPtr<ID3D11Texture2D>        m_depthBuffer;
        ComPtr<ID3D11DepthStencilView> m_dsv;
        ComPtr<ID3D11DepthStencilState> m_depthState;
        ComPtr<ID3D11RasterizerState>  m_rasterState;

        ComPtr<ID3D11VertexShader>     m_vs;
        ComPtr<ID3D11PixelShader>      m_ps;
        ComPtr<ID3D11InputLayout>      m_inputLayout;
        ComPtr<ID3D11Buffer>           m_constantBuffer;
        ComPtr<ID3D11Buffer>           m_cubeVertexBuffer;
        ComPtr<ID3D11Buffer>           m_cubeIndexBuffer;

        ComPtr<ID3D11VertexShader>     m_gridVS;
        ComPtr<ID3D11PixelShader>      m_gridPS;
        ComPtr<ID3D11InputLayout>      m_gridInputLayout;
        ComPtr<ID3D11Buffer>           m_gridVertexBuffer;
        u32                            m_gridVertexCount = 0;
        bool                           m_showGrid = true;

        std::unique_ptr<DX11CommandBuffer> m_commandBuffer;

        D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;
        std::string m_gpuDescription;
        u64 m_dedicatedVramBytes = 0;
        u32 m_width = 0;
        u32 m_height = 0;
    };
}
