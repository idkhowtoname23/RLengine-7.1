#include "Scene/StressTest.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"
#include "Core/Logger.hpp"
#include "Core/Threading/ThreadPool.hpp"

#include <cmath>
#include <numbers>
#include <numeric>
#include <thread>
#include <vector>
#include <algorithm>

namespace RL
{
    void StressTest::Spawn(Scene& scene, u32 count, u32 textureCount)
    {
        Clear(scene);

        if (scene.GetCamera().GetProjectionType() == CameraProjection::Orthographic)
        {
            scene.GetCamera().SetProjectionType(CameraProjection::Perspective);
            scene.GetCamera().SetPosition({ 0.0f, 0.0f, -30.0f });
            scene.GetCamera().SetRotation({ 0.0f, 0.0f, 0.0f });
            scene.GetCamera().SetFOV(DirectX::XMConvertToRadians(45.0f));
        }

        scene.Reserve(count);
        m_spawnedEntities.reserve(count);
        m_simData.reserve(count);

        const f32 pi = std::numbers::pi_v<f32>;
        const u32 safeTexCount = (textureCount > 0) ? textureCount : 1;

        for (u32 i = 0; i < count; ++i)
        {
            Entity entity = scene.CreateEntity();
            if (!entity.IsValid())
            {
                break;
            }

            const f32 normalizedIdx = static_cast<f32>(i) / static_cast<f32>(count);
            const f32 radius = 2.0f + std::sqrt(static_cast<f32>(i)) * 0.45f;
            const f32 angle = normalizedIdx * 40.0f * pi;
            const f32 speed = 0.3f + (1.0f / (radius * 0.2f + 1.0f)) * 1.5f;
            const f32 scale = 0.15f + (static_cast<f32>(i % 10) * 0.02f);

            EntitySimData sim{};
            sim.Radius = radius;
            sim.BaseAngle = angle;
            sim.AngularSpeed = (i % 2 == 0) ? speed : -speed;
            sim.HeightOffset = std::sin(angle * 3.0f) * 0.5f;
            sim.PulseFreq = 1.5f + static_cast<f32>(i % 7) * 0.5f;
            sim.BaseScale = scale;

            auto& transform = scene.GetComponent<TransformComponent>(entity);
            transform.Position = {
                radius * std::cos(angle),
                radius * std::sin(angle),
                sim.HeightOffset
            };
            transform.Scale = { scale, scale, 1.0f };
            transform.Rotation = { 0.0f, 0.0f, angle };

            auto& quad = scene.AddComponent<QuadRenderComponent>(entity);
            quad.TextureIndex = (i % safeTexCount);

            const f32 r = 0.5f + 0.5f * std::sin(angle);
            const f32 g = 0.5f + 0.5f * std::sin(angle + 2.0f);
            const f32 b = 0.5f + 0.5f * std::sin(angle + 4.0f);
            quad.Color = { r, g, b, 0.95f };

            m_spawnedEntities.push_back(entity);
            m_simData.push_back(sim);
        }

        m_simTime = 0.0f;
        m_minFps = 9999.0f;
        m_maxFps = 0.0f;
        m_fpsHistory.clear();
        m_frameTimeHistory.clear();

        RL_LOG_INFO("[StressTest] Spawned {} entities into active scene.", m_spawnedEntities.size());
    }

    void StressTest::Update(Scene& scene, f32 dt)
    {
        if (m_spawnedEntities.empty())
        {
            return;
        }

        m_simTime += dt;

        auto* transformPool = scene.GetPool<TransformComponent>();
        if (!transformPool)
        {
            return;
        }

        const usize count = m_spawnedEntities.size();
        ThreadPool::Get().ParallelFor(count, 1024, [this, transformPool](usize s, usize e) {
            for (usize i = s; i < e; ++i)
            {
                const Entity entity = m_spawnedEntities[i];
                auto* transform = transformPool->TryGet(entity.GetIndex());
                if (!transform)
                {
                    continue;
                }

                const auto& sim = m_simData[i];
                const f32 currentAngle = sim.BaseAngle + m_simTime * sim.AngularSpeed;
                const f32 sinPulse = std::sin(m_simTime * sim.PulseFreq);

                transform->Position.x = sim.Radius * std::cos(currentAngle);
                transform->Position.y = sim.Radius * std::sin(currentAngle);
                transform->Position.z = sim.HeightOffset + sinPulse * 0.1f;

                transform->Rotation.z = currentAngle;

                const f32 pulse = 1.0f + 0.25f * sinPulse;
                transform->Scale.x = sim.BaseScale * pulse;
                transform->Scale.y = sim.BaseScale * pulse;
            }
        });
    }

    void StressTest::Clear(Scene& scene)
    {
        if (m_spawnedEntities.empty())
        {
            return;
        }

        const usize count = m_spawnedEntities.size();
        if (count == scene.GetAliveEntityCount())
        {
            scene.Clear();
        }
        else
        {
            for (Entity entity : m_spawnedEntities)
            {
                scene.DestroyEntity(entity);
            }
        }

        m_spawnedEntities.clear();
        m_simData.clear();
        m_simTime = 0.0f;

        RL_LOG_INFO("[StressTest] Cleared {} stress-test entities from scene.", count);
    }

    void StressTest::RecordFrame(f32 fps, f32 frameTimeMs)
    {
        if (m_fpsHistory.size() >= MaxHistoryLength)
        {
            m_fpsHistory.pop_front();
            m_frameTimeHistory.pop_front();
        }

        m_fpsHistory.push_back(fps);
        m_frameTimeHistory.push_back(frameTimeMs);

        if (fps < m_minFps) m_minFps = fps;
        if (fps > m_maxFps) m_maxFps = fps;
    }

    f32 StressTest::GetAverageFPS() const noexcept
    {
        if (m_fpsHistory.empty()) return 0.0f;
        const f32 sum = std::accumulate(m_fpsHistory.begin(), m_fpsHistory.end(), 0.0f);
        return sum / static_cast<f32>(m_fpsHistory.size());
    }
}
