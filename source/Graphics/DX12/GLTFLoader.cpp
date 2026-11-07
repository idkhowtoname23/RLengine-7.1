#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "Graphics/DX12/GLTFLoader.hpp"
#include "Core/Logger.hpp"

#include <filesystem>
#include <format>

namespace RL
{
    std::expected<ModelData, std::string> GLTFLoader::LoadFromFile(
        ID3D12Device* device,
        std::string_view filepath)
    {
        if (!device)
        {
            return std::unexpected("Device pointer is null when loading glTF.");
        }

        const std::string pathStr(filepath);
        if (!std::filesystem::exists(pathStr))
        {
            return std::unexpected(std::format("glTF file does not exist: '{}'", pathStr));
        }

        cgltf_options options{};
        cgltf_data* data = nullptr;

        cgltf_result parseRes = cgltf_parse_file(&options, pathStr.c_str(), &data);
        if (parseRes != cgltf_result_success || !data)
        {
            return std::unexpected(std::format("cgltf failed to parse file '{}': error code {}", pathStr, static_cast<int>(parseRes)));
        }

        cgltf_result bufRes = cgltf_load_buffers(&options, data, pathStr.c_str());
        if (bufRes != cgltf_result_success)
        {
            cgltf_free(data);
            return std::unexpected(std::format("cgltf failed to load buffers for '{}': error code {}", pathStr, static_cast<int>(bufRes)));
        }

        ModelData modelData;
        modelData.Name = std::filesystem::path(pathStr).stem().string();

        const std::filesystem::path modelDir = std::filesystem::path(pathStr).parent_path();

        for (cgltf_size m = 0; m < data->meshes_count; ++m)
        {
            const cgltf_mesh& gltfMesh = data->meshes[m];
            const std::string meshName = gltfMesh.name ? gltfMesh.name : std::format("{}_Mesh{}", modelData.Name, m);

            for (cgltf_size p = 0; p < gltfMesh.primitives_count; ++p)
            {
                const cgltf_primitive& prim = gltfMesh.primitives[p];

                const cgltf_accessor* posAccessor = nullptr;
                const cgltf_accessor* normAccessor = nullptr;
                const cgltf_accessor* uvAccessor = nullptr;
                const cgltf_accessor* colAccessor = nullptr;
                const cgltf_accessor* jointsAccessor = nullptr;
                const cgltf_accessor* weightsAccessor = nullptr;

                for (cgltf_size a = 0; a < prim.attributes_count; ++a)
                {
                    const cgltf_attribute& attr = prim.attributes[a];
                    if (attr.type == cgltf_attribute_type_position)
                    {
                        posAccessor = attr.data;
                    }
                    else if (attr.type == cgltf_attribute_type_normal)
                    {
                        normAccessor = attr.data;
                    }
                    else if (attr.type == cgltf_attribute_type_texcoord)
                    {
                        uvAccessor = attr.data;
                    }
                    else if (attr.type == cgltf_attribute_type_color)
                    {
                        colAccessor = attr.data;
                    }
                    else if (attr.type == cgltf_attribute_type_joints)
                    {
                        jointsAccessor = attr.data;
                    }
                    else if (attr.type == cgltf_attribute_type_weights)
                    {
                        weightsAccessor = attr.data;
                    }
                }

                if (!posAccessor || posAccessor->count == 0)
                {
                    continue;
                }

                const cgltf_size vertCount = posAccessor->count;
                std::vector<MeshVertex> vertices(vertCount);

                for (cgltf_size i = 0; i < vertCount; ++i)
                {
                    float pos[3]{ 0.0f, 0.0f, 0.0f };
                    cgltf_accessor_read_float(posAccessor, i, pos, 3);
                    vertices[i].Position = { pos[0], pos[1], pos[2] };

                    if (normAccessor)
                    {
                        float norm[3]{ 0.0f, 1.0f, 0.0f };
                        cgltf_accessor_read_float(normAccessor, i, norm, 3);
                        vertices[i].Normal = { norm[0], norm[1], norm[2] };
                    }
                    else
                    {
                        vertices[i].Normal = { 0.0f, 1.0f, 0.0f };
                    }

                    if (uvAccessor)
                    {
                        float uv[2]{ 0.0f, 0.0f };
                        cgltf_accessor_read_float(uvAccessor, i, uv, 2);
                        vertices[i].UV = { uv[0], uv[1] };
                    }
                    else
                    {
                        vertices[i].UV = { 0.0f, 0.0f };
                    }

                    if (colAccessor)
                    {
                        float col[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
                        cgltf_accessor_read_float(colAccessor, i, col, 4);
                        vertices[i].Color = { col[0], col[1], col[2], col[3] };
                    }
                    else
                    {
                        vertices[i].Color = { 1.0f, 1.0f, 1.0f, 1.0f };
                    }

                    if (jointsAccessor)
                    {
                        cgltf_uint joints[4]{ 0, 0, 0, 0 };
                        cgltf_accessor_read_uint(jointsAccessor, i, joints, 4);
                        vertices[i].BoneIndices = { static_cast<u32>(joints[0]), static_cast<u32>(joints[1]), static_cast<u32>(joints[2]), static_cast<u32>(joints[3]) };
                    }

                    if (weightsAccessor)
                    {
                        float weights[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
                        cgltf_accessor_read_float(weightsAccessor, i, weights, 4);
                        vertices[i].BoneWeights = { weights[0], weights[1], weights[2], weights[3] };
                    }
                }

                std::vector<u32> indices;
                if (prim.indices)
                {
                    const cgltf_size idxCount = prim.indices->count;
                    indices.resize(idxCount);
                    for (cgltf_size i = 0; i < idxCount; ++i)
                    {
                        const cgltf_size rawIdx = cgltf_accessor_read_index(prim.indices, i);
                        indices[i] = (rawIdx < vertCount) ? static_cast<u32>(rawIdx) : 0u;
                    }
                }
                else
                {
                    indices.resize(vertCount);
                    for (cgltf_size i = 0; i < vertCount; ++i)
                    {
                        indices[i] = static_cast<u32>(i);
                    }
                }

                auto meshRes = Mesh::CreateFromData(device, vertices, indices, meshName);
                if (meshRes)
                {
                    modelData.Meshes.push_back(std::move(*meshRes));
                }

                if (prim.material && prim.material->has_pbr_metallic_roughness)
                {
                    const auto& pbr = prim.material->pbr_metallic_roughness;
                    if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image)
                    {
                        const char* uri = pbr.base_color_texture.texture->image->uri;
                        if (uri)
                        {
                            std::string uriStr(uri);
                            if (uriStr.find(':') == std::string::npos)
                            {
                                std::error_code ec;
                                const std::filesystem::path fullTexPath = std::filesystem::weakly_canonical(modelDir / uri, ec);
                                const auto rel = fullTexPath.lexically_relative(modelDir);
                                const std::string relStr = rel.empty() ? "" : rel.string();
                                if (!ec && !relStr.starts_with("..") && !rel.is_absolute())
                                {
                                    modelData.TexturePaths.push_back(fullTexPath.string());
                                }
                            }
                        }
                    }
                }
            }
        }

        for (cgltf_size s = 0; s < data->skins_count; ++s)
        {
            const cgltf_skin& gltfSkin = data->skins[s];
            Skeleton skeleton;
            skeleton.Name = gltfSkin.name ? gltfSkin.name : std::format("{}_Skin{}", modelData.Name, s);
            skeleton.Bones.resize(gltfSkin.joints_count);
            skeleton.SkinMatrices.resize(gltfSkin.joints_count);

            std::vector<float> ibm(gltfSkin.joints_count * 16, 0.0f);
            if (gltfSkin.inverse_bind_matrices)
            {
                for (cgltf_size j = 0; j < gltfSkin.joints_count; ++j)
                {
                    cgltf_accessor_read_float(gltfSkin.inverse_bind_matrices, j, &ibm[j * 16], 16);
                }
            }

            for (cgltf_size j = 0; j < gltfSkin.joints_count; ++j)
            {
                const cgltf_node* jointNode = gltfSkin.joints[j];
                BoneInfo& bone = skeleton.Bones[j];
                bone.Name = jointNode->name ? jointNode->name : std::format("Bone_{}", j);
                if (gltfSkin.inverse_bind_matrices)
                {
                    std::memcpy(&bone.InverseBindMatrix, &ibm[j * 16], sizeof(DirectX::XMFLOAT4X4));
                }
                else
                {
                    DirectX::XMStoreFloat4x4(&bone.InverseBindMatrix, DirectX::XMMatrixIdentity());
                }
                DirectX::XMStoreFloat4x4(&bone.LocalTransform, DirectX::XMMatrixIdentity());
                DirectX::XMStoreFloat4x4(&bone.FinalMatrix, DirectX::XMMatrixIdentity());
                skeleton.SkinMatrices[j] = bone.FinalMatrix;
            }
            modelData.Skeletons.push_back(std::move(skeleton));
        }

        for (cgltf_size a = 0; a < data->animations_count; ++a)
        {
            const cgltf_animation& gltfAnim = data->animations[a];
            AnimationClip clip;
            clip.Name = gltfAnim.name ? gltfAnim.name : std::format("Anim_{}", a);
            for (cgltf_size c = 0; c < gltfAnim.channels_count; ++c)
            {
                const cgltf_animation_channel& ch = gltfAnim.channels[c];
                AnimationTrack track;
                if (ch.target_node && ch.target_node->name)
                {
                    track.BoneName = ch.target_node->name;
                }
                if (ch.sampler && ch.sampler->input)
                {
                    const cgltf_size keyCount = ch.sampler->input->count;
                    track.Timestamps.resize(keyCount);
                    for (cgltf_size k = 0; k < keyCount; ++k)
                    {
                        cgltf_accessor_read_float(ch.sampler->input, k, &track.Timestamps[k], 1);
                        if (track.Timestamps[k] > clip.Duration)
                        {
                            clip.Duration = track.Timestamps[k];
                        }
                    }
                }
                clip.Tracks.push_back(std::move(track));
            }
            modelData.Animations.push_back(std::move(clip));
        }

        cgltf_free(data);

        if (modelData.Meshes.empty())
        {
            return std::unexpected(std::format("No valid renderable meshes found in glTF file '{}'", pathStr));
        }

        RL_LOG_INFO("[GLTFLoader] Loaded model '{}' ({} submeshes) from '{}'.",
            modelData.Name, modelData.Meshes.size(), pathStr);

        return modelData;
    }

    std::expected<std::shared_ptr<Mesh>, std::string> GLTFLoader::LoadSingleMesh(
        ID3D12Device* device,
        std::string_view filepath)
    {
        auto modelRes = LoadFromFile(device, filepath);
        if (!modelRes)
        {
            return std::unexpected(modelRes.error());
        }

        if (modelRes->Meshes.empty())
        {
            return std::unexpected("Loaded model contains zero meshes.");
        }

        return modelRes->Meshes[0];
    }
}
