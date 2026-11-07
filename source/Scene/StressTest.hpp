#pragma once

#include "Core/Types.hpp"
#include "Scene/Entity.hpp"

#include <vector>
#include <deque>

namespace RL
{
    class Scene;

    class StressTest
    {
    public:
        StressTest() = default;
        ~StressTest() = default;

        void Spawn(Scene& scene, u32 count, u32 textureCount = 3);
        void Update(Scene& scene, f32 dt);
        void Clear(Scene& scene);

        [[nodiscard]] bool IsActive() const noexcept { return !m_spawnedEntities.empty(); }
        [[nodiscard]] u32 GetEntityCount() const noexcept { return static_cast<u32>(m_spawnedEntities.size()); }

        void RecordFrame(f32 fps, f32 frameTimeMs);

        [[nodiscard]] f32 GetAverageFPS() const noexcept;
        [[nodiscard]] f32 GetMinFPS() const noexcept { return m_minFps; }
        [[nodiscard]] f32 GetMaxFPS() const noexcept { return m_maxFps; }
        [[nodiscard]] const std::deque<f32>& GetFpsHistory() const noexcept { return m_fpsHistory; }
        [[nodiscard]] const std::deque<f32>& GetFrameTimeHistory() const noexcept { return m_frameTimeHistory; }

    private:
        struct EntitySimData
        {
            f32 Radius = 1.0f;
            f32 BaseAngle = 0.0f;
            f32 AngularSpeed = 1.0f;
            f32 HeightOffset = 0.0f;
            f32 PulseFreq = 2.0f;
            f32 BaseScale = 0.25f;
        };

        std::vector<Entity> m_spawnedEntities;
        std::vector<EntitySimData> m_simData;

        f32 m_simTime = 0.0f;
        f32 m_minFps = 9999.0f;
        f32 m_maxFps = 0.0f;
        std::deque<f32> m_fpsHistory;
        std::deque<f32> m_frameTimeHistory;
        static constexpr usize MaxHistoryLength = 120;
    };
}
