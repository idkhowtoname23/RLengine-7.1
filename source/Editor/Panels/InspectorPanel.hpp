#pragma once

#include "Editor/UI/Panel.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"
#include "Core/Types.hpp"

#include <imgui.h>
#include <optional>
#include <functional>
#include <string>

namespace RL::Editor
{
    class InspectorPanel : public Panel
    {
    public:
        explicit InspectorPanel(Scene* scene)
            : Panel("Inspector##INS")
            , m_scene(scene)
        {}

        void SetScene(Scene* scene) noexcept { m_scene = scene; }

        void SetSelected(std::optional<Entity> entity) noexcept
        {
            m_selected = entity;
        }

        void OnRender() override
        {
            if (!m_selected.has_value() || !m_scene)
            {
                ImGui::TextDisabled("Select an entity in the Scene Hierarchy.");
                return;
            }

            const Entity ent = *m_selected;

            if (!m_scene->IsAlive(ent))
            {
                m_selected = std::nullopt;
                ImGui::TextDisabled("Entity is no longer alive.");
                return;
            }

            std::string name = "Entity";
            if (m_scene->HasComponent<TagComponent>(ent))
                name = m_scene->GetComponent<TagComponent>(ent).Tag;

            char nameBuf[128];
            std::strncpy(nameBuf, name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##ent_name", nameBuf, sizeof(nameBuf)))
            {
                if (m_scene->HasComponent<TagComponent>(ent))
                    m_scene->GetComponent<TagComponent>(ent).Tag = nameBuf;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 6);
            ImGui::TextDisabled("ID:%llu", ent.GetIndex());

            ImGui::Separator();

            DrawTransformComponent(ent);
            DrawMeshComponent(ent);
            DrawQuadRenderComponent(ent);
            DrawScriptComponent(ent);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const f32 btnWidth = ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.91f, 0.49f, 0.05f, 0.6f));
            if (ImGui::Button("+ Add Component", { btnWidth, 0 }))
            {
                ImGui::OpenPopup("##add_component_menu");
            }
            ImGui::PopStyleColor(2);

            if (ImGui::BeginPopup("##add_component_menu"))
            {
                if (ImGui::MenuItem("Transform"))
                {
                    if (!m_scene->HasComponent<TransformComponent>(ent))
                        m_scene->AddComponent<TransformComponent>(ent);
                }
                if (ImGui::MenuItem("Mesh Renderer"))
                {
                    if (!m_scene->HasComponent<MeshComponent>(ent))
                        m_scene->AddComponent<MeshComponent>(ent, "Cube");
                }
                if (ImGui::MenuItem("Quad Renderer"))
                {
                    if (!m_scene->HasComponent<QuadRenderComponent>(ent))
                        m_scene->AddComponent<QuadRenderComponent>(ent);
                }
                if (ImGui::MenuItem("Script"))
                {
                    if (!m_scene->HasComponent<ScriptComponent>(ent))
                        m_scene->AddComponent<ScriptComponent>(ent);
                }
                ImGui::EndPopup();
            }
        }

    private:

        [[nodiscard]] static bool ComponentHeader(const char* name, bool* pRemove = nullptr)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            const bool open = ImGui::CollapsingHeader(name);

            ImGui::PopStyleColor(3);

