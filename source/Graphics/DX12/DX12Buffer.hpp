#pragma once

#include "Graphics/DX12/DX12Common.hpp"

#include <string>
#include <expected>
#include <span>

namespace RL
{
    enum class BufferType : u8
    {
        Generic,
        Vertex,
        Index,
        Constant
    };

    inline constexpr usize D3D12_CBV_ALIGNMENT = 256;

    [[nodiscard]] constexpr usize AlignUp(usize size, usize alignment) noexcept
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

    class GPUBuffer
    {
    public:
        GPUBuffer() = default;
        ~GPUBuffer();

        GPUBuffer(const GPUBuffer&) = delete;
        GPUBuffer& operator=(const GPUBuffer&) = delete;

        GPUBuffer(GPUBuffer&& other) noexcept;
        GPUBuffer& operator=(GPUBuffer&& other) noexcept;

        [[nodiscard]] std::expected<void, std::string> Initialize(
            ID3D12Device* device,
            usize sizeInBytes,
            BufferType type = BufferType::Generic,
            D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_UPLOAD);

        void Destroy() noexcept;

        [[nodiscard]] void* Map(u32 subresource = 0);
        void Unmap(u32 subresource = 0);

        void UpdateData(const void* data, usize sizeInBytes, usize offsetInBytes = 0);

        template <typename T>
        void UpdateData(std::span<const T> data, usize elementOffset = 0)
        {
            UpdateData(data.data(), data.size_bytes(), elementOffset * sizeof(T));
        }

        template <typename T>
        void UpdateData(const T& data, usize byteOffset = 0)
        {
            UpdateData(&data, sizeof(T), byteOffset);
        }

        [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView(u32 strideInBytes) const noexcept;
        [[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView(DXGI_FORMAT format = DXGI_FORMAT_R32_UINT) const noexcept;

        [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const noexcept;
        [[nodiscard]] ID3D12Resource* GetResource() const noexcept { return m_resource.Get(); }
        [[nodiscard]] usize GetSizeInBytes() const noexcept { return m_sizeInBytes; }
        [[nodiscard]] BufferType GetBufferType() const noexcept { return m_type; }
        [[nodiscard]] void* GetMappedData() const noexcept { return m_mappedPtr; }

    private:
        ComPtr<ID3D12Resource> m_resource;
        void* m_mappedPtr = nullptr;
        usize m_sizeInBytes = 0;
        BufferType m_type = BufferType::Generic;
        D3D12_HEAP_TYPE m_heapType = D3D12_HEAP_TYPE_UPLOAD;
    };
}
