#pragma once

#include "Graphics/DX12/DX12Common.hpp"
#include "Graphics/DX12/DX12Texture.hpp"
#include "Graphics/DX12/DescriptorManager.hpp"

#include <string>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <expected>

namespace RL
{
    class DX12Device;

    class TextureManager
    {
    public:
        TextureManager() = default;
        ~TextureManager();

        TextureManager(const TextureManager&) = delete;
        TextureManager& operator=(const TextureManager&) = delete;

        TextureManager(TextureManager&&) noexcept = default;
        TextureManager& operator=(TextureManager&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(
            DX12Device& device,
            DescriptorManager& descriptorManager);

        void Shutdown() noexcept;

        [[nodiscard]] std::shared_ptr<DX12Texture> LoadTexture(const std::filesystem::path& path);

        [[nodiscard]] std::shared_ptr<DX12Texture> CreateColorTexture(
            std::string_view name,
            u8 r, u8 g, u8 b, u8 a = 255);

        [[nodiscard]] std::shared_ptr<DX12Texture> GetWhiteTexture() const noexcept { return m_whiteTexture; }
        [[nodiscard]] u32 GetWhiteTextureDescriptorIndex() const noexcept { return m_whiteDescriptorIndex; }

        [[nodiscard]] std::shared_ptr<DX12Texture> GetTextureByPath(const std::filesystem::path& path) const;
        [[nodiscard]] std::shared_ptr<DX12Texture> GetTextureByIndex(u32 descriptorIndex) const;
        [[nodiscard]] usize GetLoadedTextureCount() const noexcept { return m_texturesByPath.size(); }

    private:
        DX12Device* m_device = nullptr;
        DescriptorManager* m_descriptorManager = nullptr;

        std::shared_ptr<DX12Texture> m_whiteTexture;
        u32 m_whiteDescriptorIndex = 0;

        std::unordered_map<std::string, std::shared_ptr<DX12Texture>> m_texturesByPath;
        std::unordered_map<u32, std::shared_ptr<DX12Texture>> m_texturesByIndex;
    };
}