            if (pRemove)
            {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 10);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f,0.1f,0.1f,0.6f));
                const std::string removeId = std::format("##rm_{}", name);
                if (ImGui::SmallButton(removeId.c_str())) *pRemove = true;
                ImGui::PopStyleColor(2);
            }

            return open;
        }

        static void LabeledField(const char* label, f32 width = 100.0f)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::Text("%s", label);
            ImGui::PopStyleColor();
            ImGui::SameLine(width);
        }

        void DrawTransformComponent(Entity ent)
        {
            if (!m_scene->HasComponent<TransformComponent>(ent)) return;

            bool remove = false;
            if (!ComponentHeader("Transform", &remove)) return;
            if (remove) { m_scene->RemoveComponent<TransformComponent>(ent); return; }

            auto& t = m_scene->GetComponent<TransformComponent>(ent);

            ImGui::PushItemWidth(-1);
            ImGui::Spacing();

            LabeledField("Position");
            float pos[3] = { t.Position.x, t.Position.y, t.Position.z };
            if (ImGui::DragFloat3("##pos", pos, 0.05f))
            {
                t.Position = { pos[0], pos[1], pos[2] };
            }

            LabeledField("Rotation");
            float rot[3] = { t.Rotation.x, t.Rotation.y, t.Rotation.z };
            if (ImGui::DragFloat3("##rot", rot, 0.5f))
            {
                t.Rotation = { rot[0], rot[1], rot[2] };
            }

            LabeledField("Scale");
            float scl[3] = { t.Scale.x, t.Scale.y, t.Scale.z };
            if (ImGui::DragFloat3("##scl", scl, 0.01f, 0.001f, 1000.0f))
            {
                t.Scale = { scl[0], scl[1], scl[2] };
            }

            ImGui::PopItemWidth();
            ImGui::Spacing();
        }

        void DrawMeshComponent(Entity ent)
        {
            if (!m_scene->HasComponent<MeshComponent>(ent)) return;

            bool remove = false;
            if (!ComponentHeader("Mesh Renderer", &remove)) return;
            if (remove) { m_scene->RemoveComponent<MeshComponent>(ent); return; }

            auto& mc = m_scene->GetComponent<MeshComponent>(ent);

            ImGui::Spacing();
            LabeledField("Mesh", 80.0f);
            char meshBuf[128];
            std::strncpy(meshBuf, mc.MeshName.c_str(), sizeof(meshBuf) - 1);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##mesh_name", meshBuf, sizeof(meshBuf)))
            {
                mc.MeshName = meshBuf;
            }
            ImGui::Spacing();
        }

        void DrawQuadRenderComponent(Entity ent)
        {
            if (!m_scene->HasComponent<QuadRenderComponent>(ent)) return;

            bool remove = false;
            if (!ComponentHeader("Quad Renderer", &remove)) return;
            if (remove) { m_scene->RemoveComponent<QuadRenderComponent>(ent); return; }

            auto& qr = m_scene->GetComponent<QuadRenderComponent>(ent);

            ImGui::Spacing();

            LabeledField("Color", 80.0f);
            float col[4] = { qr.Color.x, qr.Color.y, qr.Color.z, qr.Color.w };
            if (ImGui::ColorEdit4("##quad_color", col))
            {
                qr.Color = { col[0], col[1], col[2], col[3] };
            }

            LabeledField("Visible", 80.0f);
            ImGui::Checkbox("##quad_visible", &qr.IsVisible);

            LabeledField("Tex Index", 80.0f);
            int texIdx = static_cast<int>(qr.TextureIndex);
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::DragInt("##tex_idx", &texIdx, 1, 0, 1024))
            {
                qr.TextureIndex = static_cast<u32>(std::max(0, texIdx));
            }

            ImGui::Spacing();
        }

        void DrawScriptComponent(Entity ent)
        {
            if (!m_scene->HasComponent<ScriptComponent>(ent)) return;

            bool remove = false;
            if (!ComponentHeader("Script", &remove)) return;
            if (remove) { m_scene->RemoveComponent<ScriptComponent>(ent); return; }

            auto& sc = m_scene->GetComponent<ScriptComponent>(ent);

            ImGui::Spacing();
            LabeledField("Script", 80.0f);
            char scriptBuf[256];
            std::strncpy(scriptBuf, sc.ScriptName.c_str(), sizeof(scriptBuf) - 1);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##script_name", scriptBuf, sizeof(scriptBuf)))
            {
                sc.ScriptName = scriptBuf;
            }
            ImGui::Spacing();
        }

        Scene*                m_scene = nullptr;
        std::optional<Entity> m_selected;
    };

}
