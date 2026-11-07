#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"
#include "Scene/Entity.hpp"
#include "Scene/Components.hpp"
#include "Scene/Camera.hpp"

#include <vector>
#include <memory>
#include <span>
#include <string_view>
#include <algorithm>
#include <typeindex>

struct ID3D12GraphicsCommandList;

namespace RL
{
    class RenderSystem;
    class ParticleSystem;
    class PhysicsSystem;
    class AudioSystem;

    class IComponentPool
    {
    public:
        virtual ~IComponentPool() = default;
        virtual void Remove(u32 entityIndex) = 0;
        virtual bool Has(u32 entityIndex) const noexcept = 0;
        virtual void Clear() = 0;
    };

    template<typename T>
    class ComponentPool final : public IComponentPool
    {
    public:
        static constexpr u32 InvalidIndex = 0xFFFFFFFF;

        ComponentPool() = default;
        ~ComponentPool() override = default;

        ComponentPool(const ComponentPool&) = delete;
        ComponentPool& operator=(const ComponentPool&) = delete;
        ComponentPool(ComponentPool&&) noexcept = default;
        ComponentPool& operator=(ComponentPool&&) noexcept = default;

        template<typename... Args>
        T& Emplace(u32 entityIndex, Args&&... args)
        {
            if (entityIndex >= m_sparse.size())
            {
                m_sparse.resize(entityIndex + 1, InvalidIndex);
            }

            if (m_sparse[entityIndex] != InvalidIndex)
            {
                const u32 denseIdx = m_sparse[entityIndex];
                m_denseComponents[denseIdx] = T{ std::forward<Args>(args)... };
                return m_denseComponents[denseIdx];
            }

            const u32 denseIdx = static_cast<u32>(m_denseComponents.size());
            m_denseComponents.emplace_back(std::forward<Args>(args)...);
            m_denseEntities.push_back(entityIndex);
            m_sparse[entityIndex] = denseIdx;
            return m_denseComponents.back();
        }

        void Remove(u32 entityIndex) override
        {
            if (!Has(entityIndex))
            {
                return;
            }

            const u32 denseIdx = m_sparse[entityIndex];
            const u32 lastDenseIdx = static_cast<u32>(m_denseComponents.size() - 1);
            const u32 lastEntityIndex = m_denseEntities[lastDenseIdx];

            if (denseIdx != lastDenseIdx)
            {
                m_denseComponents[denseIdx] = std::move(m_denseComponents[lastDenseIdx]);
                m_denseEntities[denseIdx] = lastEntityIndex;
                m_sparse[lastEntityIndex] = denseIdx;
            }

            m_denseComponents.pop_back();
            m_denseEntities.pop_back();
            m_sparse[entityIndex] = InvalidIndex;
        }

        [[nodiscard]] bool Has(u32 entityIndex) const noexcept override
        {
            return entityIndex < m_sparse.size() && m_sparse[entityIndex] != InvalidIndex;
        }

        [[nodiscard]] T* TryGet(u32 entityIndex) noexcept
        {
            if (entityIndex >= m_sparse.size() || m_sparse[entityIndex] == InvalidIndex)
            {
                return nullptr;
            }
            return &m_denseComponents[m_sparse[entityIndex]];
        }

        [[nodiscard]] const T* TryGet(u32 entityIndex) const noexcept
        {
            if (entityIndex >= m_sparse.size() || m_sparse[entityIndex] == InvalidIndex)
            {
                return nullptr;
            }
            return &m_denseComponents[m_sparse[entityIndex]];
        }

        [[nodiscard]] T& Get(u32 entityIndex)
        {
            const u32 denseIdx = m_sparse[entityIndex];
            return m_denseComponents[denseIdx];
        }

        [[nodiscard]] const T& Get(u32 entityIndex) const
        {
            const u32 denseIdx = m_sparse[entityIndex];
            return m_denseComponents[denseIdx];
        }

        void Clear() override
        {
            m_denseComponents.clear();
            m_denseEntities.clear();
            std::fill(m_sparse.begin(), m_sparse.end(), InvalidIndex);
        }

