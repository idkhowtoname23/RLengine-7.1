#include "Scene/Systems/PhysicsSystem.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"

#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace RL
{
    namespace
    {
        inline u64 HashGridCell(i32 cx, i32 cy, i32 cz) noexcept
        {
            const u64 h1 = static_cast<u64>(static_cast<u32>(cx)) * 73856093ull;
            const u64 h2 = static_cast<u64>(static_cast<u32>(cy)) * 19349663ull;
            const u64 h3 = static_cast<u64>(static_cast<u32>(cz)) * 83492791ull;
            return h1 ^ h2 ^ h3;
        }
    }
    void PhysicsSystem::ClearSnapshot() noexcept
    {
        m_initialTransforms.clear();
        m_initialRotations.clear();
        m_accumulator = 0.0f;
    }

    void PhysicsSystem::CaptureSnapshot(Scene& scene)
    {
        ClearSnapshot();
        auto* rbPool = scene.GetPool<RigidBodyComponent>();
        auto* trPool = scene.GetPool<TransformComponent>();
        if (!rbPool || !trPool) return;

        for (const auto& ent : scene.GetAliveEntities())
        {
            if (rbPool->Has(ent.GetIndex()))
            {
                if (const auto* tr = trPool->TryGet(ent.GetIndex()))
                {
                    m_initialTransforms[ent.GetID()] = tr->Position;
                    m_initialRotations[ent.GetID()] = tr->Rotation;
                }
            }
        }
    }

    void PhysicsSystem::RestoreSnapshot(Scene& scene)
    {
        auto* rbPool = scene.GetPool<RigidBodyComponent>();
        auto* trPool = scene.GetPool<TransformComponent>();
        if (!rbPool || !trPool) return;

        for (const auto& [entityID, pos] : m_initialTransforms)
        {
            Entity ent(entityID);
            if (!scene.IsValid(ent)) continue;

            if (auto* tr = trPool->TryGet(ent.GetIndex()))
            {
                tr->Position = pos;
            }
            if (auto* rb = rbPool->TryGet(ent.GetIndex()))
            {
                rb->LinearVelocity = { 0.0f, 0.0f, 0.0f };
                rb->AngularVelocity = { 0.0f, 0.0f, 0.0f };
                rb->IsGrounded = false;
            }
        }
        for (const auto& [entityID, rot] : m_initialRotations)
        {
            Entity ent(entityID);
            if (!scene.IsValid(ent)) continue;

            if (auto* tr = trPool->TryGet(ent.GetIndex()))
            {
                tr->Rotation = rot;
            }
        }
        m_accumulator = 0.0f;
    }

    void PhysicsSystem::Step(Scene& scene, f32 dt)
    {
        if (dt <= 0.0f) return;

        m_accumulator += dt;
        if (m_accumulator > 0.2f)
        {
            m_accumulator = 0.2f;
        }

        while (m_accumulator >= FixedTimeStep)
        {
            Integrate(scene, FixedTimeStep);
            m_accumulator -= FixedTimeStep;
        }
    }

    void PhysicsSystem::Integrate(Scene& scene, f32 dt)
    {
        auto* rbPool = scene.GetPool<RigidBodyComponent>();
        auto* trPool = scene.GetPool<TransformComponent>();
        if (!rbPool || !trPool) return;

        auto* boxPool = scene.GetPool<BoxColliderComponent>();
        auto* spherePool = scene.GetPool<SphereColliderComponent>();

        constexpr int kSubSteps = 2;
        const f32 subDt = dt / static_cast<f32>(kSubSteps);

        for (int step = 0; step < kSubSteps; ++step)
        {
            const auto entities = rbPool->GetEntities();
            auto rbData = rbPool->GetData();

            for (usize i = 0; i < entities.size(); ++i)
            {
                const u32 entityIdx = entities[i];
                auto& rb = rbData[i];
                if (rb.Type != BodyType::Dynamic) continue;

                auto* tr = trPool->TryGet(entityIdx);
                if (!tr) continue;

                if (rb.UseGravity)
                {
                    rb.LinearVelocity.y += m_gravity.y * rb.GravityScale * subDt;
                }

                rb.LinearVelocity.x *= std::max(0.0f, 1.0f - rb.LinearDamping * subDt);
                rb.LinearVelocity.y *= std::max(0.0f, 1.0f - rb.LinearDamping * subDt);
                rb.LinearVelocity.z *= std::max(0.0f, 1.0f - rb.LinearDamping * subDt);

                tr->Position.x += rb.LinearVelocity.x * subDt;
                tr->Position.y += rb.LinearVelocity.y * subDt;
                tr->Position.z += rb.LinearVelocity.z * subDt;

                tr->Rotation.x += rb.AngularVelocity.x * subDt;
                tr->Rotation.y += rb.AngularVelocity.y * subDt;
                tr->Rotation.z += rb.AngularVelocity.z * subDt;

                float bottomY = tr->Position.y - 0.5f * std::abs(tr->Scale.y);
                float radiusOrHalfY = 0.5f * std::abs(tr->Scale.y);

                if (boxPool && boxPool->Has(entityIdx))
                {
                    const auto& box = boxPool->Get(entityIdx);
                    radiusOrHalfY = box.HalfSize.y * std::abs(tr->Scale.y);
                    bottomY = (tr->Position.y + box.Offset.y) - radiusOrHalfY;
                }
                else if (spherePool && spherePool->Has(entityIdx))
                {
                    const auto& sphere = spherePool->Get(entityIdx);
                    radiusOrHalfY = sphere.Radius * std::max({ std::abs(tr->Scale.x), std::abs(tr->Scale.y), std::abs(tr->Scale.z) });
                    bottomY = (tr->Position.y + sphere.Offset.y) - radiusOrHalfY;
                }

                if (bottomY < 0.0f)
                {
                    tr->Position.y = radiusOrHalfY;
                    if (rb.LinearVelocity.y < 0.0f)
                    {
                        rb.LinearVelocity.y = -rb.LinearVelocity.y * rb.Restitution;
                        if (std::abs(rb.LinearVelocity.y) < 0.2f)
                        {
                            rb.LinearVelocity.y = 0.0f;
                            rb.IsGrounded = true;
                        }
                    }
                    rb.LinearVelocity.x *= std::max(0.0f, 1.0f - rb.Friction * subDt * 10.0f);
                    rb.LinearVelocity.z *= std::max(0.0f, 1.0f - rb.Friction * subDt * 10.0f);
                }
                else
                {
                    rb.IsGrounded = false;
                }
            }

            constexpr float kCellSize = 4.0f;
            constexpr float kInvCellSize = 1.0f / kCellSize;

            std::unordered_map<u64, std::vector<usize>> spatialGrid;
            spatialGrid.reserve(entities.size());

            for (usize i = 0; i < entities.size(); ++i)
            {
                const u32 e = entities[i];
                auto* tr = trPool->TryGet(e);
                if (!tr) continue;

                const i32 cx = static_cast<i32>(std::floor(tr->Position.x * kInvCellSize));
                const i32 cy = static_cast<i32>(std::floor(tr->Position.y * kInvCellSize));
                const i32 cz = static_cast<i32>(std::floor(tr->Position.z * kInvCellSize));
                spatialGrid[HashGridCell(cx, cy, cz)].push_back(i);
            }

            for (usize i = 0; i < entities.size(); ++i)
            {
                const u32 e1 = entities[i];
                auto& rb1 = rbData[i];
                auto* tr1 = trPool->TryGet(e1);
                if (!tr1) continue;

                const i32 baseCx = static_cast<i32>(std::floor(tr1->Position.x * kInvCellSize));
                const i32 baseCy = static_cast<i32>(std::floor(tr1->Position.y * kInvCellSize));
                const i32 baseCz = static_cast<i32>(std::floor(tr1->Position.z * kInvCellSize));

                for (i32 dz = -1; dz <= 1; ++dz)
                {
                    for (i32 dy = -1; dy <= 1; ++dy)
                    {
                        for (i32 dx = -1; dx <= 1; ++dx)
                        {
                            const u64 cellKey = HashGridCell(baseCx + dx, baseCy + dy, baseCz + dz);
                            auto it = spatialGrid.find(cellKey);
                            if (it == spatialGrid.end()) continue;

                            for (usize j : it->second)
                            {
                                if (j <= i) continue;

                                const u32 e2 = entities[j];
                                auto& rb2 = rbData[j];
                                if (rb1.Type != BodyType::Dynamic && rb2.Type != BodyType::Dynamic) continue;

                                auto* tr2 = trPool->TryGet(e2);
                                if (!tr2) continue;

                                float r1 = 0.5f * std::max({ std::abs(tr1->Scale.x), std::abs(tr1->Scale.y), std::abs(tr1->Scale.z) });
                                float r2 = 0.5f * std::max({ std::abs(tr2->Scale.x), std::abs(tr2->Scale.y), std::abs(tr2->Scale.z) });

                                float distDiffX = tr2->Position.x - tr1->Position.x;
                                float distDiffY = tr2->Position.y - tr1->Position.y;
                                float distDiffZ = tr2->Position.z - tr1->Position.z;
                                float distSq = distDiffX * distDiffX + distDiffY * distDiffY + distDiffZ * distDiffZ;
                                float minDist = r1 + r2;

                                if (distSq < minDist * minDist && distSq > 1e-6f)
                                {
                                    float dist = std::sqrt(distSq);
                                    float nx = distDiffX / dist;
                                    float ny = distDiffY / dist;
                                    float nz = distDiffZ / dist;
                                    float penetration = minDist - dist;

                                    if (rb1.Type == BodyType::Dynamic && rb2.Type == BodyType::Dynamic)
                                    {
                                        tr1->Position.x -= nx * penetration * 0.5f;
                                        tr1->Position.y -= ny * penetration * 0.5f;
                                        tr1->Position.z -= nz * penetration * 0.5f;

                                        tr2->Position.x += nx * penetration * 0.5f;
                                        tr2->Position.y += ny * penetration * 0.5f;
                                        tr2->Position.z += nz * penetration * 0.5f;

                                        float rvx = rb2.LinearVelocity.x - rb1.LinearVelocity.x;
                                        float rvy = rb2.LinearVelocity.y - rb1.LinearVelocity.y;
                                        float rvz = rb2.LinearVelocity.z - rb1.LinearVelocity.z;
                                        float velAlongNormal = rvx * nx + rvy * ny + rvz * nz;

                                        if (velAlongNormal < 0.0f)
                                        {
                                            float e = (std::min)(rb1.Restitution, rb2.Restitution);
                                            const float m1 = (std::max)(0.001f, rb1.Mass);
                                            const float m2 = (std::max)(0.001f, rb2.Mass);
                                            const float invMass1 = (rb1.Type == BodyType::Dynamic) ? (1.0f / m1) : 0.0f;
                                            const float invMass2 = (rb2.Type == BodyType::Dynamic) ? (1.0f / m2) : 0.0f;
                                            const float invMassSum = invMass1 + invMass2;

                                            if (invMassSum > 0.0001f)
                                            {
                                                const float impulseMagnitude = -(1.0f + e) * velAlongNormal / invMassSum;
                                                if (rb1.Type == BodyType::Dynamic)
                                                {
                                                    rb1.LinearVelocity.x -= (impulseMagnitude * invMass1) * nx;
                                                    rb1.LinearVelocity.y -= (impulseMagnitude * invMass1) * ny;
                                                    rb1.LinearVelocity.z -= (impulseMagnitude * invMass1) * nz;
                                                }
                                                if (rb2.Type == BodyType::Dynamic)
                                                {
                                                    rb2.LinearVelocity.x += (impulseMagnitude * invMass2) * nx;
                                                    rb2.LinearVelocity.y += (impulseMagnitude * invMass2) * ny;
                                                    rb2.LinearVelocity.z += (impulseMagnitude * invMass2) * nz;
                                                }
                                            }
                                        }
                                    }
                                    else if (rb1.Type == BodyType::Dynamic)
                                    {
                                        tr1->Position.x -= nx * penetration;
                                        tr1->Position.y -= ny * penetration;
                                        tr1->Position.z -= nz * penetration;

                                        float velAlongNormal = rb1.LinearVelocity.x * nx + rb1.LinearVelocity.y * ny + rb1.LinearVelocity.z * nz;
                                        if (velAlongNormal > 0.0f)
                                        {
                                            rb1.LinearVelocity.x -= (1.0f + rb1.Restitution) * velAlongNormal * nx;
                                            rb1.LinearVelocity.y -= (1.0f + rb1.Restitution) * velAlongNormal * ny;
                                            rb1.LinearVelocity.z -= (1.0f + rb1.Restitution) * velAlongNormal * nz;
                                        }
                                    }
                                    else if (rb2.Type == BodyType::Dynamic)
                                    {
                                        tr2->Position.x += nx * penetration;
                                        tr2->Position.y += ny * penetration;
                                        tr2->Position.z += nz * penetration;

                                        float velAlongNormal = rb2.LinearVelocity.x * (-nx) + rb2.LinearVelocity.y * (-ny) + rb2.LinearVelocity.z * (-nz);
                                        if (velAlongNormal > 0.0f)
                                        {
                                            rb2.LinearVelocity.x += (1.0f + rb2.Restitution) * velAlongNormal * nx;
                                            rb2.LinearVelocity.y += (1.0f + rb2.Restitution) * velAlongNormal * ny;
                                            rb2.LinearVelocity.z += (1.0f + rb2.Restitution) * velAlongNormal * nz;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::optional<RaycastHit> PhysicsSystem::Raycast(
        const Scene& scene,
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        f32 maxDistance)
    {
        const auto* trPool = scene.GetPool<TransformComponent>();
        if (!trPool) return std::nullopt;

        const auto* boxPool = scene.GetPool<BoxColliderComponent>();
        const auto* spherePool = scene.GetPool<SphereColliderComponent>();

        float dirLen = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        if (dirLen < 1e-6f) return std::nullopt;

        const float dx = direction.x / dirLen;
        const float dy = direction.y / dirLen;
        const float dz = direction.z / dirLen;

        float closestDist = maxDistance;
        std::optional<RaycastHit> hit;

        for (const auto& ent : scene.GetAliveEntities())
        {
            const auto* tr = trPool->TryGet(ent.GetIndex());
            if (!tr) continue;

            const auto* box = boxPool ? boxPool->TryGet(ent.GetIndex()) : nullptr;
            const auto* sphere = spherePool ? spherePool->TryGet(ent.GetIndex()) : nullptr;

            if (sphere && !box)
            {
                const float cx = tr->Position.x + sphere->Offset.x * tr->Scale.x;
                const float cy = tr->Position.y + sphere->Offset.y * tr->Scale.y;
                const float cz = tr->Position.z + sphere->Offset.z * tr->Scale.z;
                const float maxScale = (std::max)({ std::abs(tr->Scale.x), std::abs(tr->Scale.y), std::abs(tr->Scale.z) });
                const float r = (std::max)(sphere->Radius * maxScale, 1e-4f);

                const float ox = origin.x - cx;
                const float oy = origin.y - cy;
                const float oz = origin.z - cz;

                const float b = ox * dx + oy * dy + oz * dz;
                const float c = (ox * ox + oy * oy + oz * oz) - (r * r);

                if (c > 0.0f && b > 0.0f) continue;

                const float discriminant = b * b - c;
                if (discriminant < 0.0f) continue;

                float t = -b - std::sqrt(discriminant);
                if (t < 0.0f) t = 0.0f;

                if (t < closestDist)
                {
                    closestDist = t;
                    RaycastHit h;
                    h.HitEntity = ent;
                    h.Point = { origin.x + dx * t, origin.y + dy * t, origin.z + dz * t };
                    float nx = (h.Point.x - cx) / r;
                    float ny = (h.Point.y - cy) / r;
                    float nz = (h.Point.z - cz) / r;
                    float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (nlen > 1e-4f)
                    {
                        h.Normal = { nx / nlen, ny / nlen, nz / nlen };
                    }
                    else
                    {
                        h.Normal = { -dx, -dy, -dz };
                    }
                    h.Distance = t;
                    hit = h;
                }
            }
            else
            {
                float cx = tr->Position.x;
                float cy = tr->Position.y;
                float cz = tr->Position.z;
                float hx = 0.5f * std::abs(tr->Scale.x);
                float hy = 0.5f * std::abs(tr->Scale.y);
                float hz = 0.5f * std::abs(tr->Scale.z);

                if (box)
                {
                    cx = tr->Position.x + box->Offset.x * tr->Scale.x;
                    cy = tr->Position.y + box->Offset.y * tr->Scale.y;
                    cz = tr->Position.z + box->Offset.z * tr->Scale.z;
                    hx = std::abs(box->HalfSize.x * tr->Scale.x);
                    hy = std::abs(box->HalfSize.y * tr->Scale.y);
                    hz = std::abs(box->HalfSize.z * tr->Scale.z);
                }

                hx = (std::max)(hx, 1e-4f);
                hy = (std::max)(hy, 1e-4f);
                hz = (std::max)(hz, 1e-4f);

                const float minX = cx - hx;
                const float maxX = cx + hx;
                const float minY = cy - hy;
                const float maxY = cy + hy;
                const float minZ = cz - hz;
                const float maxZ = cz + hz;

                float tNear = -1e30f;
                float tFar = 1e30f;
                DirectX::XMFLOAT3 nearNormal{ 0.0f, 1.0f, 0.0f };

                bool missed = false;

                if (std::abs(dx) > 1e-6f)
                {
                    float t1 = (minX - origin.x) / dx;
                    float t2 = (maxX - origin.x) / dx;
                    DirectX::XMFLOAT3 n{ -1.0f, 0.0f, 0.0f };
                    if (t1 > t2)
                    {
                        std::swap(t1, t2);
                        n = { 1.0f, 0.0f, 0.0f };
                    }
                    if (t1 > tNear)
                    {
                        tNear = t1;
                        nearNormal = n;
                    }
                    if (t2 < tFar)
                    {
                        tFar = t2;
                    }
                }
                else if (origin.x < minX || origin.x > maxX)
                {
                    missed = true;
                }

                if (!missed)
                {
                    if (std::abs(dy) > 1e-6f)
                    {
                        float t1 = (minY - origin.y) / dy;
                        float t2 = (maxY - origin.y) / dy;
                        DirectX::XMFLOAT3 n{ 0.0f, -1.0f, 0.0f };
                        if (t1 > t2)
                        {
                            std::swap(t1, t2);
                            n = { 0.0f, 1.0f, 0.0f };
                        }
                        if (t1 > tNear)
                        {
                            tNear = t1;
                            nearNormal = n;
                        }
                        if (t2 < tFar)
                        {
                            tFar = t2;
                        }
                    }
                    else if (origin.y < minY || origin.y > maxY)
                    {
                        missed = true;
                    }
                }

                if (!missed)
                {
                    if (std::abs(dz) > 1e-6f)
                    {
                        float t1 = (minZ - origin.z) / dz;
                        float t2 = (maxZ - origin.z) / dz;
                        DirectX::XMFLOAT3 n{ 0.0f, 0.0f, -1.0f };
                        if (t1 > t2)
                        {
                            std::swap(t1, t2);
                            n = { 0.0f, 0.0f, 1.0f };
                        }
                        if (t1 > tNear)
                        {
                            tNear = t1;
                            nearNormal = n;
                        }
                        if (t2 < tFar)
                        {
                            tFar = t2;
                        }
                    }
                    else if (origin.z < minZ || origin.z > maxZ)
                    {
                        missed = true;
                    }
                }

                if (missed) continue;

                if (tNear <= tFar && tFar >= 0.0f && tNear < closestDist)
                {
                    const float t = (tNear >= 0.0f) ? tNear : 0.0f;
                    closestDist = t;
                    RaycastHit h;
                    h.HitEntity = ent;
                    h.Point = { origin.x + dx * t, origin.y + dy * t, origin.z + dz * t };
                    h.Normal = nearNormal;
                    h.Distance = t;
                    hit = h;
                }
            }
        }

        return hit;
    }
}
