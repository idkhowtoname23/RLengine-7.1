#include "Scene/Scene.hpp"
#include "Scene/Systems/RenderSystem.hpp"
#include "Scene/Systems/PhysicsSystem.hpp"
#include "Graphics/DX12/ParticleSystem.hpp"
#include "Audio/AudioSystem.hpp"

namespace RL
{
    Scene::Scene() = default;
    Scene::~Scene() = default;

    Entity Scene::CreateEntity(std::string_view name)
    {
        return CreateEntityWithUUID(UUID::Generate(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID id, std::string_view name)
    {
        u32 index = 0;
        u32 generation = 1;

        if (!m_freeIndices.empty())
        {
            index = m_freeIndices.back();
            m_freeIndices.pop_back();
            generation = m_entityGenerations[index];
        }
        else
        {
            index = static_cast<u32>(m_entityGenerations.size());
            m_entityGenerations.push_back(generation);
        }

        m_aliveEntityCount++;
        Entity entity(index, generation);

        m_aliveEntities.push_back(entity);
        if (index >= m_entityToAliveIndex.size())
        {
            m_entityToAliveIndex.resize(index + 1, 0xFFFFFFFF);
        }
        m_entityToAliveIndex[index] = static_cast<u32>(m_aliveEntities.size() - 1);

        AddComponent<IDComponent>(entity, id);
        AddComponent<TagComponent>(entity, std::string(name));
        AddComponent<TransformComponent>(entity);
        AddComponent<RelationshipComponent>(entity);

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!IsValid(entity))
        {
            return;
        }

        if (HasComponent<RelationshipComponent>(entity))
        {
            auto& rel = GetComponent<RelationshipComponent>(entity);
            if (rel.Parent.IsValid() && IsValid(rel.Parent) && HasComponent<RelationshipComponent>(rel.Parent))
            {
                auto& parentRel = GetComponent<RelationshipComponent>(rel.Parent);
                auto it = std::remove(parentRel.Children.begin(), parentRel.Children.end(), entity);
                parentRel.Children.erase(it, parentRel.Children.end());
                rel.Parent = Entity{};
            }

            const auto children = std::move(rel.Children);
            rel.Children.clear();
            for (Entity child : children)
            {
                if (IsValid(child))
                {
                    DestroyEntity(child);
                }
            }
        }

        const u32 index = entity.GetIndex();

        for (auto& pool : m_pools)
        {
            if (pool)
            {
                pool->Remove(index);
            }
        }

        if (index < m_entityToAliveIndex.size())
        {
            const u32 aliveIdx = m_entityToAliveIndex[index];
            if (aliveIdx != 0xFFFFFFFF && aliveIdx < m_aliveEntities.size())
            {
                const u32 lastIdx = static_cast<u32>(m_aliveEntities.size() - 1);
                const Entity lastEntity = m_aliveEntities[lastIdx];
                m_aliveEntities[aliveIdx] = lastEntity;
                m_entityToAliveIndex[lastEntity.GetIndex()] = aliveIdx;
                m_aliveEntities.pop_back();
                m_entityToAliveIndex[index] = 0xFFFFFFFF;
            }
        }

        m_entityGenerations[index]++;
        m_freeIndices.push_back(index);

        if (m_aliveEntityCount > 0)
        {
            m_aliveEntityCount--;
        }
    }

    bool Scene::IsValid(Entity entity) const noexcept
    {
        if (!entity.IsValid())
        {
            return false;
        }

        const u32 index = entity.GetIndex();
        if (index >= m_entityGenerations.size())
        {
            return false;
        }

        return m_entityGenerations[index] == entity.GetVersion();
    }

    void Scene::Clear()
    {
        for (auto& pool : m_pools)
        {
            if (pool)
            {
                pool->Clear();
            }
        }

        for (u32& gen : m_entityGenerations)
        {
            gen++;
        }

        m_freeIndices.clear();
        for (u32 i = 0; i < static_cast<u32>(m_entityGenerations.size()); ++i)
        {
            m_freeIndices.push_back(i);
        }

        m_aliveEntities.clear();
        std::fill(m_entityToAliveIndex.begin(), m_entityToAliveIndex.end(), 0xFFFFFFFF);
        m_aliveEntityCount = 0;
    }

    bool Scene::IsAncestor(Entity ancestor, Entity descendant) const noexcept
    {
        if (!IsValid(ancestor) || !IsValid(descendant))
        {
            return false;
        }

        Entity current = GetParent(descendant);
        u32 depth = 0;
        while (current.IsValid() && IsValid(current) && depth < 64)
        {
            if (current == ancestor)
            {
                return true;
            }
            current = GetParent(current);
            depth++;
        }

        return false;
    }

    void Scene::SetParent(Entity child, Entity parent)
    {
        if (!IsValid(child))
        {
            return;
        }

        if (child == parent)
        {
            return;
        }

        if (parent.IsValid() && IsValid(parent))
        {
            if (IsAncestor(child, parent))
            {
                return;
            }
        }

        if (!HasComponent<RelationshipComponent>(child))
        {
            AddComponent<RelationshipComponent>(child);
        }

        if (parent.IsValid() && IsValid(parent) && !HasComponent<RelationshipComponent>(parent))
        {
            AddComponent<RelationshipComponent>(parent);
        }

        Entity oldParent = GetComponent<RelationshipComponent>(child).Parent;
        if (oldParent.IsValid() && IsValid(oldParent) && HasComponent<RelationshipComponent>(oldParent))
        {
            auto& oldParentRel = GetComponent<RelationshipComponent>(oldParent);
            auto it = std::remove(oldParentRel.Children.begin(), oldParentRel.Children.end(), child);
            oldParentRel.Children.erase(it, oldParentRel.Children.end());
        }

        if (parent.IsValid() && IsValid(parent))
        {
            GetComponent<RelationshipComponent>(child).Parent = parent;
            auto& newParentRel = GetComponent<RelationshipComponent>(parent);
            if (std::find(newParentRel.Children.begin(), newParentRel.Children.end(), child) == newParentRel.Children.end())
            {
                newParentRel.Children.push_back(child);
            }
        }
        else
        {
            GetComponent<RelationshipComponent>(child).Parent = Entity{};
        }
    }

    Entity Scene::GetParent(Entity entity) const noexcept
    {
        if (!IsValid(entity))
        {
            return Entity{};
        }
        const auto* rel = TryGetComponent<RelationshipComponent>(entity);
        if (!rel)
        {
            return Entity{};
        }
        return rel->Parent;
    }

    const std::vector<Entity>& Scene::GetChildren(Entity entity) const noexcept
    {
        static const std::vector<Entity> s_emptyChildren{};
        if (!IsValid(entity))
        {
            return s_emptyChildren;
        }
        const auto* rel = TryGetComponent<RelationshipComponent>(entity);
        if (!rel)
        {
            return s_emptyChildren;
        }
        return rel->Children;
    }

    DirectX::XMMATRIX Scene::GetWorldMatrix(Entity entity) const noexcept
    {
        if (!IsValid(entity))
        {
            return DirectX::XMMatrixIdentity();
        }

        const auto* transform = TryGetComponent<TransformComponent>(entity);
        if (!transform)
        {
            return DirectX::XMMatrixIdentity();
        }

        DirectX::XMMATRIX worldMatrix = transform->GetWorldMatrix();
        Entity current = entity;
        u32 depth = 0;
        while (depth < 64)
        {
            const auto* rel = TryGetComponent<RelationshipComponent>(current);
            if (!rel || !rel->Parent.IsValid() || !IsValid(rel->Parent) || rel->Parent == current)
            {
                break;
            }
            const auto* parentTransform = TryGetComponent<TransformComponent>(rel->Parent);
            if (!parentTransform)
            {
                break;
            }
            worldMatrix = worldMatrix * parentTransform->GetWorldMatrix();
            current = rel->Parent;
            depth++;
        }

        return worldMatrix;
    }

    Entity Scene::GetEntityFromIndex(u32 index) const noexcept
    {
        if (index >= m_entityGenerations.size())
        {
            return Entity{};
        }
        return Entity{ index, m_entityGenerations[index] };
    }

    void Scene::Update(f32 dt)
    {
        if (m_physicsSystem)
        {
            m_physicsSystem->Step(*this, dt);
        }
        if (m_particleSystem)
        {
            m_particleSystem->Update(*this, dt);
        }
        if (m_audioSystem)
        {
            m_audioSystem->Update(*this, dt);
        }
    }

    void Scene::Render(ID3D12GraphicsCommandList* cmdList, u32 frameIndex)
    {
        if (m_renderSystem)
        {
            m_renderSystem->Render(*this, cmdList, frameIndex);
        }
        if (m_particleSystem)
        {
            m_particleSystem->Render(cmdList, m_camera.GetViewProjectionMatrix(), frameIndex);
        }
    }
}