        void Reserve(usize capacity)
        {
            m_denseComponents.reserve(capacity);
            m_denseEntities.reserve(capacity);
            m_sparse.reserve(capacity);
        }

        [[nodiscard]] std::span<T> GetData() noexcept { return m_denseComponents; }
        [[nodiscard]] std::span<const T> GetData() const noexcept { return m_denseComponents; }
        [[nodiscard]] std::span<const u32> GetEntities() const noexcept { return m_denseEntities; }
        [[nodiscard]] usize Size() const noexcept { return m_denseComponents.size(); }
        [[nodiscard]] bool Empty() const noexcept { return m_denseComponents.empty(); }

    private:
        std::vector<T> m_denseComponents;
        std::vector<u32> m_denseEntities;
        std::vector<u32> m_sparse;
    };

    namespace Detail
    {
        inline u32 GetUniqueComponentTypeID() noexcept
        {
            static u32 counter = 0;
            return counter++;
        }

        template<typename T>
        inline u32 GetComponentTypeID() noexcept
        {
            static const u32 id = GetUniqueComponentTypeID();
            return id;
        }
    }

    class Scene
    {
    public:
        Scene();
        ~Scene();

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;

        [[nodiscard]] Entity CreateEntity(std::string_view name = "Entity");
        [[nodiscard]] Entity CreateEntityWithUUID(UUID id, std::string_view name = "Entity");
        void DestroyEntity(Entity entity);
        [[nodiscard]] bool IsValid(Entity entity) const noexcept;
        [[nodiscard]] bool IsAlive(Entity entity) const noexcept { return IsValid(entity); }
        template<typename Func> void ForEachEntity(Func&& fn) { auto list = m_aliveEntities; for (Entity e : list) { if (IsValid(e)) fn(e); } }
        void Clear();

        void SetParent(Entity child, Entity parent);
        [[nodiscard]] Entity GetParent(Entity entity) const noexcept;
        [[nodiscard]] const std::vector<Entity>& GetChildren(Entity entity) const noexcept;
        [[nodiscard]] bool IsAncestor(Entity ancestor, Entity descendant) const noexcept;
        [[nodiscard]] DirectX::XMMATRIX GetWorldMatrix(Entity entity) const noexcept;
        [[nodiscard]] Entity GetEntityFromIndex(u32 index) const noexcept;

        template<typename T, typename... Args>
        T& AddComponent(Entity entity, Args&&... args)
        {
            if (!IsValid(entity))
            {
                RL_LOG_FATAL("Cannot add component: entity is invalid or dead!");
                std::terminate();
            }
            auto& pool = GetOrCreatePool<T>();
            return pool.Emplace(entity.GetIndex(), std::forward<Args>(args)...);
        }

        template<typename T>
        [[nodiscard]] T& GetComponent(Entity entity)
        {
            auto* comp = TryGetComponent<T>(entity);
            if (!comp)
            {
                RL_LOG_FATAL("Cannot get component: entity is invalid or component does not exist!");
                std::terminate();
            }
            return *comp;
        }

        template<typename T>
        [[nodiscard]] const T& GetComponent(Entity entity) const
        {
            const auto* comp = TryGetComponent<T>(entity);
            if (!comp)
            {
                RL_LOG_FATAL("Cannot get component: entity is invalid or component does not exist!");
                std::terminate();
            }
            return *comp;
        }

        template<typename T>
        [[nodiscard]] T* TryGetComponent(Entity entity) noexcept
        {
            if (!IsValid(entity)) return nullptr;
            auto* pool = GetPool<T>();
            if (!pool || !pool->Has(entity.GetIndex())) return nullptr;
            return &pool->Get(entity.GetIndex());
        }

        template<typename T>
        [[nodiscard]] const T* TryGetComponent(Entity entity) const noexcept
        {
            if (!IsValid(entity)) return nullptr;
            const auto* pool = GetPool<T>();
            if (!pool || !pool->Has(entity.GetIndex())) return nullptr;
            return &pool->Get(entity.GetIndex());
        }

