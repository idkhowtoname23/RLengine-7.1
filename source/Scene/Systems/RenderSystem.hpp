#pragma once

#include "Core/Types.hpp"
#include "Core/Memory/Allocators.hpp"
#include "Graphics/DX12/DX12Common.hpp"
#include "Graphics/DX12/DX12Buffer.hpp"
#include "Graphics/DX12/Vertex.hpp"
#include "Graphics/DX12/Pipeline.hpp"
#include "Graphics/DX12/Mesh.hpp"
#include "Graphics/DX12/ShadowMap.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <vector>
#include <DirectXMath.h>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace RL
{
    class Scene;
    class DescriptorManager;
    class TextureManager;
    class ShaderCompiler;

    struct FrustumPlanes
    {
        DirectX::XMFLOAT4 Planes[6];
        [[nodiscard]] bool TestSphere(const DirectX::XMFLOAT3& center, float radius) const noexcept;
    };

    struct MeshBatch
    {
        Mesh* MeshPtr = nullptr;
        u32 StartInstance = 0;
        u32 Count = 0;
    };

    class RenderSystem
    {
    public:
        static constexpr u32 InitialInstanceCapacity = 16384;
        static constexpr u32 ConstantBufferStride = 1024;
        static constexpr u32 FrameCount = NUM_FRAMES;

        RenderSystem() = default;
        ~RenderSystem();

        RenderSystem(const RenderSystem&) = delete;
        RenderSystem& operator=(const RenderSystem&) = delete;

        RenderSystem(RenderSystem&&) noexcept = default;
        RenderSystem& operator=(RenderSystem&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(
            ID3D12Device* device,
            DescriptorManager* descriptorManager = nullptr,
            TextureManager* textureManager = nullptr,
            ShaderCompiler* shaderCompiler = nullptr);
        void Destroy() noexcept;

        void SetDescriptorManager(DescriptorManager* descriptorManager) noexcept { m_descriptorManager = descriptorManager; }
        void SetTextureManager(TextureManager* textureManager) noexcept { m_textureManager = textureManager; }
        void SetQuadPipeline(Pipeline* pipeline) noexcept { m_quadPipeline = pipeline; }
        void SetShaderCompiler(ShaderCompiler* shaderCompiler) noexcept { m_shaderCompiler = shaderCompiler; }

        [[nodiscard]] DescriptorManager* GetDescriptorManager() const noexcept { return m_descriptorManager; }
        [[nodiscard]] TextureManager* GetTextureManager() const noexcept { return m_textureManager; }
        [[nodiscard]] Pipeline* GetQuadPipeline() const noexcept { return m_quadPipeline; }
        [[nodiscard]] ShaderCompiler* GetShaderCompiler() const noexcept { return m_shaderCompiler; }

        void SetViewport(const D3D12_VIEWPORT& vp, const D3D12_RECT& scissor) noexcept
        {
            m_viewport = vp;
            m_scissorRect = scissor;
        }
        void SetMainRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) noexcept
        {
            m_mainRtv = rtv;
            m_mainDsv = dsv;
        }
        [[nodiscard]] ShadowMap* GetShadowMap() const noexcept { return m_shadowMap.get(); }
        [[nodiscard]] Memory::FrameAllocator& GetTransientAllocator() noexcept { return m_transientAllocator; }

        void Render(Scene& scene, ID3D12GraphicsCommandList* cmdList, u32 frameIndex);

        [[nodiscard]] u32 GetLastRenderedCount() const noexcept { return m_lastRenderedCount; }
        [[nodiscard]] u32 GetInstanceCapacity() const noexcept { return m_instanceCapacity; }
        void EnsureInstanceCapacity(u32 neededCapacity);

        void SetShowGrid(bool show) noexcept { m_showGrid = show; }
        [[nodiscard]] bool IsGridVisible() const noexcept { return m_showGrid; }

        void RegisterMesh(std::string_view name, std::shared_ptr<Mesh> mesh);
        [[nodiscard]] std::shared_ptr<Mesh> GetMesh(std::string_view name) const;
        [[nodiscard]] std::vector<std::string> GetAvailableMeshNames() const;
        [[nodiscard]] const std::vector<MeshBatch>& GetActiveBatches() const noexcept { return m_activeBatches; }

    private:
        void PrepareMeshInstances(
            Scene& scene,
            InstanceData* mappedInstances,
            u32& meshInstanceCount,
            const FrustumPlanes& frustum);
        void PrepareQuadInstances(
            Scene& scene,
            InstanceData* mappedInstances,
            u32& renderedCount);

        FrustumPlanes m_currentFrustum{};
        std::vector<MeshBatch> m_activeBatches;
        std::vector<u32> m_visibleMeshIndices;
        std::vector<u32> m_visibleQuadIndices;
        std::vector<std::vector<u32>> m_chunkVisibleMesh;
        std::vector<std::vector<u32>> m_chunkVisibleQuad;

        ID3D12Device* m_device = nullptr;
        u32 m_instanceCapacity = InitialInstanceCapacity;

        GPUBuffer m_vertexBuffer;
        GPUBuffer m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vbView{};
        D3D12_INDEX_BUFFER_VIEW m_ibView{};

        GPUBuffer m_constantBuffers[NUM_FRAMES];
        GPUBuffer m_dirShadowConstantBuffers[NUM_FRAMES];
        GPUBuffer m_cascadeConstantBuffers[FrameCount][ShadowMap::CascadeCount];
        GPUBuffer m_pointShadowConstantBuffers[NUM_FRAMES];
        GPUBuffer m_boneBuffers[NUM_FRAMES];
        GPUBuffer m_instanceBuffers[NUM_FRAMES];
        GPUBuffer m_frameConstantBuffers[NUM_FRAMES];
        u32 m_lastRenderedCount = 0;
        bool m_isInitialized = false;
        bool m_showGrid = true;

        DescriptorManager* m_descriptorManager = nullptr;
        TextureManager* m_textureManager = nullptr;
        Pipeline m_ownedQuadPipeline;
        Pipeline* m_quadPipeline = &m_ownedQuadPipeline;
        ShaderCompiler* m_shaderCompiler = nullptr;

        Memory::FrameAllocator m_transientAllocator{ 32 * 1024 * 1024 };

        Pipeline m_meshPipeline;
        Pipeline m_gridPipeline;
        Pipeline m_shadowPipeline;
        std::unique_ptr<ShadowMap> m_shadowMap;

        D3D12_VIEWPORT m_viewport{};
        D3D12_RECT m_scissorRect{};
        D3D12_CPU_DESCRIPTOR_HANDLE m_mainRtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE m_mainDsv{};

        std::shared_ptr<Mesh> m_cubeMesh;
        std::shared_ptr<Mesh> m_sphereMesh;
        std::shared_ptr<Mesh> m_planeMesh;
        std::shared_ptr<Mesh> m_gridMesh;
        std::unordered_map<std::string, std::shared_ptr<Mesh>> m_meshes;
    };
}
