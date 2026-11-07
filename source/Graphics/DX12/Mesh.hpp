#pragma once

#include "Core/Types.hpp"
#include "Graphics/DX12/DX12Common.hpp"
#include "Graphics/DX12/DX12Buffer.hpp"

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <span>
#include <memory>
#include <expected>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace RL
{
    struct MeshVertex
    {
        DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Normal{ 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT2 UV{ 0.0f, 0.0f };
        DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMUINT4  BoneIndices{ 0, 0, 0, 0 };
        DirectX::XMFLOAT4 BoneWeights{ 0.0f, 0.0f, 0.0f, 0.0f };
    };

    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh() = default;

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&&) noexcept = default;
        Mesh& operator=(Mesh&&) noexcept = default;

        [[nodiscard]] static std::expected<std::shared_ptr<Mesh>, std::string> CreateFromData(
            ID3D12Device* device,
            std::span<const MeshVertex> vertices,
            std::span<const u32> indices,
            std::string_view name = "Mesh",
            ID3D12CommandQueue* commandQueue = nullptr);

        [[nodiscard]] static std::expected<std::shared_ptr<Mesh>, std::string> CreateCube(
            ID3D12Device* device,
            f32 size = 1.0f,
            ID3D12CommandQueue* commandQueue = nullptr);

        [[nodiscard]] static std::expected<std::shared_ptr<Mesh>, std::string> CreateSphere(
            ID3D12Device* device,
            f32 radius = 0.5f,
            u32 slices = 24,
            u32 stacks = 16,
            ID3D12CommandQueue* commandQueue = nullptr);

        [[nodiscard]] static std::expected<std::shared_ptr<Mesh>, std::string> CreatePlane(
            ID3D12Device* device,
            f32 width = 10.0f,
            f32 depth = 10.0f,
            u32 subdivisions = 1,
            ID3D12CommandQueue* commandQueue = nullptr);

        [[nodiscard]] static std::expected<std::shared_ptr<Mesh>, std::string> CreateGrid(
            ID3D12Device* device,
            f32 size = 20.0f,
            u32 lines = 40,
            ID3D12CommandQueue* commandQueue = nullptr);

        void Bind(ID3D12GraphicsCommandList* cmdList) const noexcept;
        void Draw(ID3D12GraphicsCommandList* cmdList) const noexcept;

        [[nodiscard]] u32 GetVertexCount() const noexcept { return m_vertexCount; }
        [[nodiscard]] u32 GetIndexCount() const noexcept { return m_indexCount; }
        [[nodiscard]] std::string_view GetName() const noexcept { return m_name; }
        void SetName(std::string_view name) { m_name = name; }

    private:
        GPUBuffer m_vertexBuffer;
        GPUBuffer m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vbView{};
        D3D12_INDEX_BUFFER_VIEW m_ibView{};

        u32 m_vertexCount = 0;
        u32 m_indexCount = 0;
        std::string m_name;
    };
}
