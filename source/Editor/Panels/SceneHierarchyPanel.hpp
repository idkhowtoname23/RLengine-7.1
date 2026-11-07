#pragma once

#include "Editor/UI/Panel.hpp"
#include "Scene/Scene.hpp"
#include "Core/Types.hpp"

#include <imgui.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace RL::Editor
{
    class SceneHierarchyPanel : public Panel
    {
    public:
        using SelectionCallback = std::function<void(Entity)>;

        explicit SceneHierarchyPanel(Scene* scene)
            : Panel("Scene Hierarchy##SH")
            , m_scene(scene)
        {}

        void SetScene(Scene* scene) noexcept { m_scene = scene; }

        void SetSelectionCallback(SelectionCallback cb)
        {
            m_onSelect = std::move(cb);
        }

        [[nodiscard]] std::optional<Entity> GetSelected() const noexcept
        {
            return m_selected;
        }

        void SetSelected(std::optional<Entity> entity) noexcept
        {
            m_selected = entity;
        }

        void OnRender() override
        {
            if (!m_scene) { ImGui::TextDisabled("No scene"); return; }

            RenderToolbar();

            ImGui::Separator();

            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##hier_search", "  Search entities...",
                m_searchBuf, sizeof(m_searchBuf));

            ImGui::Separator();

            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);

            if (ImGui::BeginChild("##entity_list", ImVec2(0, 0), false,
                ImGuiWindowFlags_HorizontalScrollbar))
            {

                const std::string filter(m_searchBuf);
                const auto& entities = m_scene->GetAliveEntities();
                std::vector<Entity> rootEntities;
                rootEntities.reserve(entities.size());
                for (const auto& ent : entities)
                {
                    if (!m_scene->IsAlive(ent)) continue;
                    const bool isRoot = !m_scene->HasComponent<RelationshipComponent>(ent) ||
                                        !m_scene->GetComponent<RelationshipComponent>(ent).Parent.IsValid() ||
                                        !m_scene->IsValid(m_scene->GetComponent<RelationshipComponent>(ent).Parent);
                    if (!isRoot) continue;

                    if (!filter.empty())
                    {
                        const std::string name = m_scene->HasComponent<TagComponent>(ent)
                            ? m_scene->GetComponent<TagComponent>(ent).Tag
                            : std::format("Entity {}", ent.GetIndex());
                        if (name.find(filter) == std::string::npos && !HasMatchingDescendant(ent, filter))
                        {
                            continue;
                        }
                    }

                    rootEntities.push_back(ent);
                }

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(rootEntities.size()));
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                    {
                        RenderEntityNode(rootEntities[i], filter);
                    }
                }

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                    ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
                {
                    m_selected = std::nullopt;
                    if (m_onSelect) m_onSelect(Entity{});
                }

                if (ImGui::BeginPopupContextWindow("##hier_empty_ctx",
                    ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
                {
                    RenderCreateMenu();
                    ImGui::EndPopup();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

    private:
        void RenderToolbar()
        {

            if (ImGui::Button("  +  "))
            {
                ImGui::OpenPopup("##hier_create_menu");
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Create");

            if (ImGui::BeginPopup("##hier_create_menu"))
            {
                RenderCreateMenu();
                ImGui::EndPopup();
            }

            ImGui::SameLine(0, 12);

            if (m_selected.has_value())
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button(" Del "))
                {
                    m_scene->DestroyEntity(*m_selected);
                    m_selected = std::nullopt;
                }
                ImGui::PopStyleColor();
            }
        }

        void RenderCreateMenu()
        {
            if (ImGui::MenuItem("Empty Entity"))
            {
                const Entity ent = m_scene->CreateEntity("Entity");
                SelectEntity(ent);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("3D Objects"))
            {
                if (ImGui::MenuItem("Cube"))      CreateMeshEntity("Cube",   "Cube");
                if (ImGui::MenuItem("Sphere"))    CreateMeshEntity("Sphere", "Sphere");
                if (ImGui::MenuItem("Plane"))     CreateMeshEntity("Plane",  "Plane");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Lights"))
            {
                if (ImGui::MenuItem("Point Light"))
                    CreateTaggedEntity("Point Light", "PointLight");
                if (ImGui::MenuItem("Directional Light"))
                    CreateTaggedEntity("Directional Light", "DirLight");
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Camera"))
                CreateTaggedEntity("Camera", "Camera");
        }

        [[nodiscard]] bool HasMatchingDescendant(Entity ent, const std::string& filter) const
        {
            if (!m_scene) return false;
            const auto* rel = m_scene->TryGetComponent<RelationshipComponent>(ent);
            if (!rel) return false;
            for (Entity child : rel->Children)
            {
                if (!m_scene->IsAlive(child)) continue;
                if (m_scene->HasComponent<TagComponent>(child))
                {
                    if (m_scene->GetComponent<TagComponent>(child).Tag.find(filter) != std::string::npos)
                    {
                        return true;
                    }
                }
                if (HasMatchingDescendant(child, filter))
                {
                    return true;
                }
            }
            return false;
        }

        void RenderEntityNode(Entity ent, const std::string& filter)
        {
            if (!m_scene || !m_scene->IsAlive(ent)) return;

            const std::string name = m_scene->HasComponent<TagComponent>(ent)
                ? m_scene->GetComponent<TagComponent>(ent).Tag
                : std::format("Entity {}", ent.GetIndex());

            if (!filter.empty() && name.find(filter) == std::string::npos && !HasMatchingDescendant(ent, filter))
            {
                return;
            }

            const char* icon = GetEntityIcon(ent);
            const bool isSelected = m_selected.has_value() && (*m_selected == ent);

            const auto* rel = m_scene->TryGetComponent<RelationshipComponent>(ent);
            const bool hasChildren = rel && !rel->Children.empty();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                      ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                      ImGuiTreeNodeFlags_SpanAvailWidth;

            if (!hasChildren)
            {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }

            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            const u64 idx = ent.GetIndex();
            if (idx % 2 == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(0.16f, 0.16f, 0.16f, 0.5f));
            }

            const std::string label = std::format("{} {}##ent_{}", icon, name, ent.GetIndex());
            const bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

            if (idx % 2 == 0) ImGui::PopStyleColor();

            if (ImGui::BeginDragDropSource())
            {
                u64 id = ent.GetID();
                ImGui::SetDragDropPayload("RL_ENTITY", &id, sizeof(u64));
                ImGui::Text("Parenting %s", name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("RL_ENTITY"))
                {
                    u64 droppedId = *static_cast<const u64*>(p->Data);
                    for (Entity e : m_scene->GetAliveEntities())
                    {
                        if (e.GetID() == droppedId)
                        {
                            m_scene->SetParent(e, ent);
                            break;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                SelectEntity(ent);
            }

            const std::string ctxId = std::format("##ent_ctx_{}", ent.GetIndex());
            if (ImGui::BeginPopupContextItem(ctxId.c_str()))
            {
                RenderEntityContextMenu(ent, name);
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_renamingEntity = ent;
                const auto& tag = name;
                std::strncpy(m_renameBuf, tag.c_str(), sizeof(m_renameBuf) - 1);
                ImGui::OpenPopup("##rename_popup");
            }

            if (hasChildren && nodeOpen)
            {
                for (Entity child : rel->Children)
                {
                    if (m_scene->IsAlive(child))
                    {
                        RenderEntityNode(child, filter);
                    }
                }
                ImGui::TreePop();
            }

            if (m_renamingEntity.has_value() && *m_renamingEntity == ent)
            {
                if (ImGui::BeginPopup("##rename_popup"))
                {
                    ImGui::Text("Rename:");
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##rename_input", m_renameBuf, sizeof(m_renameBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (m_scene->HasComponent<TagComponent>(ent))
                            m_scene->GetComponent<TagComponent>(ent).Tag = m_renameBuf;
                        m_renamingEntity = std::nullopt;
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    {
                        m_renamingEntity = std::nullopt;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        void RenderEntityContextMenu(Entity ent, const std::string& name)
        {
            ImGui::TextDisabled("%s", name.c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Select"))    SelectEntity(ent);
            if (ImGui::MenuItem("Duplicate")) DuplicateEntity(ent);
            if (ImGui::MenuItem("Unparent"))  m_scene->SetParent(ent, Entity{});
            ImGui::Separator();
            if (ImGui::MenuItem("Rename..."))
            {
                m_renamingEntity = ent;
                std::strncpy(m_renameBuf, name.c_str(), sizeof(m_renameBuf) - 1);
                ImGui::OpenPopup("##rename_popup");
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::MenuItem("Delete"))
            {
                m_scene->DestroyEntity(ent);
                if (m_selected == ent) m_selected = std::nullopt;
            }
            ImGui::PopStyleColor();
        }

        void SelectEntity(Entity ent)
        {
            m_selected = ent;
            if (m_onSelect) m_onSelect(ent);
        }

        void DuplicateEntity(Entity src)
        {

            const std::string srcName = m_scene->HasComponent<TagComponent>(src)
                ? m_scene->GetComponent<TagComponent>(src).Tag + " (Copy)"
                : "Entity (Copy)";

            const Entity dup = m_scene->CreateEntity(srcName);
            SelectEntity(dup);
        }

        void CreateMeshEntity(const char* name, const char* meshType)
        {
            const Entity ent = m_scene->CreateEntity(name);
            m_scene->RemoveComponent<QuadRenderComponent>(ent);
            m_scene->AddComponent<MeshComponent>(ent, meshType);
            SelectEntity(ent);
        }

        void CreateTaggedEntity(const char* name, [[maybe_unused]] const char* type)
        {
            const Entity ent = m_scene->CreateEntity(name);
            SelectEntity(ent);
        }

        [[nodiscard]] const char* GetEntityIcon(Entity ent) const noexcept
        {
            if (m_scene->HasComponent<MeshComponent>(ent))        return "[M]";
            if (m_scene->HasComponent<QuadRenderComponent>(ent))  return "[Q]";
            if (m_scene->HasComponent<ScriptComponent>(ent))      return "[S]";
            return "[E]";
        }

        Scene*                    m_scene  = nullptr;
        std::optional<Entity>     m_selected;
        std::optional<Entity>     m_renamingEntity;
        SelectionCallback         m_onSelect;

        char m_searchBuf[128] = {};
        char m_renameBuf[128] = {};
    };

}
