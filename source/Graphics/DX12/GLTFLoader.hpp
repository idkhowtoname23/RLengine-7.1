#pragma once

#include "Core/Types.hpp"
#include "Graphics/DX12/Mesh.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <expected>
#include <DirectXMath.h>

struct ID3D12Device;

namespace RL
{
    struct BoneInfo
    {
        std::string Name;
        i32 ParentIndex = -1;
        DirectX::XMFLOAT4X4 InverseBindMatrix;
        DirectX::XMFLOAT4X4 LocalTransform;
        DirectX::XMFLOAT4X4 FinalMatrix;
    };

    struct Skeleton
    {
        std::string Name;
        std::vector<BoneInfo> Bones;
        std::vector<DirectX::XMFLOAT4X4> SkinMatrices;

        void UpdatePalette()
        {
            SkinMatrices.resize(Bones.size());
            for (usize i = 0; i < Bones.size(); ++i)
            {
                SkinMatrices[i] = Bones[i].FinalMatrix;
            }
        }
    };

    struct AnimationTrack
    {
        std::string BoneName;
        i32 BoneIndex = -1;
        std::vector<f32> Timestamps;
        std::vector<DirectX::XMFLOAT3> Positions;
        std::vector<DirectX::XMFLOAT4> Rotations;
        std::vector<DirectX::XMFLOAT3> Scales;
    };

    struct AnimationClip
    {
        std::string Name;
        f32 Duration = 0.0f;
        std::vector<AnimationTrack> Tracks;
    };

    struct ModelData
    {
        std::string Name;
        std::vector<std::shared_ptr<Mesh>> Meshes;
        std::vector<std::string> TexturePaths;
        std::vector<Skeleton> Skeletons;
        std::vector<AnimationClip> Animations;
    };

    class GLTFLoader
    {
    public:
        GLTFLoader() = delete;

        [[nodiscard]] static std::expected<ModelData, std::string> LoadFromFile(
            ID3D12Device* device,
            std::string_view filepath);

        [[nodiscard]] static std::expected<std::shared_ptr<Mesh>, std::string> LoadSingleMesh(
            ID3D12Device* device,
            std::string_view filepath);
    };
}
