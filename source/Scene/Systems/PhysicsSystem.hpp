#pragma once

#include "Core/Types.hpp"
#include "Scene/Entity.hpp"
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include <optional>

namespace RL
{
    class Scene;

    struct RaycastHit
    {
        Entity HitEntity{};
        DirectX::XMFLOAT3 Point{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Normal{ 0.0f, 1.0f, 0.0f };
        f32 Distance = 0.0f;
    };

    class PhysicsSystem
    {
    public:
        PhysicsSystem() = default;
        ~PhysicsSystem() = default;

        void Step(Scene& scene, f32 dt);
        void SetGravity(const DirectX::XMFLOAT3& g) noexcept { m_gravity = g; }
        [[nodiscard]] const DirectX::XMFLOAT3& GetGravity() const noexcept { return m_gravity; }

        [[nodiscard]] std::optional<RaycastHit> Raycast(
            const Scene& scene,
            const DirectX::XMFLOAT3& origin,
            const DirectX::XMFLOAT3& direction,
            f32 maxDistance);

        void CaptureSnapshot(Scene& scene);
        void RestoreSnapshot(Scene& scene);
        void ClearSnapshot() noexcept;

    private:
        void Integrate(Scene& scene, f32 dt);

        static constexpr f32 FixedTimeStep = 1.0f / 60.0f;
        f32 m_accumulator = 0.0f;
        DirectX::XMFLOAT3 m_gravity{ 0.0f, -9.81f, 0.0f };
        std::unordered_map<u64, DirectX::XMFLOAT3> m_initialTransforms;
        std::unordered_map<u64, DirectX::XMFLOAT3> m_initialRotations;
    };
}
