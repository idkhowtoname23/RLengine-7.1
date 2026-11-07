#pragma once

#include "Core/Types.hpp"
#include "Graphics/RHI/RHICommon.hpp"

#include <string_view>
#include <memory>

namespace RL::RHI
{
    class IRHIBuffer
    {
    public:
        virtual ~IRHIBuffer() = default;
        virtual void UpdateData(const void* data, usize size) = 0;
        [[nodiscard]] virtual usize GetSize() const noexcept = 0;
    };

    class IRHIPipelineState
    {
    public:
        virtual ~IRHIPipelineState() = default;
        [[nodiscard]] virtual bool IsValid() const noexcept = 0;
    };

    class IRHITexture
    {
    public:
        virtual ~IRHITexture() = default;
        [[nodiscard]] virtual u32 GetWidth() const noexcept = 0;
        [[nodiscard]] virtual u32 GetHeight() const noexcept = 0;
    };

    class IRHICommandBuffer
    {
    public:
        virtual ~IRHICommandBuffer() = default;

        virtual void SetViewport(f32 x, f32 y, f32 width, f32 height) = 0;
        virtual void SetScissorRect(i32 left, i32 top, i32 right, i32 bottom) = 0;
        virtual void SetPipelineState(IRHIPipelineState* pipeline) = 0;
        virtual void SetConstantBuffer(u32 slot, IRHIBuffer* buffer) = 0;
        virtual void SetVertexBuffer(IRHIBuffer* buffer, u32 stride, u32 offset = 0) = 0;
        virtual void SetIndexBuffer(IRHIBuffer* buffer, u32 offset = 0) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 startIndex = 0, i32 baseVertex = 0) = 0;
        virtual void DrawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndex = 0, i32 baseVertex = 0, u32 startInstance = 0) = 0;
    };

    class IRHIDevice
    {
    public:
        virtual ~IRHIDevice() = default;

        [[nodiscard]] virtual GraphicsAPI GetAPI() const noexcept = 0;
        [[nodiscard]] virtual std::string_view GetGpuDescription() const noexcept = 0;
        [[nodiscard]] virtual u64 GetDedicatedVramBytes() const noexcept = 0;
        [[nodiscard]] virtual bool IsInitialized() const noexcept = 0;

        virtual void Resize(u32 width, u32 height) = 0;
        virtual void BeginFrame() = 0;
        virtual void ClearRenderTarget(f32 r = 0.04f, f32 g = 0.06f, f32 b = 0.09f, f32 a = 1.0f) = 0;
        virtual void EndFrame() = 0;
        virtual void Present(bool vsync = true) = 0;
        virtual void WaitForGPU() = 0;

        [[nodiscard]] virtual IRHICommandBuffer* GetActiveCommandBuffer() noexcept = 0;
    };
}