        template<typename T>
        [[nodiscard]] bool HasComponent(Entity entity) const noexcept
        {
            if (!IsValid(entity)) return false;
            const auto* pool = GetPool<T>();
            return pool && pool->Has(entity.GetIndex());
        }

        template<typename T>
        void RemoveComponent(Entity entity)
        {
            if (!IsValid(entity)) return;
            auto* pool = GetPool<T>();
            if (pool)
            {
                pool->Remove(entity.GetIndex());
            }
        }

        template<typename T>
        [[nodiscard]] ComponentPool<T>* GetPool() noexcept
        {
            const u32 typeId = Detail::GetComponentTypeID<T>();
            if (typeId >= m_pools.size() || !m_pools[typeId])
            {
                return nullptr;
            }
            return static_cast<ComponentPool<T>*>(m_pools[typeId].get());
        }

        template<typename T>
        [[nodiscard]] const ComponentPool<T>* GetPool() const noexcept
        {
            const u32 typeId = Detail::GetComponentTypeID<T>();
            if (typeId >= m_pools.size() || !m_pools[typeId])
            {
                return nullptr;
            }
            return static_cast<const ComponentPool<T>*>(m_pools[typeId].get());
        }

        [[nodiscard]] usize GetAliveEntityCount() const noexcept { return m_aliveEntityCount; }
        [[nodiscard]] std::span<const Entity> GetAliveEntities() const noexcept { return m_aliveEntities; }

        void Reserve(usize capacity)
        {
            m_entityGenerations.reserve(capacity);
            m_aliveEntities.reserve(capacity);
            m_entityToAliveIndex.reserve(capacity);
            GetOrCreatePool<TransformComponent>().Reserve(capacity);
            GetOrCreatePool<QuadRenderComponent>().Reserve(capacity);
            GetOrCreatePool<TagComponent>().Reserve(capacity);
            GetOrCreatePool<RelationshipComponent>().Reserve(capacity);
        }

        void Update(f32 dt);
        void Render(ID3D12GraphicsCommandList* cmdList, u32 frameIndex);

        void SetRenderSystem(RenderSystem* renderSystem) noexcept { m_renderSystem = renderSystem; }
        [[nodiscard]] RenderSystem* GetRenderSystem() const noexcept { return m_renderSystem; }

        void SetParticleSystem(ParticleSystem* particleSystem) noexcept { m_particleSystem = particleSystem; }
        [[nodiscard]] ParticleSystem* GetParticleSystem() const noexcept { return m_particleSystem; }

        void SetPhysicsSystem(PhysicsSystem* physicsSystem) noexcept { m_physicsSystem = physicsSystem; }
        [[nodiscard]] PhysicsSystem* GetPhysicsSystem() const noexcept { return m_physicsSystem; }

        void SetAudioSystem(AudioSystem* audioSystem) noexcept { m_audioSystem = audioSystem; }
        [[nodiscard]] AudioSystem* GetAudioSystem() const noexcept { return m_audioSystem; }

        [[nodiscard]] Camera& GetCamera() noexcept { return m_camera; }
        [[nodiscard]] const Camera& GetCamera() const noexcept { return m_camera; }

    private:
        template<typename T>
        ComponentPool<T>& GetOrCreatePool()
        {
            const u32 typeId = Detail::GetComponentTypeID<T>();
            if (typeId >= m_pools.size())
            {
                m_pools.resize(typeId + 1);
            }
            if (!m_pools[typeId])
            {
                m_pools[typeId] = std::make_unique<ComponentPool<T>>();
            }
            return static_cast<ComponentPool<T>&>(*m_pools[typeId]);
        }

    private:
        std::vector<u32> m_entityGenerations;
        std::vector<u32> m_freeIndices;
        std::vector<std::unique_ptr<IComponentPool>> m_pools;
        std::vector<Entity> m_aliveEntities;
        std::vector<u32> m_entityToAliveIndex;
        usize m_aliveEntityCount = 0;

        Camera m_camera;
        RenderSystem* m_renderSystem = nullptr;
        ParticleSystem* m_particleSystem = nullptr;
        PhysicsSystem* m_physicsSystem = nullptr;
        AudioSystem* m_audioSystem = nullptr;
    };
}
