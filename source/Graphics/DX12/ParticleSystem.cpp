#include "Graphics/DX12/ParticleSystem.hpp"
#include "Graphics/DX12/ShaderCompiler.hpp"
#include "Graphics/DX12/DescriptorManager.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"
#include "Core/Logger.hpp"

#include <d3d12.h>
#include <random>
#include <algorithm>

namespace RL
{
    struct ParticleVertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT2 UV;
    };

    ParticleSystem::~ParticleSystem()
    {
        Destroy();
    }

    void ParticleSystem::Destroy() noexcept
    {
        m_vertexBuffer.Destroy();
        for (u32 i = 0; i < NUM_FRAMES; ++i)
        {
            m_instanceBuffers[i].Destroy();
            m_constantBuffers[i].Destroy();
        }
        m_particles.clear();
        m_activeCount = 0;
        m_isInitialized = false;
    }

    std::expected<void, std::string> ParticleSystem::Initialize(
        ID3D12Device* device,
        DescriptorManager* descriptorManager,
        ShaderCompiler* shaderCompiler)
    {
        if (!device || !shaderCompiler)
        {
            return std::unexpected("Device or ShaderCompiler null in ParticleSystem::Initialize.");
        }

        m_descriptorManager = descriptorManager;
        m_particles.resize(MaxTotalParticles);
        m_activeCount = 0;

        const ParticleVertex quadVertices[6] = {
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
            { { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f } },
            { {  0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f } },
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
            { {  0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f } },
            { {  0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } }
        };

        auto vbRes = m_vertexBuffer.Initialize(
            device,
            sizeof(quadVertices),
            BufferType::Vertex,
            D3D12_HEAP_TYPE_UPLOAD
        );
        if (!vbRes) return std::unexpected("Failed to create Particle VertexBuffer: " + vbRes.error());

        m_vertexBuffer.UpdateData(std::span{ quadVertices, 6 });
        m_vbView.BufferLocation = m_vertexBuffer.GetGPUVirtualAddress();
        m_vbView.SizeInBytes = sizeof(quadVertices);
        m_vbView.StrideInBytes = sizeof(ParticleVertex);

        const usize instanceBufferSize = static_cast<usize>(MaxTotalParticles) * sizeof(ParticleInstance);
        for (u32 i = 0; i < NUM_FRAMES; ++i)
        {
            auto instRes = m_instanceBuffers[i].Initialize(
                device,
                instanceBufferSize,
                BufferType::Generic,
                D3D12_HEAP_TYPE_UPLOAD
            );
            if (!instRes) return std::unexpected("Failed to allocate Particle InstanceBuffer: " + instRes.error());

            auto cbRes = m_constantBuffers[i].Initialize(
                device,
                sizeof(DirectX::XMFLOAT4X4) * 2 + sizeof(DirectX::XMFLOAT4),
                BufferType::Constant,
                D3D12_HEAP_TYPE_UPLOAD
            );
            if (!cbRes) return std::unexpected("Failed to allocate Particle ConstantBuffer: " + cbRes.error());
        }

        ShaderCompileDesc vsDesc{};
        vsDesc.filePath = L"shaders/particles.hlsl";
        vsDesc.entryPoint = "VSMain";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.shaderModel = "6_5";

        auto vsBlob = shaderCompiler->CompileShader(vsDesc);
        if (!vsBlob) return std::unexpected("Failed to compile particle VS: " + vsBlob.error());

        ShaderCompileDesc psDesc{};
        psDesc.filePath = L"shaders/particles.hlsl";
        psDesc.entryPoint = "PSMain";
        psDesc.stage = ShaderStage::Pixel;
        psDesc.shaderModel = "6_5";

        auto psBlob = shaderCompiler->CompileShader(psDesc);
        if (!psBlob) return std::unexpected("Failed to compile particle PS: " + psBlob.error());

        const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        PipelineConfig pipeCfg{};
        pipeCfg.TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipeCfg.CullMode = D3D12_CULL_MODE_NONE;
        pipeCfg.FillMode = D3D12_FILL_MODE_SOLID;
        pipeCfg.RtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipeCfg.EnableDepth = true;
        pipeCfg.DepthWrite = false;
        pipeCfg.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        pipeCfg.DsvFormat = DXGI_FORMAT_D32_FLOAT;

        auto pipeRes = m_pipeline.Initialize(device, vsBlob->Get(), psBlob->Get(), inputLayout, pipeCfg);
        if (!pipeRes) return std::unexpected("Failed to create Particle Pipeline: " + pipeRes.error());

        m_isInitialized = true;
        return {};
    }

    void ParticleSystem::Update(Scene& scene, f32 dt)
    {
        if (dt <= 0.0f) return;

        static std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        std::uniform_real_distribution<float> distSigned(-1.0f, 1.0f);

        auto* emitterPool = scene.GetPool<ParticleEmitterComponent>();
        auto* trPool = scene.GetPool<TransformComponent>();

        if (emitterPool && trPool)
        {
            const auto emitters = emitterPool->GetData();
            const auto entities = emitterPool->GetEntities();

            for (usize e = 0; e < emitters.size(); ++e)
            {
                const auto& emit = emitters[e];
                if (!emit.IsActive) continue;

                const auto* tr = trPool->TryGet(entities[e]);
                if (!tr) continue;

                f32 safeRate = std::clamp(emit.EmissionRate, 0.0f, 50000.0f);
                f32 countToSpawn = safeRate * dt;
                int spawnInt = std::clamp(static_cast<int>(countToSpawn), 0, 5000);
                if (dist01(rng) < (countToSpawn - static_cast<f32>(spawnInt)))
                {
                    spawnInt = std::min(spawnInt + 1, 5000);
                }

                for (int s = 0; s < spawnInt; ++s)
                {
                    if (m_activeCount >= MaxTotalParticles) break;

                    Particle& p = m_particles[m_activeCount++];
                    p.Position = tr->Position;
                    p.Velocity = {
                        emit.InitialVelocity.x + distSigned(rng) * emit.VelocitySpread.x,
                        emit.InitialVelocity.y + distSigned(rng) * emit.VelocitySpread.y,
                        emit.InitialVelocity.z + distSigned(rng) * emit.VelocitySpread.z
                    };
                    p.Gravity = emit.Gravity;
                    p.StartColor = emit.StartColor;
                    p.EndColor = emit.EndColor;
                    p.Color = emit.StartColor;
                    p.StartSize = emit.StartSize;
                    p.EndSize = emit.EndSize;
                    p.Size = emit.StartSize;
                    p.MaxLife = emit.Lifetime;
                    p.Life = emit.Lifetime;
                }
            }
        }

        u32 i = 0;
        while (i < m_activeCount)
        {
            Particle& p = m_particles[i];
            p.Life -= dt;

            if (p.Life <= 0.0f)
            {
                m_particles[i] = m_particles[m_activeCount - 1];
                m_activeCount--;
                continue;
            }

            p.Velocity.x += p.Gravity.x * dt;
            p.Velocity.y += p.Gravity.y * dt;
            p.Velocity.z += p.Gravity.z * dt;

            p.Position.x += p.Velocity.x * dt;
            p.Position.y += p.Velocity.y * dt;
            p.Position.z += p.Velocity.z * dt;

            if (p.Position.y < 0.0f)
            {
                p.Position.y = 0.0f;
                p.Velocity.y = -p.Velocity.y * 0.45f;
                p.Velocity.x *= 0.8f;
                p.Velocity.z *= 0.8f;
            }

            const float maxLife = (std::max)(0.001f, p.MaxLife);
            const float t = std::clamp(1.0f - (p.Life / maxLife), 0.0f, 1.0f);
            p.Size = p.StartSize + (p.EndSize - p.StartSize) * t;
            p.Color = {
                p.StartColor.x + (p.EndColor.x - p.StartColor.x) * t,
                p.StartColor.y + (p.EndColor.y - p.StartColor.y) * t,
                p.StartColor.z + (p.EndColor.z - p.StartColor.z) * t,
                p.StartColor.w + (p.EndColor.w - p.StartColor.w) * t
            };

            ++i;
        }
    }

    void ParticleSystem::Render(ID3D12GraphicsCommandList* cmdList, const DirectX::XMMATRIX& viewProj, u32 frameIndex)
    {
        if (!m_isInitialized || m_activeCount == 0 || !cmdList) return;

        auto* mapped = static_cast<ParticleInstance*>(m_instanceBuffers[frameIndex].GetMappedData());
        if (!mapped) return;

        for (u32 i = 0; i < m_activeCount; ++i)
        {
            const auto& p = m_particles[i];
            mapped[i].PositionSize = { p.Position.x, p.Position.y, p.Position.z, p.Size };
            mapped[i].Color = p.Color;
        }

        struct alignas(256) ParticleCB
        {
            DirectX::XMFLOAT4X4 World;
            DirectX::XMFLOAT4X4 ViewProj;
            DirectX::XMFLOAT4   ColorMultiplier;
        };

        auto* cb = static_cast<ParticleCB*>(m_constantBuffers[frameIndex].GetMappedData());
        if (cb)
        {
            DirectX::XMStoreFloat4x4(&cb->World, DirectX::XMMatrixIdentity());
            DirectX::XMStoreFloat4x4(&cb->ViewProj, viewProj);
            cb->ColorMultiplier = { 1.0f, 1.0f, 1.0f, 1.0f };
        }

        m_pipeline.Bind(cmdList);
        cmdList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[frameIndex].GetGPUVirtualAddress());
        if (m_descriptorManager)
        {
            m_descriptorManager->SetGraphicsRootDescriptorTable(cmdList, 1, 0);
        }
        cmdList->SetGraphicsRootShaderResourceView(2, m_instanceBuffers[frameIndex].GetGPUVirtualAddress());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &m_vbView);
        cmdList->DrawInstanced(6, m_activeCount, 0, 0);
    }
}
