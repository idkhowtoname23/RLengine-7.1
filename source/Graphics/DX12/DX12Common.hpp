#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"
#include "Core/HResult.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace RL
{
    using Microsoft::WRL::ComPtr;

    inline constexpr u32 NUM_FRAMES = 3;
    inline constexpr u32 FrameBufferCount = NUM_FRAMES;

    struct FrameContext
    {
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        u64 fenceValue = 0;
    };

    [[nodiscard]] inline D3D12_RESOURCE_BARRIER CreateTransitionBarrier(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter) noexcept
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return barrier;
    }

    [[nodiscard]] inline D3D12_CPU_DESCRIPTOR_HANDLE OffsetCPUDescriptorHandle(
        D3D12_CPU_DESCRIPTOR_HANDLE baseHandle,
        u32 index,
        u32 descriptorSize) noexcept
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = baseHandle;
        handle.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(descriptorSize);
        return handle;
    }
}
