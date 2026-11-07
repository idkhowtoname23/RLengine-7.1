#pragma once

#include "Graphics/DX12/DX12Common.hpp"
#include "Graphics/RHI/RHICommon.hpp"
#include "Graphics/RHI/RHIInterfaces.hpp"

#include <string>
#include <string_view>
#include <expected>
#include <functional>
#include <memory>

namespace RL
{
    class DX12CommandBuffer final : public RHI::IRHICommandBuffer
    {
    public:
        explicit DX12CommandBuffer(ID3D12GraphicsCommandList* cmdList = nullptr) : m_cmdList(cmdList) {}
        void SetCommandList(ID3D12GraphicsCommandList* cmdList) noexcept { m_cmdList = cmdList; }

        void SetViewport(f32 x, f32 y, f32 width, f32 height) override
        {
            if (!m_cmdList) return;
            D3D12_VIEWPORT vp{ x, y, width, height, 0.0f, 1.0f };
            m_cmdList->RSSetViewports(1, &vp);
        }

        void SetScissorRect(i32 left, i32 top, i32 right, i32 bottom) override
        {
            if (!m_cmdList) return;
            D3D12_RECT rect{ left, top, right, bottom };
            m_cmdList->RSSetScissorRects(1, &rect);
        }

        void SetPipelineState(RHI::IRHIPipelineState*) override {}
        void SetConstantBuffer(u32, RHI::IRHIBuffer*) override {}
        void SetVertexBuffer(RHI::IRHIBuffer*, u32, u32) override {}
        void SetIndexBuffer(RHI::IRHIBuffer*, u32) override {}

        void DrawIndexed(u32 indexCount, u32 startIndex, i32 baseVertex) override
        {
            if (!m_cmdList) return;
            m_cmdList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
        }

        void DrawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndex, i32 baseVertex, u32 startInstance) override
        {
            if (!m_cmdList) return;
            m_cmdList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndex, baseVertex, startInstance);
        }

    private:
        ID3D12GraphicsCommandList* m_cmdList = nullptr;
    };

    class DX12Device final : public RHI::IRHIDevice
    {
    public:
        DX12Device();
        ~DX12Device() override;

        DX12Device(const DX12Device&) = delete;
        DX12Device& operator=(const DX12Device&) = delete;

        DX12Device(DX12Device&&) noexcept = default;
        DX12Device& operator=(DX12Device&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(HWND hwnd, u32 width, u32 height);
        void Shutdown() noexcept;

        [[nodiscard]] RHI::GraphicsAPI GetAPI() const noexcept override { return RHI::GraphicsAPI::DirectX12; }
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

        void FlushGpu() { WaitForGPU(); }
        void MoveToNextFrame();

        void ExecuteSingleTimeCommands(std::function<void(ID3D12GraphicsCommandList*)> action);

        [[nodiscard]] ID3D12Device* GetDevice() const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D12CommandQueue* GetCommandQueue() const noexcept { return m_commandQueue.Get(); }
        [[nodiscard]] ID3D12GraphicsCommandList* GetCommandList() const noexcept { return m_commandList.Get(); }
        [[nodiscard]] IDXGISwapChain4* GetSwapChain() const noexcept { return m_swapChain.Get(); }
        [[nodiscard]] u32 GetCurrentFrameIndex() const noexcept { return m_frameIndex; }
        [[nodiscard]] const FrameContext& GetCurrentFrameContext() const noexcept { return m_frameContexts[m_frameIndex]; }
        [[nodiscard]] const RHI::GpuDeviceInfo& GetGpuInfo() const noexcept { return m_gpuInfo; }
        [[nodiscard]] ID3D12Resource* GetCurrentRenderTarget() const noexcept { return m_renderTargets[m_frameIndex].Get(); }
        [[nodiscard]] ID3D12Resource* GetDepthStencilBuffer() const noexcept { return m_depthStencilBuffer.Get(); }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const noexcept { return m_dsvHeap ? m_dsvHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{0}; }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const noexcept
        {
            return m_rtvHeap ? OffsetCPUDescriptorHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize) : D3D12_CPU_DESCRIPTOR_HANDLE{0};
        }
        void SetMainRenderTarget() noexcept
        {
            if (!m_commandList || !m_rtvHeap || !m_dsvHeap) return;
            const auto rtv = GetRTVHandle();
            const auto dsv = GetDSVHandle();
            m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        }

    private:
        [[nodiscard]] std::expected<void, std::string> CreateDeviceAndFactory();
        [[nodiscard]] std::expected<void, std::string> CreateCommandQueue();
        [[nodiscard]] std::expected<void, std::string> CreateSwapChain(HWND hwnd, u32 width, u32 height);
        [[nodiscard]] std::expected<void, std::string> CreateRtvDescriptorHeap();
        [[nodiscard]] std::expected<void, std::string> CreateDsvDescriptorHeap();
        [[nodiscard]] std::expected<void, std::string> CreateDepthStencilView();
        [[nodiscard]] std::expected<void, std::string> CreateFrameResources();
        [[nodiscard]] std::expected<void, std::string> CreateSyncObjects();

        void CreateRenderTargetViews();

    private:
        ComPtr<IDXGIFactory7> m_dxgiFactory;
        ComPtr<IDXGIAdapter4> m_dxgiAdapter;
        ComPtr<ID3D12Device>  m_device;
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        ComPtr<IDXGISwapChain4> m_swapChain;

        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        u32 m_rtvDescriptorSize = 0;

        ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
        u32 m_dsvDescriptorSize = 0;
        ComPtr<ID3D12Resource> m_depthStencilBuffer;

        ComPtr<ID3D12Resource> m_renderTargets[NUM_FRAMES];
        FrameContext m_frameContexts[NUM_FRAMES];
        ComPtr<ID3D12GraphicsCommandList> m_commandList;

        std::unique_ptr<DX12CommandBuffer> m_commandBuffer;

        ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent = nullptr;
        u64 m_currentFenceValue = 0;

        u32 m_frameIndex = 0;
        u32 m_width = 0;
        u32 m_height = 0;

        bool m_tearingSupported = false;
        bool m_isFrameActive = false;

        std::string m_gpuDescription;
        u64 m_dedicatedVramBytes = 0;
        RHI::GpuDeviceInfo m_gpuInfo;
    };
}
