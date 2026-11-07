#include "Graphics/DX12/DX12Texture.hpp"
#include "Graphics/DX12/DX12Device.hpp"
#include "Core/Logger.hpp"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4244 4100 4245)
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include <d3d12.h>
#include <format>
#include <cstring>

namespace RL
{
    namespace
    {
        std::vector<u8> GenerateNextMip(const u8* src, u32 srcW, u32 srcH, u32 dstW, u32 dstH)
        {
            std::vector<u8> dst(static_cast<usize>(dstW) * dstH * 4);
            for (u32 y = 0; y < dstH; ++y)
            {
                const u32 srcY0 = y * 2;
                const u32 srcY1 = std::min(srcY0 + 1, srcH - 1);
                for (u32 x = 0; x < dstW; ++x)
                {
                    const u32 srcX0 = x * 2;
                    const u32 srcX1 = std::min(srcX0 + 1, srcW - 1);

                    const u8* p00 = src + (srcY0 * srcW + srcX0) * 4;
                    const u8* p10 = src + (srcY0 * srcW + srcX1) * 4;
                    const u8* p01 = src + (srcY1 * srcW + srcX0) * 4;
                    const u8* p11 = src + (srcY1 * srcW + srcX1) * 4;

                    u8* out = dst.data() + (y * dstW + x) * 4;
                    for (u32 c = 0; c < 4; ++c)
                    {
                        out[c] = static_cast<u8>((static_cast<u32>(p00[c]) + p10[c] + p01[c] + p11[c] + 2) / 4);
                    }
                }
            }
            return dst;
        }
    }

    DX12Texture::~DX12Texture()
    {
        Destroy();
    }

    void DX12Texture::Destroy() noexcept
    {
        m_resource.Reset();
        m_width = 0;
        m_height = 0;
        m_mipLevels = 1;
        m_descriptorIndex = 0;
        m_filePath.clear();
    }

    std::expected<void, std::string> DX12Texture::InitializeFromFile(
        DX12Device& device,
        const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            return std::unexpected(std::format("Image file not found: '{}'", path.string()));
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            return std::unexpected(std::format("stb_image failed to load '{}': {}",
                path.string(), stbi_failure_reason()));
        }

        auto uploadRes = UploadPixelData(device, pixels, static_cast<u32>(width), static_cast<u32>(height));
        stbi_image_free(pixels);

        if (!uploadRes)
        {
            return uploadRes;
        }

