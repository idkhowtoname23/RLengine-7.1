#include "Graphics/DX12/TextureManager.hpp"
#include "Graphics/DX12/DX12Device.hpp"
#include "Core/Logger.hpp"

namespace RL
{
    TextureManager::~TextureManager()
    {
        Shutdown();
    }

    std::expected<void, std::string> TextureManager::Initialize(
        DX12Device& device,
        DescriptorManager& descriptorManager)
    {
        m_device = &device;
        m_descriptorManager = &descriptorManager;

        const u32 whitePixel = 0xFFFFFFFF;
        m_whiteTexture = std::make_shared<DX12Texture>();
        auto initRes = m_whiteTexture->InitializeFromRawPixels(device, &whitePixel, 1, 1);
        if (!initRes)
        {
            return std::unexpected(std::format("Failed to create default white texture: {}", initRes.error()));
        }

        m_whiteDescriptorIndex = m_descriptorManager->Allocate();
        m_whiteTexture->SetDescriptorIndex(m_whiteDescriptorIndex);
        m_whiteTexture->CreateShaderResourceView(
            m_device->GetDevice(),
            m_descriptorManager->GetCPUHandle(m_whiteDescriptorIndex)
        );

        m_texturesByPath["__default_white"] = m_whiteTexture;
        m_texturesByIndex[m_whiteDescriptorIndex] = m_whiteTexture;

        RL_LOG_INFO("[TextureManager] Initialized with Default 1x1 White Texture bound at descriptor index {}.",
            m_whiteDescriptorIndex);

        return {};
    }

    void TextureManager::Shutdown() noexcept
    {
        m_texturesByPath.clear();
        m_texturesByIndex.clear();
        m_whiteTexture.reset();
        m_whiteDescriptorIndex = 0;
        m_device = nullptr;
        m_descriptorManager = nullptr;
    }

    std::shared_ptr<DX12Texture> TextureManager::LoadTexture(const std::filesystem::path& path)
    {
        if (!m_device || !m_descriptorManager)
        {
            RL_LOG_ERROR("[TextureManager] Attempted to load texture before initialization.");
            return m_whiteTexture;
        }

        const std::string key = path.lexically_normal().string();
        if (auto it = m_texturesByPath.find(key); it != m_texturesByPath.end())
        {
            return it->second;
        }

        auto texture = std::make_shared<DX12Texture>();
        auto loadRes = texture->InitializeFromFile(*m_device, path);
        if (!loadRes)
        {
            RL_LOG_ERROR("[TextureManager] Failed to load '{}': {}. Falling back to default white texture.",
                key, loadRes.error());
            return m_whiteTexture;
        }

        const u32 descriptorSlot = m_descriptorManager->Allocate();
        if (descriptorSlot == DescriptorManager::InvalidIndex)
        {
            RL_LOG_ERROR("[TextureManager] Descriptor heap exhausted when registering '{}'. Falling back to default white texture.", key);
            return m_whiteTexture;
        }
        texture->SetDescriptorIndex(descriptorSlot);
        texture->CreateShaderResourceView(
            m_device->GetDevice(),
            m_descriptorManager->GetCPUHandle(descriptorSlot)
        );

        m_texturesByPath[key] = texture;
        m_texturesByIndex[descriptorSlot] = texture;

        RL_LOG_INFO("[TextureManager] Texture '{}' registered with SRV descriptor index {}.",
            key, descriptorSlot);

        return texture;
    }

    std::shared_ptr<DX12Texture> TextureManager::CreateColorTexture(
        std::string_view name,
        u8 r, u8 g, u8 b, u8 a)
    {
        if (!m_device || !m_descriptorManager)
        {
            return m_whiteTexture;
        }

        const std::string key = std::string(name);
        if (auto it = m_texturesByPath.find(key); it != m_texturesByPath.end())
        {
            return it->second;
        }

        const u32 pixel = (static_cast<u32>(a) << 24) |
                          (static_cast<u32>(b) << 16) |
                          (static_cast<u32>(g) << 8)  |
                           static_cast<u32>(r);

        auto texture = std::make_shared<DX12Texture>();
        auto initRes = texture->InitializeFromRawPixels(*m_device, &pixel, 1, 1);
        if (!initRes)
        {
            return m_whiteTexture;
        }

        const u32 descriptorSlot = m_descriptorManager->Allocate();
        if (descriptorSlot == DescriptorManager::InvalidIndex)
        {
            RL_LOG_ERROR("[TextureManager] Descriptor heap exhausted when registering '{}'. Falling back to default white texture.", key);
            return m_whiteTexture;
        }
        texture->SetDescriptorIndex(descriptorSlot);
        texture->CreateShaderResourceView(
            m_device->GetDevice(),
            m_descriptorManager->GetCPUHandle(descriptorSlot)
        );

        m_texturesByPath[key] = texture;
        m_texturesByIndex[descriptorSlot] = texture;

        return texture;
    }

    std::shared_ptr<DX12Texture> TextureManager::GetTextureByPath(const std::filesystem::path& path) const
    {
        const std::string key = path.lexically_normal().string();
        if (auto it = m_texturesByPath.find(key); it != m_texturesByPath.end())
        {
            return it->second;
        }
        return m_whiteTexture;
    }

    std::shared_ptr<DX12Texture> TextureManager::GetTextureByIndex(u32 descriptorIndex) const
    {
        if (auto it = m_texturesByIndex.find(descriptorIndex); it != m_texturesByIndex.end())
        {
            return it->second;
        }
        return m_whiteTexture;
    }
}

