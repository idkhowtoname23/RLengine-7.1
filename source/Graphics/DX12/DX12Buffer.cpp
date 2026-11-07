#include "Graphics/DX12/DX12Buffer.hpp"

#include <cstring>
#include <format>

namespace RL
{
    GPUBuffer::GPUBuffer(GPUBuffer&& other) noexcept
        : m_resource(std::move(other.m_resource))
        , m_mappedPtr(other.m_mappedPtr)
        , m_sizeInBytes(other.m_sizeInBytes)
        , m_type(other.m_type)
        , m_heapType(other.m_heapType)
    {
        other.m_mappedPtr = nullptr;
        other.m_sizeInBytes = 0;
    }

    GPUBuffer& GPUBuffer::operator=(GPUBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();

            m_resource = std::move(other.m_resource);
            m_mappedPtr = other.m_mappedPtr;
            m_sizeInBytes = other.m_sizeInBytes;
            m_type = other.m_type;
            m_heapType = other.m_heapType;

            other.m_mappedPtr = nullptr;
            other.m_sizeInBytes = 0;
        }
        return *this;
    }

    GPUBuffer::~GPUBuffer()
    {
        Destroy();
    }

    void GPUBuffer::Destroy() noexcept
    {
        if (m_mappedPtr && m_resource)
        {
            Unmap(0);
        }
        m_resource.Reset();
        m_mappedPtr = nullptr;
        m_sizeInBytes = 0;
    }

    std::expected<void, std::string> GPUBuffer::Initialize(
        ID3D12Device* device,
        usize sizeInBytes,
        BufferType type,
        D3D12_HEAP_TYPE heapType)
    {
        if (!device)
        {
            return std::unexpected("Device pointer is null when creating GPUBuffer.");
        }
        if (sizeInBytes == 0)
        {
            return std::unexpected("Cannot create GPUBuffer with zero size.");
        }

        Destroy();

        m_type = type;
        m_heapType = heapType;

        if (type == BufferType::Constant)
        {
            m_sizeInBytes = AlignUp(sizeInBytes, D3D12_CBV_ALIGNMENT);
        }
        else
        {
            m_sizeInBytes = sizeInBytes;
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = heapType;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Alignment = 0;
        resDesc.Width = m_sizeInBytes;
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc.Count = 1;
        resDesc.SampleDesc.Quality = 0;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        const D3D12_RESOURCE_STATES initialState = (heapType == D3D12_HEAP_TYPE_UPLOAD)
            ? D3D12_RESOURCE_STATE_GENERIC_READ
            : D3D12_RESOURCE_STATE_COMMON;

        RL_CHECK_HR(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&m_resource)
        ));

        if (heapType == D3D12_HEAP_TYPE_UPLOAD)
        {
            const D3D12_RANGE readRange{ 0, 0 };
            RL_CHECK_HR(m_resource->Map(0, &readRange, &m_mappedPtr));
        }

        return {};
    }

    void* GPUBuffer::Map(u32 subresource)
    {
        if (m_mappedPtr)
        {
            return m_mappedPtr;
        }

        if (!m_resource)
        {
            return nullptr;
        }

        const D3D12_RANGE readRange{ 0, 0 };
        if (FAILED(m_resource->Map(subresource, &readRange, &m_mappedPtr)))
        {
            return nullptr;
        }

        return m_mappedPtr;
    }

    void GPUBuffer::Unmap(u32 subresource)
    {
        if (m_mappedPtr && m_resource)
        {
            const D3D12_RANGE writtenRange{ 0, m_sizeInBytes };
            m_resource->Unmap(subresource, &writtenRange);
            m_mappedPtr = nullptr;
        }
    }

    void GPUBuffer::UpdateData(const void* data, usize sizeInBytes, usize offsetInBytes)
    {
        if (!data || sizeInBytes == 0 || (offsetInBytes + sizeInBytes > m_sizeInBytes))
        {
            return;
        }

        if (m_mappedPtr)
        {
            std::memcpy(static_cast<byte*>(m_mappedPtr) + offsetInBytes, data, sizeInBytes);
        }
        else
        {
            void* ptr = Map(0);
            if (ptr)
            {
                std::memcpy(static_cast<byte*>(ptr) + offsetInBytes, data, sizeInBytes);
                Unmap(0);
            }
        }
    }

    D3D12_VERTEX_BUFFER_VIEW GPUBuffer::GetVertexBufferView(u32 strideInBytes) const noexcept
    {
        D3D12_VERTEX_BUFFER_VIEW vbv{};
        if (m_resource)
        {
            vbv.BufferLocation = m_resource->GetGPUVirtualAddress();
            vbv.SizeInBytes = static_cast<UINT>(m_sizeInBytes);
            vbv.StrideInBytes = strideInBytes;
        }
        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW GPUBuffer::GetIndexBufferView(DXGI_FORMAT format) const noexcept
    {
        D3D12_INDEX_BUFFER_VIEW ibv{};
        if (m_resource)
        {
            ibv.BufferLocation = m_resource->GetGPUVirtualAddress();
            ibv.SizeInBytes = static_cast<UINT>(m_sizeInBytes);
            ibv.Format = format;
        }
        return ibv;
    }

    D3D12_GPU_VIRTUAL_ADDRESS GPUBuffer::GetGPUVirtualAddress() const noexcept
    {
        return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
    }
}
