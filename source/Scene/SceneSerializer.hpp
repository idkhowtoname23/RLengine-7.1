#pragma once

#include "Scene/Scene.hpp"
#include <filesystem>

struct ID3D12Device;

namespace RL
{
    class SceneSerializer
    {
    public:
        static bool SerializeJson(const Scene& scene, const std::filesystem::path& filepath);
        static bool DeserializeJson(Scene& scene, ID3D12Device* device, const std::filesystem::path& filepath);

        static bool SerializeBinary(const Scene& scene, const std::filesystem::path& filepath);
        static bool DeserializeBinary(Scene& scene, ID3D12Device* device, const std::filesystem::path& filepath);

        static bool Save(const Scene& scene, const std::filesystem::path& filepath);
        static bool Load(Scene& scene, ID3D12Device* device, const std::filesystem::path& filepath);
    };
}
