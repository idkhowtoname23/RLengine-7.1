#pragma once

#include "Graphics/DX12/DX12Common.hpp"

#include <expected>
#include <string>
#include <vector>

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;

namespace RL
{
    class DescriptorManager
    {
    public:
        static constexpr u32 DefaultCapacity = 1024;
        static constexpr u32 InvalidIndex = 0xFFFFFFFF;

        DescriptorManager() = default;
        ~DescriptorManager();

        DescriptorManager(const DescriptorManager&) = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;

        DescriptorManager(DescriptorManager&&) noexcept = default;
        DescriptorManager& operator=(DescriptorManager&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(ID3D12Device* device, u32 capacity = DefaultCapacity);
        void Shutdown() noexcept;

        [[nodiscard]] u32 Allocate();
        void Free(u32 index);

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(u32 index) const noexcept;
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(u32 index) const noexcept;

        void Bind(ID3D12GraphicsCommandList* cmdList) const noexcept;

        void SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* cmdList, u32 rootParamIndex, u32 descriptorIndex) const noexcept;

        [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const noexcept { return m_heap.Get(); }
        [[nodiscard]] u32 GetDescriptorSize() const noexcept { return m_descriptorSize; }
        [[nodiscard]] u32 GetCapacity() const noexcept { return m_capacity; }
        [[nodiscard]] u32 GetAllocatedCount() const noexcept { return m_allocatedCount; }

    private:
        ComPtr<ID3D12DescriptorHeap> m_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStartHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStartHandle{};
        u32 m_descriptorSize = 0;
        u32 m_capacity = 0;
        u32 m_allocatedCount = 0;
        std::vector<u32> m_freeList;
        std::vector<bool> m_allocatedSlots;
    };
}
