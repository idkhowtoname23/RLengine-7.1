#pragma once

#include "Graphics/DX12/DX12Common.hpp"

#include <string>
#include <filesystem>
#include <expected>
#include <span>

struct ID3D12Device;
struct ID3D12CommandQueue;

namespace RL
{
    class DX12Device;

    class DX12Texture
    {
    public:
        DX12Texture() = default;
        ~DX12Texture();

        DX12Texture(const DX12Texture&) = delete;
        DX12Texture& operator=(const DX12Texture&) = delete;

        DX12Texture(DX12Texture&&) noexcept = default;
        DX12Texture& operator=(DX12Texture&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> InitializeFromFile(
            DX12Device& device,
            const std::filesystem::path& path);

        [[nodiscard]] std::expected<void, std::string> InitializeFromEncodedMemory(
            DX12Device& device,
            std::span<const u8> fileData);

        [[nodiscard]] std::expected<void, std::string> InitializeFromRawPixels(
            DX12Device& device,
            const void* rawPixels,
            u32 width,
            u32 height);

        void Destroy() noexcept;

        void CreateShaderResourceView(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const noexcept;

        [[nodiscard]] ID3D12Resource* GetResource() const noexcept { return m_resource.Get(); }
        [[nodiscard]] u32 GetWidth() const noexcept { return m_width; }
        [[nodiscard]] u32 GetHeight() const noexcept { return m_height; }
        [[nodiscard]] u32 GetMipLevels() const noexcept { return m_mipLevels; }
        [[nodiscard]] u32 GetDescriptorIndex() const noexcept { return m_descriptorIndex; }
        void SetDescriptorIndex(u32 index) noexcept { m_descriptorIndex = index; }
        [[nodiscard]] const std::filesystem::path& GetPath() const noexcept { return m_filePath; }

    private:
        [[nodiscard]] std::expected<void, std::string> UploadPixelData(
            DX12Device& device,
            const u8* pixels,
            u32 width,
            u32 height);

    private:
        ComPtr<ID3D12Resource> m_resource;
        u32 m_width = 0;
        u32 m_height = 0;
        u32 m_mipLevels = 1;
        u32 m_descriptorIndex = 0;
        std::filesystem::path m_filePath;
    };
}
