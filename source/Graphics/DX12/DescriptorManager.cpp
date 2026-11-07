#include "Graphics/DX12/DescriptorManager.hpp"
#include "Core/Logger.hpp"

#include <d3d12.h>
#include <format>

namespace RL
{
    DescriptorManager::~DescriptorManager()
    {
        Shutdown();
    }

    std::expected<void, std::string> DescriptorManager::Initialize(ID3D12Device* device, u32 capacity)
    {
        if (!device)
        {
            return std::unexpected("Device pointer is null when initializing DescriptorManager.");
        }
        if (capacity == 0)
        {
            return std::unexpected("Capacity cannot be zero when initializing DescriptorManager.");
        }

        Shutdown();

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = capacity;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NodeMask = 0;

        RL_CHECK_HR(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

        m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_cpuStartHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_gpuStartHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
        m_capacity = capacity;
        m_allocatedCount = 0;
        m_freeList.clear();
        m_allocatedSlots.assign(capacity, false);

        RL_LOG_INFO("[DescriptorManager] Initialized CBV_SRV_UAV heap (capacity: {} descriptors, stride: {} bytes).",
            m_capacity, m_descriptorSize);

        return {};
    }

    void DescriptorManager::Shutdown() noexcept
    {
        m_heap.Reset();
        m_cpuStartHandle = {};
        m_gpuStartHandle = {};
        m_descriptorSize = 0;
        m_capacity = 0;
        m_allocatedCount = 0;
        m_freeList.clear();
        m_allocatedSlots.clear();
    }

    u32 DescriptorManager::Allocate()
    {
        if (!m_freeList.empty())
        {
            const u32 slot = m_freeList.back();
            m_freeList.pop_back();
            m_allocatedSlots[slot] = true;
            return slot;
        }

        if (m_allocatedCount >= m_capacity)
        {
            RL_LOG_ERROR("DescriptorManager capacity exceeded! Maximum: {}", m_capacity);
            return InvalidIndex;
        }

        const u32 slot = m_allocatedCount++;
        m_allocatedSlots[slot] = true;
        return slot;
    }

    void DescriptorManager::Free(u32 index)
    {
        if (index >= m_capacity || !m_allocatedSlots[index])
        {
            RL_LOG_WARN("DescriptorManager: invalid or double free of descriptor index {}", index);
            return;
        }
        m_allocatedSlots[index] = false;
        m_freeList.push_back(index);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorManager::GetCPUHandle(u32 index) const noexcept
    {
        if (index >= m_capacity)
        {
            RL_LOG_ERROR("DescriptorManager: CPU descriptor index {} out of bounds!", index);
            return m_cpuStartHandle;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStartHandle;
        handle.ptr += static_cast<SIZE_T>(index) * m_descriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorManager::GetGPUHandle(u32 index) const noexcept
    {
        if (index >= m_capacity)
        {
            RL_LOG_ERROR("DescriptorManager: GPU descriptor index {} out of bounds!", index);
            return m_gpuStartHandle;
        }
        D3D12_GPU_DESCRIPTOR_HANDLE handle = m_gpuStartHandle;
        handle.ptr += static_cast<UINT64>(index) * m_descriptorSize;
        return handle;
    }

    void DescriptorManager::Bind(ID3D12GraphicsCommandList* cmdList) const noexcept
    {
        if (!cmdList || !m_heap) return;

        ID3D12DescriptorHeap* heaps[] = { m_heap.Get() };
        cmdList->SetDescriptorHeaps(1, heaps);
    }

    void DescriptorManager::SetGraphicsRootDescriptorTable(
        ID3D12GraphicsCommandList* cmdList,
        u32 rootParamIndex,
        u32 descriptorIndex) const noexcept
    {
        if (!cmdList || !m_heap) return;

        cmdList->SetGraphicsRootDescriptorTable(rootParamIndex, GetGPUHandle(descriptorIndex));
    }
}
