#pragma once

#include "Core/Types.hpp"
#include "Graphics/DX12/DX12Common.hpp"
#include "Graphics/DX12/DX12Buffer.hpp"
#include "Graphics/DX12/Pipeline.hpp"
#include "Scene/Entity.hpp"

#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <expected>
#include <string>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace RL
{
    class Scene;
    class DescriptorManager;
    class ShaderCompiler;

    struct alignas(16) ParticleInstance
    {
        DirectX::XMFLOAT4 PositionSize;
        DirectX::XMFLOAT4 Color;
    };

    struct Particle
    {
        DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Velocity{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 StartColor{ 1.0f, 0.6f, 0.1f, 1.0f };
        DirectX::XMFLOAT4 EndColor{ 0.8f, 0.1f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Gravity{ 0.0f, -4.9f, 0.0f };
        f32 Life = 0.0f;
        f32 MaxLife = 1.0f;
        f32 StartSize = 0.1f;
        f32 EndSize = 0.01f;
        f32 Size = 0.1f;
    };

    class ParticleSystem
    {
    public:
        static constexpr u32 MaxTotalParticles = 65536;

        ParticleSystem() = default;
        ~ParticleSystem();

        [[nodiscard]] std::expected<void, std::string> Initialize(
            ID3D12Device* device,
            DescriptorManager* descriptorManager,
            ShaderCompiler* shaderCompiler);

        void Destroy() noexcept;

        void Update(Scene& scene, f32 dt);
        void Render(ID3D12GraphicsCommandList* cmdList, const DirectX::XMMATRIX& viewProj, u32 frameIndex);

        [[nodiscard]] u32 GetActiveParticleCount() const noexcept { return m_activeCount; }

    private:
        std::vector<Particle> m_particles;
        u32 m_activeCount = 0;

        GPUBuffer m_vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vbView{};
        GPUBuffer m_instanceBuffers[NUM_FRAMES];
        GPUBuffer m_constantBuffers[NUM_FRAMES];

        Pipeline m_pipeline;
        DescriptorManager* m_descriptorManager = nullptr;
        bool m_isInitialized = false;
        f32 m_emitAccumulator = 0.0f;
    };
}