        m_filePath = path;
        RL_LOG_INFO("[DX12Texture] Loaded texture '{}' ({}x{}, RGBA8_UNORM).", path.string(), m_width, m_height);
        return {};
    }

    std::expected<void, std::string> DX12Texture::InitializeFromEncodedMemory(
        DX12Device& device,
        std::span<const u8> fileData)
    {
        if (fileData.empty())
        {
            return std::unexpected("Encoded image buffer is empty.");
        }

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_uc* pixels = stbi_load_from_memory(
            fileData.data(),
            static_cast<int>(fileData.size()),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha
        );

        if (!pixels)
        {
            return std::unexpected(std::format("stb_image failed to load from memory buffer: {}",
                stbi_failure_reason()));
        }

        auto uploadRes = UploadPixelData(device, pixels, static_cast<u32>(width), static_cast<u32>(height));
        stbi_image_free(pixels);

        if (!uploadRes)
        {
            return uploadRes;
        }

        RL_LOG_INFO("[DX12Texture] Loaded texture from memory ({}x{}, RGBA8_UNORM).", m_width, m_height);
        return {};
    }

    std::expected<void, std::string> DX12Texture::InitializeFromRawPixels(
        DX12Device& device,
        const void* rawPixels,
        u32 width,
        u32 height)
    {
        if (!rawPixels || width == 0 || height == 0)
        {
            return std::unexpected("Invalid raw pixel parameters.");
        }

        return UploadPixelData(device, static_cast<const u8*>(rawPixels), width, height);
    }

    std::expected<void, std::string> DX12Texture::UploadPixelData(
        DX12Device& device,
        const u8* pixels,
        u32 width,
        u32 height)
    {
        Destroy();

        if (!pixels || width == 0 || height == 0 || width > 16384 || height > 16384)
        {
            return std::unexpected("Invalid pixel buffer or dimensions for texture upload (max 16384x16384).");
        }

        ID3D12Device* d3dDevice = device.GetDevice();
        if (!d3dDevice)
        {
            return std::unexpected("Direct3D 12 device is null.");
        }

        m_width = width;
        m_height = height;
        m_mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;

        std::vector<std::vector<u8>> mips(m_mipLevels);
        mips[0].assign(pixels, pixels + (static_cast<usize>(width) * height * 4));

        for (u32 m = 1; m < m_mipLevels; ++m)
        {
            const u32 prevW = std::max(1u, width >> (m - 1));
            const u32 prevH = std::max(1u, height >> (m - 1));
            const u32 curW = std::max(1u, width >> m);
            const u32 curH = std::max(1u, height >> m);
            mips[m] = GenerateNextMip(mips[m - 1].data(), prevW, prevH, curW, curH);
        }

        D3D12_RESOURCE_DESC texDesc{};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Alignment = 0;
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = static_cast<UINT16>(m_mipLevels);
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeapProps{};
        defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeapProps.CreationNodeMask = 1;
        defaultHeapProps.VisibleNodeMask = 1;

        RL_CHECK_HR(d3dDevice->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_resource)
        ));

        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(m_mipLevels);
        std::vector<UINT> numRows(m_mipLevels);
        std::vector<UINT64> rowSizesInBytes(m_mipLevels);
        UINT64 uploadBufferSize = 0;

        d3dDevice->GetCopyableFootprints(
            &texDesc,
            0,
            m_mipLevels,
            0,
            layouts.data(),
            numRows.data(),
            rowSizesInBytes.data(),
            &uploadBufferSize
        );

        D3D12_HEAP_PROPERTIES uploadHeapProps{};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeapProps.CreationNodeMask = 1;
        uploadHeapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC uploadBufferDesc{};
        uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadBufferDesc.Alignment = 0;
        uploadBufferDesc.Width = uploadBufferSize;
        uploadBufferDesc.Height = 1;
        uploadBufferDesc.DepthOrArraySize = 1;
        uploadBufferDesc.MipLevels = 1;
        uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadBufferDesc.SampleDesc.Count = 1;
        uploadBufferDesc.SampleDesc.Quality = 0;
        uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ComPtr<ID3D12Resource> uploadBuffer;
        RL_CHECK_HR(d3dDevice->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)
        ));

        void* mappedData = nullptr;
        const D3D12_RANGE readRange{ 0, 0 };
        RL_CHECK_HR(uploadBuffer->Map(0, &readRange, &mappedData));

        for (u32 m = 0; m < m_mipLevels; ++m)
        {
            const u8* srcData = mips[m].data();
            u8* dstData = static_cast<u8*>(mappedData) + layouts[m].Offset;
            const u32 curW = std::max(1u, width >> m);
            const u32 curH = std::max(1u, height >> m);
            const UINT maxRows = std::min(numRows[m], curH);
            const usize rowBytes = static_cast<usize>(curW) * 4;

            for (UINT row = 0; row < maxRows; ++row)
            {
                std::memcpy(
                    dstData + (row * layouts[m].Footprint.RowPitch),
                    srcData + (row * rowBytes),
                    rowBytes
                );
            }
        }

        uploadBuffer->Unmap(0, nullptr);

        device.ExecuteSingleTimeCommands([&](ID3D12GraphicsCommandList* cmdList) {
            for (u32 m = 0; m < m_mipLevels; ++m)
            {
                D3D12_TEXTURE_COPY_LOCATION dst{};
                dst.pResource = m_resource.Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = m;

                D3D12_TEXTURE_COPY_LOCATION src{};
                src.pResource = uploadBuffer.Get();
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint = layouts[m];

                cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            }

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_resource.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            cmdList->ResourceBarrier(1, &barrier);
        });

        return {};
    }

    void DX12Texture::CreateShaderResourceView(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const noexcept
    {
        if (!device || !m_resource) return;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_mipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        device->CreateShaderResourceView(m_resource.Get(), &srvDesc, cpuHandle);
    }
}
