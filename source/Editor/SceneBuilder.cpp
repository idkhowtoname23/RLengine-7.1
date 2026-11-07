#if defined(_MSC_VER)
#pragma warning(disable: 4996)
#endif

#include "Editor/SceneBuilder.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"
#include "Scene/SceneSerializer.hpp"
#include "Scene/Systems/RenderSystem.hpp"
#include "Graphics/DX12/DX12Device.hpp"
#include "Graphics/DX12/TextureManager.hpp"
#include "Graphics/DX12/GLTFLoader.hpp"
#include "Graphics/DX12/Mesh.hpp"
#include "Core/Timer.hpp"
#include "Core/Logger.hpp"

#include <imgui.h>
#include <vector>
#include <filesystem>
#include <format>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace RL
{
    using namespace DirectX;

    void SceneBuilder::Play(Scene& scene) noexcept
    {
        if (m_simulationState == SimulationState::Edit)
        {
            m_physicsSystem.CaptureSnapshot(scene);
            m_simulationState = SimulationState::Playing;
        }
        else if (m_simulationState == SimulationState::Paused)
        {
            m_simulationState = SimulationState::Playing;
        }
    }

    void SceneBuilder::Pause() noexcept
    {
        if (m_simulationState == SimulationState::Playing)
        {
            m_simulationState = SimulationState::Paused;
        }
    }

    void SceneBuilder::Stop(Scene& scene) noexcept
    {
        if (m_simulationState == SimulationState::Playing || m_simulationState == SimulationState::Paused)
        {
            m_physicsSystem.RestoreSnapshot(scene);
            m_currentFrame = m_startFrame;
            m_simulationState = SimulationState::Edit;
        }
    }

    void SceneBuilder::Render(
        Scene& scene,
        const DX12Device& device,
        const Timer& timer,
        RenderSystem* renderSystem,
        TextureManager* textureManager,
        [[maybe_unused]] ScriptEngine* scriptEngine,
        StressTest& stressTest)
    {
        if (!m_isOpen)
        {
            return;
        }

        m_dx12Device = device.GetDevice();

        HandleBlenderHotkeys(scene);

        RenderBlenderTopHeader(scene, device, renderSystem, stressTest);

        const ImGuiViewport* vp = ImGui::GetMainViewport();

        const float statusBarHeight = m_showStatusBar ? 24.0f : 0.0f;
        const bool showTimeline = m_showTimeline && (m_workspace == BlenderWorkspace::Layout || m_workspace == BlenderWorkspace::Animation);
        const float timelineHeight = showTimeline ? 42.0f : 0.0f;
        const bool showSidebar = (m_workspace != BlenderWorkspace::Diagnostics && m_workspace != BlenderWorkspace::Scripting) && (m_showHierarchy || m_showInspector);
        const float sidebarWidth = showSidebar ? 330.0f : 0.0f;

        const float pad = 3.0f;
        const float topY = vp->WorkPos.y + pad;
        const float bottomY = vp->WorkPos.y + vp->WorkSize.y - statusBarHeight - pad;
        const float totalSidebarHeight = bottomY - topY;

        const float vpAreaX = vp->WorkPos.x + pad;
        const float vpHeaderHeight = 28.0f;
        const float sidebarX = vp->WorkPos.x + vp->WorkSize.x - sidebarWidth - pad;
        const float vpAreaWidth = showSidebar ? (sidebarX - vpAreaX - pad) : (vp->WorkSize.x - pad * 2.0f);

        if (m_workspace != BlenderWorkspace::Scripting && m_workspace != BlenderWorkspace::Diagnostics)
        {
            RenderBlenderViewportOverlays(scene, timer, device, renderSystem, vpAreaX, topY, vpAreaWidth, vpHeaderHeight);

            const float vpContentY = topY + vpHeaderHeight;
            const float vpContentHeight = (bottomY - timelineHeight) - vpContentY;
            if (vpContentHeight > 0.0f)
            {
                ImGui::SetNextWindowPos(ImVec2(vpAreaX, vpContentY), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(vpAreaWidth, vpContentHeight), ImGuiCond_Always);
                ImGuiWindowFlags vpInteractFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
                if (ImGui::Begin("##BlenderViewportInteraction", nullptr, vpInteractFlags))
                {
                    if (m_selectedEntity.IsValid() && scene.IsAlive(m_selectedEntity) && scene.HasComponent<TransformComponent>(m_selectedEntity))
                    {
                        auto& t = scene.GetComponent<TransformComponent>(m_selectedEntity);
                        XMMATRIX worldMat = scene.GetWorldMatrix(m_selectedEntity);
                        XMFLOAT4X4 world4x4;
                        XMStoreFloat4x4(&world4x4, worldMat);

                        const auto& cam = scene.GetCamera();
                        const XMMATRIX vMat = cam.CalculateViewMatrix();
                        const XMMATRIX pMat = cam.CalculateProjectionMatrix();

                        if (m_activeTool == BlenderTool::Move)
                        {
                            m_gizmo.SetMode(Editor::GizmoMode::Translate);
                        }
                        else if (m_activeTool == BlenderTool::Rotate)
                        {
                            m_gizmo.SetMode(Editor::GizmoMode::Rotate);
                        }
                        else if (m_activeTool == BlenderTool::Scale)
                        {
                            m_gizmo.SetMode(Editor::GizmoMode::Scale);
                        }
                        else if (m_activeTool == BlenderTool::Transform)
                        {
                            m_gizmo.SetMode(Editor::GizmoMode::Translate);
                        }

                        if (m_activeTool == BlenderTool::Move || m_activeTool == BlenderTool::Rotate ||
                            m_activeTool == BlenderTool::Scale || m_activeTool == BlenderTool::Transform)
                        {
                            auto gizmoRes = m_gizmo.Manipulate(world4x4, vMat, pMat, ImVec2(vpAreaX, vpContentY), ImVec2(vpAreaWidth, vpContentHeight));
                            if (gizmoRes.changed)
                            {
                                if (!m_gizmoDragging)
                                {
                                    m_gizmoDragging = true;
                                    m_gizmoStartTransform = t;
                                    m_gizmoTargetEntity = m_selectedEntity;
                                }

                                if (m_activeTool == BlenderTool::Move)
                                {
                                    t.Position.x += gizmoRes.deltaPos.x;
                                    t.Position.y += gizmoRes.deltaPos.y;
                                    t.Position.z += gizmoRes.deltaPos.z;
                                }
                                else if (m_activeTool == BlenderTool::Rotate)
                                {
                                    t.Rotation.x += gizmoRes.deltaRot.x;
                                    t.Rotation.y += gizmoRes.deltaRot.y;
                                    t.Rotation.z += gizmoRes.deltaRot.z;
                                }
                                else if (m_activeTool == BlenderTool::Scale)
                                {
                                    t.Scale.x += gizmoRes.deltaScale.x;
                                    t.Scale.y += gizmoRes.deltaScale.y;
                                    t.Scale.z += gizmoRes.deltaScale.z;
                                    t.Scale.x = (std::max)(t.Scale.x, 0.001f);
                                    t.Scale.y = (std::max)(t.Scale.y, 0.001f);
                                    t.Scale.z = (std::max)(t.Scale.z, 0.001f);
                                }
                                else if (m_activeTool == BlenderTool::Transform)
                                {
                                    t.Position.x += gizmoRes.deltaPos.x;
                                    t.Position.y += gizmoRes.deltaPos.y;
                                    t.Position.z += gizmoRes.deltaPos.z;
                                    t.Rotation.x += gizmoRes.deltaRot.x;
                                    t.Rotation.y += gizmoRes.deltaRot.y;
                                    t.Rotation.z += gizmoRes.deltaRot.z;
                                    t.Scale.x += gizmoRes.deltaScale.x;
                                    t.Scale.y += gizmoRes.deltaScale.y;
                                    t.Scale.z += gizmoRes.deltaScale.z;
                                    t.Scale.x = (std::max)(t.Scale.x, 0.001f);
                                    t.Scale.y = (std::max)(t.Scale.y, 0.001f);
                                    t.Scale.z = (std::max)(t.Scale.z, 0.001f);
                                }
                            }
                        }
                    }

                    if (m_gizmoDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        m_gizmoDragging = false;
                        if (m_gizmoTargetEntity.IsValid() && scene.IsAlive(m_gizmoTargetEntity) && scene.HasComponent<TransformComponent>(m_gizmoTargetEntity))
                        {
                            const auto& currentT = scene.GetComponent<TransformComponent>(m_gizmoTargetEntity);
                            if (std::memcmp(&m_gizmoStartTransform, &currentT, sizeof(TransformComponent)) != 0)
                            {
                                Editor::CommandHistory::Execute(std::make_unique<Editor::TransformCommand>(&scene, m_gizmoTargetEntity, m_gizmoStartTransform, currentT));
                            }
                        }
                    }

                    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && !ImGui::IsAnyItemHovered())
                    {
                        const ImVec2 mousePos = ImGui::GetMousePos();
                        Entity picked = PickEntityAtScreenPos(scene, mousePos, ImVec2(vpAreaX, vpContentY), ImVec2(vpAreaWidth, vpContentHeight));
                        if (picked.IsValid())
                        {
                            m_selectedEntity = picked;
                        }
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
                        {
                            const char* pathStr = static_cast<const char*>(payload->Data);
                            std::filesystem::path assetPath(pathStr);
                            auto ext = assetPath.extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                            if (ext == ".gltf" || ext == ".glb")
                            {
                                auto model = GLTFLoader::LoadFromFile(device.GetDevice(), assetPath.string());
                                if (model && !model->Meshes.empty())
                                {
                                    Entity e = scene.CreateEntity(assetPath.stem().string());
                                    auto& mc = scene.AddComponent<MeshComponent>(e);
                                    mc.MeshName = assetPath.stem().string();
                                    mc.ModelPath = assetPath.string();
                                    mc.MeshRef = model->Meshes[0];
                                    m_selectedEntity = e;
                                }
                            }
                            else if (ext == ".png" || ext == ".jpg" || ext == ".dds")
                            {
                                if (m_selectedEntity.IsValid() && scene.IsAlive(m_selectedEntity) && scene.HasComponent<MeshComponent>(m_selectedEntity))
                                {
                                    auto& mc = scene.GetComponent<MeshComponent>(m_selectedEntity);
                                    mc.TexturePath = assetPath.string();
                                }
                            }
                        }
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                        {
                            const char* pathStr = static_cast<const char*>(payload->Data);
                            std::filesystem::path assetPath(pathStr);
                            auto ext = assetPath.extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                            if (ext == ".gltf" || ext == ".glb")
                            {
                                auto model = GLTFLoader::LoadFromFile(device.GetDevice(), assetPath.string());
                                if (model && !model->Meshes.empty())
                                {
                                    Entity e = scene.CreateEntity(assetPath.stem().string());
                                    auto& mc = scene.AddComponent<MeshComponent>(e);
                                    mc.MeshName = assetPath.stem().string();
                                    mc.ModelPath = assetPath.string();
                                    mc.MeshRef = model->Meshes[0];
                                    m_selectedEntity = e;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                ImGui::End();
            }

            if (m_showToolShelf)
            {
                RenderBlenderToolShelf(vpAreaX + 6.0f, topY + vpHeaderHeight + 6.0f);
            }
        }

        if (showSidebar)
        {
            if (m_showHierarchy && m_showInspector)
            {
                const float outlinerHeight = std::floor((totalSidebarHeight - (showTimeline ? 0.0f : 0.0f)) * 0.44f);
                const float propertiesY = topY + outlinerHeight + pad;
                const float propertiesHeight = bottomY - propertiesY;

                RenderBlenderOutliner(scene, sidebarX, topY, sidebarWidth, outlinerHeight);
                RenderBlenderProperties(scene, device, timer, renderSystem, textureManager, sidebarX, propertiesY, sidebarWidth, propertiesHeight);
            }
            else if (m_showHierarchy)
            {
                RenderBlenderOutliner(scene, sidebarX, topY, sidebarWidth, totalSidebarHeight);
            }
            else if (m_showInspector)
            {
                RenderBlenderProperties(scene, device, timer, renderSystem, textureManager, sidebarX, topY, sidebarWidth, totalSidebarHeight);
            }
        }

        if (showTimeline)
        {
            RenderBlenderTimeline(scene, vpAreaX, bottomY - timelineHeight, vpAreaWidth, timelineHeight);
        }

        if (m_showStatusBar)
        {
            RenderBlenderStatusBar(scene, timer, device, vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - statusBarHeight, vp->WorkSize.x, statusBarHeight);
        }

        if (m_workspace == BlenderWorkspace::Diagnostics)
        {
            ImGui::SetNextWindowPos(ImVec2(vpAreaX, topY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - pad * 2.0f, bottomY - topY), ImGuiCond_Always);
            ImGuiWindowFlags diagWinFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
            if (ImGui::Begin("Hardware Diagnostics & Profiler##Full", nullptr, diagWinFlags))
            {
                ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "RLengine 7.1 Hardware Profiler & GPU Architecture");
                ImGui::Separator();
                const auto& gpu = device.GetGpuInfo();
                ImGui::Text("GPU Device: %s", gpu.name.c_str());
                ImGui::Text("Architecture: %s", gpu.architectureName.c_str());
                ImGui::Text("GPU Generation: %s", gpu.generationName.c_str());
                ImGui::Text("Direct3D Feature Level: %s", gpu.GetFeatureLevelString().data());
                ImGui::Text("Shader Model: %s", gpu.GetShaderModelString().data());
                ImGui::Text("Dedicated VRAM: %.2f GB", gpu.GetDedicatedVramGB());
                ImGui::Separator();
                ImGui::Text("Engine Performance:");
                ImGui::Text("FPS: %u (Frame Time: %.2f ms)", timer.GetFPS(), timer.GetFrameTimeMS());
                ImGui::Text("Alive ECS Entities: %zu", scene.GetAliveEntityCount());
                if (renderSystem)
                {
                    ImGui::Text("Rendered Entities: %u", renderSystem->GetLastRenderedCount());
                }
            }
            ImGui::End();
        }

        if (m_openAddPopup)
        {
            RenderBlenderAddPopup(scene, device.GetDevice(), renderSystem, stressTest);
        }
    }

    void SceneBuilder::RenderBlenderTopHeader(
        Scene& scene,
        const DX12Device& device,
        RenderSystem* renderSystem,
        StressTest& stressTest)
    {
        if (ImGui::BeginMainMenuBar())
        {

            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", "Ctrl+N"))
                {
                    scene.Clear();
                    m_selectedEntity = Entity{};
                }
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
                {
                    LoadScene(scene, device.GetDevice(), "assets/scenes/default.rlscene");
                }
                if (ImGui::MenuItem("Save Scene...", "Ctrl+S"))
                {
                    SaveScene(scene, "assets/scenes/default.rlscene");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Import glTF / 3D Model...", "Ctrl+I"))
                {
                    if (renderSystem && std::filesystem::exists(m_gltfPathInput))
                    {
                        auto model = GLTFLoader::LoadFromFile(device.GetDevice(), m_gltfPathInput);
                        if (model && !model->Meshes.empty())
                        {
                            Entity e = scene.CreateEntity("ImportedModel");
                            auto& mc = scene.AddComponent<MeshComponent>(e);
                            mc.MeshName = "ImportedMesh";
                            mc.ModelPath = m_gltfPathInput;
                            mc.MeshRef = model->Meshes[0];
                            m_selectedEntity = e;
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4"))
                {
                    PostQuitMessage(0);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, Editor::CommandHistory::CanUndo())) { Editor::CommandHistory::Undo(); }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, Editor::CommandHistory::CanRedo())) { Editor::CommandHistory::Redo(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Preferences...")) {}
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add"))
            {
                if (ImGui::BeginMenu("Mesh"))
                {
                    if (ImGui::MenuItem("Cube", "Shift+A > Cube"))
                    {
                        Entity e = scene.CreateEntity("Cube");
                        scene.AddComponent<MeshComponent>(e, "Cube");
                        m_selectedEntity = e;
                    }
                    if (ImGui::MenuItem("Sphere"))
                    {
                        Entity e = scene.CreateEntity("Sphere");
                        scene.AddComponent<MeshComponent>(e, "Sphere");
                        m_selectedEntity = e;
                    }
                    if (ImGui::MenuItem("Plane"))
                    {
                        Entity e = scene.CreateEntity("Plane");
                        scene.AddComponent<MeshComponent>(e, "Plane");
                        m_selectedEntity = e;
                    }
                    if (ImGui::MenuItem("Grid"))
                    {
                        Entity e = scene.CreateEntity("Grid");
                        scene.AddComponent<MeshComponent>(e, "Grid");
                        m_selectedEntity = e;
                    }
                    if (ImGui::MenuItem("2D Sprite Quad"))
                    {
                        Entity e = scene.CreateEntity("SpriteQuad");
                        m_selectedEntity = e;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Light"))
                {
                    if (ImGui::MenuItem("Point Light"))
                    {
                        Entity e = scene.CreateEntity("PointLight");
                        scene.AddComponent<LightComponent>(e, LightType::Point, XMFLOAT3{1.0f, 0.95f, 0.8f}, 8.0f);
                        m_selectedEntity = e;
                    }
                    if (ImGui::MenuItem("Sun / Directional Light"))
                    {
                        Entity e = scene.CreateEntity("SunLight");
                        scene.AddComponent<LightComponent>(e, LightType::Directional, XMFLOAT3{1.0f, 1.0f, 0.9f}, 5.0f);
                        m_selectedEntity = e;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera"))
                {
                    if (ImGui::MenuItem("Perspective Camera"))
                    {
                        Entity e = scene.CreateEntity("Camera");
                        scene.AddComponent<CameraComponent>(e);
                        m_selectedEntity = e;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Physics"))
                {
                    if (ImGui::MenuItem("Rigid Body Dynamic Cube"))
                    {
                        Entity e = scene.CreateEntity("RigidBodyCube");
                        scene.AddComponent<MeshComponent>(e, "Cube");
                        auto& rb = scene.AddComponent<RigidBodyComponent>(e);
                        rb.Type = BodyType::Dynamic;
                        rb.Mass = 1.0f;
                        rb.Restitution = 0.6f;
                        auto& bc = scene.AddComponent<BoxColliderComponent>(e);
                        bc.HalfSize = { 0.5f, 0.5f, 0.5f };
                        auto& t = scene.GetComponent<TransformComponent>(e);
                        t.Position.y = 5.0f;
                        m_selectedEntity = e;
                    }
                    if (ImGui::MenuItem("Rigid Body Dynamic Sphere"))
                    {
                        Entity e = scene.CreateEntity("RigidBodySphere");
                        scene.AddComponent<MeshComponent>(e, "Sphere");
                        auto& rb = scene.AddComponent<RigidBodyComponent>(e);
                        rb.Type = BodyType::Dynamic;
                        rb.Mass = 1.0f;
                        rb.Restitution = 0.75f;
                        auto& sc = scene.AddComponent<SphereColliderComponent>(e);
                        sc.Radius = 0.5f;
                        auto& t = scene.GetComponent<TransformComponent>(e);
                        t.Position.y = 7.0f;
                        m_selectedEntity = e;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Effects"))
                {
                    if (ImGui::MenuItem("Particle Emitter"))
                    {
                        Entity e = scene.CreateEntity("ParticleEmitter");
                        auto& pe = scene.AddComponent<ParticleEmitterComponent>(e);
                        pe.EmissionRate = 200.0f;
                        pe.Speed = 6.0f;
                        pe.FloorBounce = true;
                        auto& t = scene.GetComponent<TransformComponent>(e);
                        t.Position.y = 1.0f;
                        m_selectedEntity = e;
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::BeginMenu("GPU Stress Benchmark"))
                {
                    if (ImGui::MenuItem("Spawn 5,000 Entities"))
                    {
                        stressTest.Spawn(scene, 5000);
                    }
                    if (ImGui::MenuItem("Spawn 50,000 Entities"))
                    {
                        stressTest.Spawn(scene, 50000);
                    }
                    if (ImGui::MenuItem("Spawn 100,000 Entities"))
                    {
                        stressTest.Spawn(scene, 100000);
                    }
                    if (ImGui::MenuItem("Spawn 1,000,000 Entities (1M GPU Instanced)"))
                    {
                        stressTest.Spawn(scene, 1000000);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Render"))
            {
                if (ImGui::MenuItem("Render Frame", "F12")) {}
                ImGui::Separator();
                ImGui::TextDisabled("GPU: %s", device.GetGpuInfo().name.c_str());
                ImGui::TextDisabled("Arch: %s", device.GetGpuInfo().architectureName.c_str());
                ImGui::TextDisabled("Feature Level: %s", device.GetGpuInfo().GetFeatureLevelString().data());
                ImGui::TextDisabled("Shader Model: %s", device.GetGpuInfo().GetShaderModelString().data());
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem("Tool Shelf [T]", nullptr, &m_showToolShelf);
                ImGui::MenuItem("Outliner", nullptr, &m_showHierarchy);
                ImGui::MenuItem("Properties [N]", nullptr, &m_showInspector);
                ImGui::MenuItem("Timeline", nullptr, &m_showTimeline);
                ImGui::MenuItem("Status Bar", nullptr, &m_showStatusBar);
                ImGui::MenuItem("3D Compass Gizmo", nullptr, &m_show3DCompass);
                ImGui::EndMenu();
            }

            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            auto WorkspaceButton = [this](const char* label, BlenderWorkspace ws) {
                const bool active = (m_workspace == ws);
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.910f, 0.490f, 0.051f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
                }

                if (ImGui::Button(label))
                {
                    m_workspace = ws;
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine(0, 3);
            };

            WorkspaceButton("Layout", BlenderWorkspace::Layout);
            WorkspaceButton("Modeling", BlenderWorkspace::Modeling);
            WorkspaceButton("Shading", BlenderWorkspace::Shading);
            WorkspaceButton("Animation", BlenderWorkspace::Animation);
            WorkspaceButton("Scripting", BlenderWorkspace::Scripting);
            WorkspaceButton("Diagnostics", BlenderWorkspace::Diagnostics);

            const float rightBlockWidth = 240.0f;
            if (ImGui::GetWindowWidth() > 850.0f)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - rightBlockWidth);
            }

            const char* modeNames[] = { "Object Mode", "Edit Mode", "Sculpt Mode" };
            int currentModeIdx = static_cast<int>(m_mode);
            ImGui::SetNextItemWidth(105);
            if (ImGui::Combo("##ModeSelector", &currentModeIdx, modeNames, IM_ARRAYSIZE(modeNames)))
            {
                m_mode = static_cast<BlenderMode>(currentModeIdx);
            }

            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.91f, 0.49f, 0.051f, 1.0f));
            ImGui::TextUnformatted("RLengine 7.1");
            ImGui::PopStyleColor();

            ImGui::EndMainMenuBar();
        }
    }

    void SceneBuilder::RenderBlenderToolShelf(float x, float y)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(38.0f, 350.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));

        if (ImGui::Begin("##BlenderToolShelf", nullptr, flags))
        {
            ImGui::PushID("CloseShelf");
            if (ImGui::Button("<", ImVec2(28.0f, 18.0f)))
            {
                m_showToolShelf = false;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Close Tool Shelf [T]");
            }
            ImGui::PopID();
            ImGui::Separator();

            auto ToolButton = [this](const char* label, const char* tooltip, BlenderTool tool) {
                ImGui::PushID(static_cast<int>(tool));
                const bool active = (m_activeTool == tool);
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.910f, 0.490f, 0.051f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                }

                if (ImGui::Button(label, ImVec2(28.0f, 28.0f)))
                {
                    m_activeTool = tool;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", tooltip);
                }

                ImGui::PopStyleColor(2);
                ImGui::PopID();
            };

            ToolButton("[+]", "Select Box [W]", BlenderTool::SelectBox);
            ToolButton("(+)", "3D Cursor [Shift+RMB]", BlenderTool::Cursor);
            ToolButton(" G ", "Move / Grab [G]", BlenderTool::Move);
            ToolButton(" R ", "Rotate [R]", BlenderTool::Rotate);
            ToolButton(" S ", "Scale [S]", BlenderTool::Scale);
            ToolButton(" * ", "Transform (Universal)", BlenderTool::Transform);
            ToolButton(" / ", "Annotate", BlenderTool::Annotate);
            ToolButton(" | ", "Measure", BlenderTool::Measure);
            ToolButton(" # ", "Add Primitive [Shift+A]", BlenderTool::AddPrimitive);

            if (m_activeTool == BlenderTool::AddPrimitive)
            {
                m_openAddPopup = true;
                m_activeTool = BlenderTool::SelectBox;
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
    }

    void SceneBuilder::RenderBlenderViewportOverlays(
        Scene& scene,
        const Timer& timer,
        [[maybe_unused]] const DX12Device& device,
        RenderSystem* renderSystem,
        float x, float y, float width, float height)
    {

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        ImGuiWindowFlags headerFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.14f, 0.98f));

        if (ImGui::Begin("##ViewportHeader", nullptr, headerFlags))
        {

            ImGui::TextDisabled("View");
            ImGui::SameLine(0, 8);
            ImGui::TextDisabled("Select");
            ImGui::SameLine(0, 8);
            ImGui::TextDisabled("Add");
            ImGui::SameLine(0, 16);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 16);

            const char* spaceNames[] = { "Global", "Local" };
            int currentSpaceIdx = static_cast<int>(m_transformSpace);
            ImGui::SetNextItemWidth(75);
            if (ImGui::Combo("##TransformSpace", &currentSpaceIdx, spaceNames, IM_ARRAYSIZE(spaceNames)))
            {
                m_transformSpace = static_cast<TransformSpace>(currentSpaceIdx);
            }

            ImGui::SameLine(0, 8);
            static bool snapEnabled = false;
            ImGui::Checkbox("Snap", &snapEnabled);

            const float rightBarWidth = 450.0f;
            if (width > rightBarWidth + 200.0f)
            {
                ImGui::SameLine(width - rightBarWidth);
            }

            if (m_simulationState == SimulationState::Playing)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.25f, 1.0f));
            }
            if (ImGui::SmallButton(" [>] "))
            {
                Play(scene);
            }
            if (m_simulationState == SimulationState::Playing)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Play Simulation");
            }

            ImGui::SameLine(0, 3);

            if (m_simulationState == SimulationState::Paused)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.70f, 0.10f, 1.0f));
            }
            if (ImGui::SmallButton(" [||] "))
            {
                Pause();
            }
            if (m_simulationState == SimulationState::Paused)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Pause Simulation");
            }

            ImGui::SameLine(0, 3);

            if (m_simulationState != SimulationState::Edit)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
            }
            if (ImGui::SmallButton(" [[]] "))
            {
                Stop(scene);
            }
            if (m_simulationState != SimulationState::Edit)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Stop Simulation");
            }

            ImGui::SameLine(0, 6);

            if (ImGui::SmallButton(m_showToolShelf ? "Tools: ON" : "Tools: OFF"))
            {
                m_showToolShelf = !m_showToolShelf;
            }
            ImGui::SameLine(0, 4);

            if (renderSystem)
            {
                bool gridVis = renderSystem->IsGridVisible();
                if (ImGui::SmallButton(gridVis ? "Grid: ON" : "Grid: OFF"))
                {
                    renderSystem->SetShowGrid(!gridVis);
                }
                ImGui::SameLine(0, 4);
            }

            if (ImGui::SmallButton(m_show3DCompass ? "Gizmo: ON" : "Gizmo: OFF"))
            {
                m_show3DCompass = !m_show3DCompass;
            }
            ImGui::SameLine(0, 8);

            auto ShadingButton = [this](const char* label, const char* tooltip, ViewportShadingMode mode) {
                const bool active = (m_shadingMode == mode);
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.910f, 0.490f, 0.051f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.70f, 1.0f));
                }

                if (ImGui::Button(label, ImVec2(44.0f, 20.0f)))
                {
                    m_shadingMode = mode;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", tooltip);
                }

                ImGui::PopStyleColor(2);
                ImGui::SameLine(0, 3);
            };

            ShadingButton("Wire", "Wireframe Shading [Z]", ViewportShadingMode::Wireframe);
            ShadingButton("Solid", "Solid Shading [Z]", ViewportShadingMode::Solid);
            ShadingButton("Mat", "Material Preview [Z]", ViewportShadingMode::MaterialPreview);
            ShadingButton("Render", "Rendered View [Z]", ViewportShadingMode::Rendered);
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        if (m_show3DCompass)
        {
            const float compassRadius = 30.0f;
            const float compassX = x + width - 50.0f;
            const float compassY = y + height + 48.0f;
            Draw3DCompassGizmo(scene, XMFLOAT2(compassX, compassY), compassRadius);
        }

        ImGui::SetNextWindowPos(ImVec2(x + 50.0f, y + height + 8.0f), ImGuiCond_Always);
        ImGuiWindowFlags diagFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
                                     ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##BlenderViewportDiag", nullptr, diagFlags))
        {
            const auto& cam = scene.GetCamera();
            const auto& camPos = cam.GetPosition();
            const char* projName = (cam.GetProjectionType() == CameraProjection::Perspective) ? "Perspective" : "Orthographic";

            ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "User %s", projName);
            ImGui::TextDisabled("Camera: (%.1f, %.1f, %.1f) | Zoom: %.2fx", camPos.x, camPos.y, camPos.z, cam.GetZoom());
            ImGui::TextDisabled("FPS: %u (%.2f ms) | Entities: %zu", timer.GetFPS(), timer.GetFrameTimeMS(), scene.GetAliveEntityCount());
            if (renderSystem)
            {
                ImGui::TextDisabled("Rendered: %u objects", renderSystem->GetLastRenderedCount());
            }
        }
        ImGui::End();
    }

    void SceneBuilder::Draw3DCompassGizmo(
        Scene& scene,
        const XMFLOAT2& screenCenter,
        float radius)
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImVec2 center(screenCenter.x, screenCenter.y);

        dl->AddCircleFilled(center, radius + 4.0f, IM_COL32(28, 28, 28, 180));
        dl->AddCircle(center, radius + 4.0f, IM_COL32(55, 55, 55, 220), 32, 1.5f);

        const XMMATRIX view = scene.GetCamera().CalculateViewMatrix();

        struct AxisInfo {
            XMVECTOR dir;
            ImU32 color;
            const char* label;
            int axisId;
        };

        const AxisInfo axes[] = {
            { XMVectorSet(1, 0, 0, 0), IM_COL32(235, 60, 60, 255), "X", 0 },
            { XMVectorSet(0, 1, 0, 0), IM_COL32(60, 180, 60, 255), "Y", 1 },
            { XMVectorSet(0, 0, 1, 0), IM_COL32(50, 140, 240, 255), "Z", 2 }
        };

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        for (const auto& a : axes)
        {
            XMVECTOR viewDir = XMVector3TransformNormal(a.dir, view);
            XMFLOAT3 vDir;
            XMStoreFloat3(&vDir, viewDir);

            const ImVec2 tip(center.x + vDir.x * radius, center.y - vDir.y * radius);

            dl->AddLine(center, tip, a.color, 2.0f);

            const float bubbleRadius = 7.0f;
            dl->AddCircleFilled(tip, bubbleRadius, a.color);
            dl->AddText(ImVec2(tip.x - 3.5f, tip.y - 6.0f), IM_COL32(255, 255, 255, 255), a.label);

            const float dx = mousePos.x - tip.x;
            const float dy = mousePos.y - tip.y;
            if (dx * dx + dy * dy <= (bubbleRadius + 3.0f) * (bubbleRadius + 3.0f))
            {
                dl->AddCircle(tip, bubbleRadius + 2.0f, IM_COL32(255, 255, 255, 220), 16, 1.5f);
                if (mouseClicked)
                {
                    auto& cam = scene.GetCamera();
                    if (a.axisId == 0)
                    {
                        cam.SetPosition(DirectX::XMFLOAT3(10.0f, 0.0f, -10.0f));
                    }
                    else if (a.axisId == 1)
                    {
                        cam.SetPosition(DirectX::XMFLOAT3(0.0f, 10.0f, -10.0f));
                    }
                    else if (a.axisId == 2)
                    {
                        cam.SetPosition(DirectX::XMFLOAT3(0.0f, 0.0f, -10.0f));
                    }
                }
            }
        }

        const float centerRadius = 5.0f;
        const float cdx = mousePos.x - center.x;
        const float cdy = mousePos.y - center.y;
        const bool centerHovered = (cdx * cdx + cdy * cdy <= (centerRadius + 3.0f) * (centerRadius + 3.0f));

        dl->AddCircleFilled(center, centerRadius, centerHovered ? IM_COL32(255, 200, 50, 255) : IM_COL32(180, 180, 180, 200));
        if (centerHovered && mouseClicked)
        {
            auto& cam = scene.GetCamera();
            const auto curProj = cam.GetProjectionType();
            cam.SetProjectionType(curProj == CameraProjection::Perspective ? CameraProjection::Orthographic : CameraProjection::Perspective);
        }
    }

    bool SceneBuilder::HasMatchingDescendant(const Scene& scene, Entity ent, std::string_view filter) const
    {
        const auto* rel = scene.TryGetComponent<RelationshipComponent>(ent);
        if (!rel) return false;
        for (Entity child : rel->Children)
        {
            if (!scene.IsAlive(child)) continue;
            if (scene.HasComponent<TagComponent>(child))
            {
                if (scene.GetComponent<TagComponent>(child).Tag.find(filter) != std::string_view::npos)
                {
                    return true;
                }
            }
            if (HasMatchingDescendant(scene, child, filter))
            {
                return true;
            }
        }
        return false;
    }

    void SceneBuilder::RenderEntityNode(Scene& scene, Entity ent, std::string_view filter)
    {
        if (!scene.IsAlive(ent)) return;

        std::string tag = "Entity";
        if (scene.HasComponent<TagComponent>(ent))
        {
            tag = scene.GetComponent<TagComponent>(ent).Tag;
        }

        if (!filter.empty() && tag.find(filter) == std::string::npos && !HasMatchingDescendant(scene, ent, filter))
        {
            return;
        }

        const char* icon = "[■]";
        if (scene.HasComponent<LightComponent>(ent)) icon = "[☼]";
        else if (scene.HasComponent<CameraComponent>(ent)) icon = "[📷]";
        else if (scene.HasComponent<ScriptComponent>(ent)) icon = "[§]";
        else if (scene.HasComponent<MeshComponent>(ent)) icon = "[🎲]";

        const auto* rel = scene.TryGetComponent<RelationshipComponent>(ent);
        const bool hasChildren = rel && !rel->Children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (m_selectedEntity == ent)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string label = std::string(icon) + " " + tag + "##" + std::to_string(ent.GetIndex());
        const bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::BeginDragDropSource())
        {
            u64 id = ent.GetID();
            ImGui::SetDragDropPayload("RL_ENTITY", &id, sizeof(u64));
            ImGui::Text("Parenting %s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("RL_ENTITY"))
            {
                u64 droppedId = *static_cast<const u64*>(p->Data);
                for (Entity e : scene.GetAliveEntities())
                {
                    if (e.GetID() == droppedId)
                    {
                        scene.SetParent(e, ent);
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemClicked())
        {
            m_selectedEntity = ent;
        }

        if (ImGui::BeginPopupContextItem())
        {
            m_selectedEntity = ent;
            if (ImGui::MenuItem("Duplicate", "Shift+D"))
            {
                Entity clone = scene.CreateEntity(tag + ".001");
                if (scene.HasComponent<TransformComponent>(ent))
                {
                    scene.GetComponent<TransformComponent>(clone) = scene.GetComponent<TransformComponent>(ent);
                    scene.GetComponent<TransformComponent>(clone).Position.x += 1.5f;
                }
                m_selectedEntity = clone;
            }
            if (ImGui::MenuItem("Unparent"))
            {
                scene.SetParent(ent, Entity{});
            }
            if (ImGui::MenuItem("Delete", "X"))
            {
                scene.DestroyEntity(ent);
                if (m_selectedEntity == ent) m_selectedEntity = Entity{};
            }
            ImGui::EndPopup();
        }

        if (hasChildren && nodeOpen)
        {
            for (Entity child : rel->Children)
            {
                if (scene.IsAlive(child))
                {
                    RenderEntityNode(scene, child, filter);
                }
            }
            ImGui::TreePop();
        }
    }

    void SceneBuilder::RenderBlenderOutliner(Scene& scene, float x, float y, float width, float height)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Outliner", &m_showHierarchy, flags))
        {

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
            ImGui::InputTextWithHint("##Filter", "Search...", m_searchFilter, sizeof(m_searchFilter));

            ImGui::SameLine();
            if (ImGui::Button("+ Add", ImVec2(55.0f, 22.0f)))
            {
                m_openAddPopup = true;
            }

            ImGui::Separator();

            ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ImGui::TreeNodeEx("Scene Collection", rootFlags))
            {
                if (ImGui::TreeNodeEx("Collection", rootFlags))
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("RL_ENTITY"))
                        {
                            u64 droppedId = *static_cast<const u64*>(p->Data);
                            for (Entity e : scene.GetAliveEntities())
                            {
                                if (e.GetID() == droppedId)
                                {
                                    scene.SetParent(e, Entity{});
                                    break;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    const auto& entities = scene.GetAliveEntities();
                    const std::string_view filter(m_searchFilter);

                    std::vector<Entity> rootEntities;
                    rootEntities.reserve(entities.size());

                    for (const auto& ent : entities)
                    {
                        if (!scene.IsAlive(ent)) continue;

                        const bool isRoot = !scene.HasComponent<RelationshipComponent>(ent) ||
                                            !scene.GetComponent<RelationshipComponent>(ent).Parent.IsValid() ||
                                            !scene.IsValid(scene.GetComponent<RelationshipComponent>(ent).Parent);
                        if (!isRoot) continue;

                        if (!filter.empty())
                        {
                            std::string tag = "Entity";
                            if (scene.HasComponent<TagComponent>(ent))
                            {
                                tag = scene.GetComponent<TagComponent>(ent).Tag;
                            }
                            if (tag.find(filter) == std::string::npos && !HasMatchingDescendant(scene, ent, filter))
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
                            RenderEntityNode(scene, rootEntities[i], filter);
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
        }
        ImGui::End();
    }

    void SceneBuilder::RenderBlenderProperties(
        Scene& scene,
        const DX12Device& device,
        const Timer& timer,
        [[maybe_unused]] RenderSystem* renderSystem,
        [[maybe_unused]] TextureManager* textureManager,
        float x, float y, float width, float height)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("Properties", &m_showInspector, flags))
        {

            ImGui::BeginChild("##PropertiesTabBar", ImVec2(34.0f, 0), false, ImGuiWindowFlags_NoScrollbar);
            {
                auto TabIcon = [this](const char* icon, const char* tooltip, PropertiesTab tab) {
                    const bool active = (m_propertiesTab == tab);
                    if (active)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.910f, 0.490f, 0.051f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.70f, 1.0f));
                    }

                    if (ImGui::Button(icon, ImVec2(26.0f, 26.0f)))
                    {
                        m_propertiesTab = tab;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", tooltip);
                    }

                    ImGui::PopStyleColor(2);
                };

                TabIcon("[R]", "Render Settings", PropertiesTab::Render);
                TabIcon("[O]", "Output Dimensions", PropertiesTab::Output);
                TabIcon("[W]", "World & Environment", PropertiesTab::World);
                TabIcon("[D]", "Object Properties", PropertiesTab::Object);
                TabIcon("[M]", "Modifiers / Components", PropertiesTab::Modifiers);
                TabIcon("[*]", "Material BSDF", PropertiesTab::Material);
                TabIcon("[#]", "Mesh Geometry Data", PropertiesTab::Data);
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("##PropertiesContent", ImVec2(0, 0), false);
            {
                switch (m_propertiesTab)
                {
                    case PropertiesTab::Render:
                    {
                        ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "Render Settings");
                        ImGui::Separator();

                        const char* apiNames[] = { "DirectX 12 (Modern / Ultimate)", "DirectX 11 (Fallback / Legacy GTX 500-700)" };
                        int apiIdx = static_cast<int>(m_activeAPI);
                        if (ImGui::Combo("Backend API", &apiIdx, apiNames, IM_ARRAYSIZE(apiNames)))
                        {
                            m_activeAPI = static_cast<RHI::GraphicsAPI>(apiIdx);
                        }

                        ImGui::Spacing();
                        const auto& gpu = device.GetGpuInfo();
                        ImGui::Text("GPU: %s", gpu.name.c_str());
                        ImGui::Text("Architecture: %s", gpu.architectureName.c_str());
                        ImGui::Text("Generation: %s", gpu.generationName.c_str());
                        ImGui::Text("Feature Level: %s", gpu.GetFeatureLevelString().data());
                        ImGui::Text("Shader Model: %s", gpu.GetShaderModelString().data());
                        ImGui::Text("Dedicated VRAM: %.2f GB", gpu.GetDedicatedVramGB());

                        ImGui::Separator();
                        ImGui::Checkbox("VSync", &m_vsync);
                        ImGui::SliderInt("Framerate Target", &m_fpsTarget, 30, 240);

                        ImGui::Separator();
                        ImGui::Text("Hardware Capabilities:");
                        ImGui::BulletText("DirectX 12 Ultimate: %s", gpu.supportsD3D12Ultimate ? "YES" : "NO");
                        ImGui::BulletText("Raytracing Tier 1.1: %s", gpu.supportsRaytracing ? "YES" : "NO");
                        ImGui::BulletText("Mesh Shaders: %s", gpu.supportsMeshShaders ? "YES" : "NO");
                        ImGui::BulletText("Variable Rate Shading: %s", gpu.supportsVariableRateShading ? "YES" : "NO");
                        break;
                    }

                    case PropertiesTab::World:
                    {
                        ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "World Environment");
                        ImGui::Separator();
                        float col[4] = { m_worldSkyColor.x, m_worldSkyColor.y, m_worldSkyColor.z, m_worldSkyColor.w };
                        if (ImGui::ColorEdit4("Sky / Horizon", col))
                        {
                            m_worldSkyColor = XMFLOAT4(col[0], col[1], col[2], col[3]);
                        }
                        ImGui::SliderFloat("Ambient Strength", &m_ambientStrength, 0.0f, 2.0f);
                        ImGui::Checkbox("Atmospheric Fog", &m_enableFog);
                        if (m_enableFog)
                        {
                            ImGui::SliderFloat("Fog Density", &m_fogDensity, 0.001f, 0.1f);
                        }
                        break;
                    }

                    case PropertiesTab::Object:
                    {
                        ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "Object Transform");
                        ImGui::Separator();

                        if (!m_selectedEntity.IsValid() || !scene.IsAlive(m_selectedEntity))
                        {
                            ImGui::TextDisabled("No entity selected in Outliner.");
                            break;
                        }

                        if (scene.HasComponent<TagComponent>(m_selectedEntity))
                        {
                            auto& tag = scene.GetComponent<TagComponent>(m_selectedEntity);
                            char nameBuf[128]{};
                            std::copy_n(tag.Tag.c_str(), std::min(sizeof(nameBuf) - 1, tag.Tag.size()), nameBuf);
                            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                            {
                                tag.Tag = nameBuf;
                            }
                        }

                        if (scene.HasComponent<TransformComponent>(m_selectedEntity))
                        {
                            auto& t = scene.GetComponent<TransformComponent>(m_selectedEntity);

                            DrawVec3Control("Location", t.Position);

                            XMFLOAT3 rotDeg(XMConvertToDegrees(t.Rotation.x), XMConvertToDegrees(t.Rotation.y), XMConvertToDegrees(t.Rotation.z));
                            if (DrawVec3Control("Rotation", rotDeg))
                            {
                                t.Rotation.x = XMConvertToRadians(rotDeg.x);
                                t.Rotation.y = XMConvertToRadians(rotDeg.y);
                                t.Rotation.z = XMConvertToRadians(rotDeg.z);
                            }

                            DrawVec3Control("Scale", t.Scale, 1.0f);
                            ImGui::Checkbox("Lock Uniform Scale", &m_uniformScale);
                        }
                        break;
                    }

                    case PropertiesTab::Modifiers:
                    {
                        ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "Modifiers & Components");
                        ImGui::Separator();

                        if (!m_selectedEntity.IsValid() || !scene.IsAlive(m_selectedEntity))
                        {
                            ImGui::TextDisabled("No entity selected.");
                            break;
                        }

                        if (ImGui::Button("+ Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 24.0f)))
                        {
                            ImGui::OpenPopup("AddComponentPopup");
                        }

                        if (ImGui::BeginPopup("AddComponentPopup"))
                        {
                            if (ImGui::MenuItem("TransformComponent")) { scene.AddComponent<TransformComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("MeshComponent")) { scene.AddComponent<MeshComponent>(m_selectedEntity, "Cube"); }
                            if (ImGui::MenuItem("LightComponent")) { scene.AddComponent<LightComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("MaterialComponent")) { scene.AddComponent<MaterialComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("CameraComponent")) { scene.AddComponent<CameraComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("RigidBodyComponent")) { scene.AddComponent<RigidBodyComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("BoxColliderComponent")) { scene.AddComponent<BoxColliderComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("SphereColliderComponent")) { scene.AddComponent<SphereColliderComponent>(m_selectedEntity); }
                            if (ImGui::MenuItem("ParticleEmitterComponent")) { scene.AddComponent<ParticleEmitterComponent>(m_selectedEntity); }
                            ImGui::EndPopup();
                        }

                        ImGui::Spacing();

                        if (scene.HasComponent<MeshComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& mc = scene.GetComponent<MeshComponent>(m_selectedEntity);
                                ImGui::Text("Mesh Type: %s", mc.MeshName.c_str());
                                ImGui::Checkbox("Visible", &mc.IsVisible);
                                ImGui::Checkbox("Wireframe", &mc.Wireframe);
                                float c[4] = { mc.Color.x, mc.Color.y, mc.Color.z, mc.Color.w };
                                if (ImGui::ColorEdit4("Mesh Color", c))
                                {
                                    mc.Color = XMFLOAT4(c[0], c[1], c[2], c[3]);
                                }
                            }
                        }

                        if (scene.HasComponent<LightComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& lc = scene.GetComponent<LightComponent>(m_selectedEntity);
                                float c[3] = { lc.Color.x, lc.Color.y, lc.Color.z };
                                if (ImGui::ColorEdit3("Light Color", c))
                                {
                                    lc.Color = XMFLOAT3(c[0], c[1], c[2]);
                                }
                                ImGui::SliderFloat("Intensity", &lc.Intensity, 0.1f, 50.0f);
                                ImGui::SliderFloat("Range", &lc.Range, 1.0f, 100.0f);
                            }
                        }

                        if (scene.HasComponent<ScriptComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Script Component (Lua)", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& sc = scene.GetComponent<ScriptComponent>(m_selectedEntity);
                                ImGui::Text("Script: %s", sc.ScriptName.c_str());
                                ImGui::Checkbox("Enabled", &sc.Enabled);
                            }
                        }

                        if (scene.HasComponent<RigidBodyComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Rigid Body Component", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& rb = scene.GetComponent<RigidBodyComponent>(m_selectedEntity);
                                const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
                                int bType = static_cast<int>(rb.Type);
                                if (ImGui::Combo("Body Type", &bType, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
                                {
                                    rb.Type = static_cast<BodyType>(bType);
                                }
                                ImGui::DragFloat("Mass", &rb.Mass, 0.1f, 0.01f, 1000.0f);
                                ImGui::SliderFloat("Restitution", &rb.Restitution, 0.0f, 1.0f);
                                ImGui::SliderFloat("Friction", &rb.Friction, 0.0f, 1.0f);
                                ImGui::DragFloat("Linear Damping", &rb.LinearDamping, 0.01f, 0.0f, 10.0f);
                                ImGui::DragFloat("Angular Damping", &rb.AngularDamping, 0.01f, 0.0f, 10.0f);
                                ImGui::Checkbox("Use Gravity", &rb.UseGravity);
                                ImGui::DragFloat("Gravity Scale", &rb.GravityScale, 0.1f, -5.0f, 5.0f);
                                ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", rb.LinearVelocity.x, rb.LinearVelocity.y, rb.LinearVelocity.z);
                                ImGui::Text("Grounded: %s", rb.IsGrounded ? "True" : "False");
                            }
                        }

                        if (scene.HasComponent<BoxColliderComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Box Collider Component", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& bc = scene.GetComponent<BoxColliderComponent>(m_selectedEntity);
                                ImGui::DragFloat3("Half Extents", &bc.HalfSize.x, 0.05f, 0.01f, 100.0f);
                                ImGui::DragFloat3("Offset##Box", &bc.Offset.x, 0.05f);
                            }
                        }

                        if (scene.HasComponent<SphereColliderComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Sphere Collider Component", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& sc = scene.GetComponent<SphereColliderComponent>(m_selectedEntity);
                                ImGui::DragFloat("Radius", &sc.Radius, 0.05f, 0.01f, 100.0f);
                                ImGui::DragFloat3("Offset##Sphere", &sc.Offset.x, 0.05f);
                            }
                        }

                        if (scene.HasComponent<ParticleEmitterComponent>(m_selectedEntity))
                        {
                            if (ImGui::CollapsingHeader("Particle Emitter Component", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                auto& pe = scene.GetComponent<ParticleEmitterComponent>(m_selectedEntity);
                                ImGui::DragFloat("Emission Rate", &pe.EmissionRate, 5.0f, 0.0f, 5000.0f);
                                ImGui::DragFloat("Max Life", &pe.Lifetime, 0.05f, 0.05f, 30.0f);
                                ImGui::DragFloat("Speed", &pe.Speed, 0.1f, 0.0f, 50.0f);
                                ImGui::SliderFloat("Cone Angle", &pe.ConeAngle, 0.0f, 180.0f);
                                ImGui::ColorEdit4("Start Color", &pe.StartColor.x);
                                ImGui::ColorEdit4("End Color", &pe.EndColor.x);
                                ImGui::DragFloat("Start Size", &pe.StartSize, 0.01f, 0.001f, 10.0f);
                                ImGui::DragFloat("End Size", &pe.EndSize, 0.01f, 0.001f, 10.0f);
                                ImGui::Checkbox("Floor Bounce", &pe.FloorBounce);
                                ImGui::Checkbox("Looping", &pe.Looping);
                            }
                        }
                        break;
                    }

                    case PropertiesTab::Material:
                    {
                        ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "Material (Principled BSDF)");
                        ImGui::Separator();

                        if (!m_selectedEntity.IsValid() || !scene.IsAlive(m_selectedEntity))
                        {
                            ImGui::TextDisabled("No entity selected.");
                            break;
                        }

                        if (!scene.HasComponent<MaterialComponent>(m_selectedEntity))
                        {
                            if (ImGui::Button("+ Add Material Component"))
                            {
                                scene.AddComponent<MaterialComponent>(m_selectedEntity);
                            }
                        }
                        else
                        {
                            auto& mat = scene.GetComponent<MaterialComponent>(m_selectedEntity);
                            if (ImGui::ColorEdit4("Base Color", &mat.BaseColor.x))
                            {
                                mat.AlbedoColor = mat.BaseColor;
                            }
                            ImGui::SliderFloat("Metallic", &mat.Metallic, 0.0f, 1.0f);
                            ImGui::SliderFloat("Roughness", &mat.Roughness, 0.0f, 1.0f);
                            ImGui::SliderFloat("AO", &mat.AO, 0.0f, 1.0f);
                            ImGui::SliderFloat("Normal Strength", &mat.NormalStrength, 0.0f, 2.0f);
                            ImGui::SliderFloat("Emission Strength", &mat.EmissionStrength, 0.0f, 20.0f);
                        }

                        if (scene.HasComponent<MeshComponent>(m_selectedEntity))
                        {
                            auto& mc = scene.GetComponent<MeshComponent>(m_selectedEntity);
                            int texIdx = static_cast<int>(mc.TextureIndex);
                            if (ImGui::InputInt("Texture Slot", &texIdx))
                            {
                                mc.TextureIndex = static_cast<u32>((std::max)(0, texIdx));
                            }
                        }
                        break;
                    }

                    case PropertiesTab::Data:
                    {
                        ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "Mesh Geometry Data");
                        ImGui::Separator();
                        if (m_selectedEntity.IsValid() && scene.HasComponent<MeshComponent>(m_selectedEntity))
                        {
                            auto& mc = scene.GetComponent<MeshComponent>(m_selectedEntity);
                            ImGui::Text("Mesh Asset: %s", mc.MeshName.c_str());
                            if (mc.MeshRef)
                            {
                                ImGui::Text("Vertices: %u", mc.MeshRef->GetVertexCount());
                                ImGui::Text("Indices: %u", mc.MeshRef->GetIndexCount());
                                ImGui::Text("Triangles: %u", mc.MeshRef->GetIndexCount() / 3);
                            }
                            else
                            {
                                ImGui::Text("Standard Procedural Primitive");
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("Select an entity with MeshComponent.");
                        }
                        break;
                    }

                    default:
                        break;
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void SceneBuilder::RenderBlenderTimeline(Scene& scene, float x, float y, float width, float height)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.13f, 0.98f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);

        if (ImGui::Begin("##BlenderTimeline", nullptr, flags))
        {

            if (ImGui::Button("|<<", ImVec2(26, 22))) { m_currentFrame = m_startFrame; }
            ImGui::SameLine(0, 3);
            if (ImGui::Button("<|", ImVec2(26, 22))) { m_currentFrame = (std::max)(m_startFrame, m_currentFrame - 1); }
            ImGui::SameLine(0, 3);

            if (m_simulationState == SimulationState::Playing)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.25f, 1.0f));
            }
            if (ImGui::Button(" [>] ", ImVec2(34, 22)))
            {
                Play(scene);
            }
            if (m_simulationState == SimulationState::Playing)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Play");
            }

            ImGui::SameLine(0, 3);

            if (m_simulationState == SimulationState::Paused)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.70f, 0.10f, 1.0f));
            }
            if (ImGui::Button(" [||] ", ImVec2(34, 22)))
            {
                Pause();
            }
            if (m_simulationState == SimulationState::Paused)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Pause");
            }

            ImGui::SameLine(0, 3);

            if (m_simulationState != SimulationState::Edit)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
            }
            if (ImGui::Button(" [[]] ", ImVec2(34, 22)))
            {
                Stop(scene);
            }
            if (m_simulationState != SimulationState::Edit)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Stop");
            }

            ImGui::SameLine(0, 3);
            if (ImGui::Button("|>", ImVec2(26, 22))) { m_currentFrame = std::min(m_endFrame, m_currentFrame + 1); }
            ImGui::SameLine(0, 3);
            if (ImGui::Button(">>|", ImVec2(26, 22))) { m_currentFrame = m_endFrame; }

            ImGui::SameLine(0, 14);
            ImGui::SetNextItemWidth(45);
            ImGui::InputInt("##StartFrame", &m_startFrame, 0);

            ImGui::SameLine(0, 6);
            const float sliderWidth = std::max(100.0f, width - 420.0f);
            ImGui::SetNextItemWidth(sliderWidth);
            ImGui::SliderInt("##CurrentFrameSlider", &m_currentFrame, m_startFrame, m_endFrame, "Frame %d");

            ImGui::SameLine(0, 6);
            ImGui::SetNextItemWidth(45);
            ImGui::InputInt("##EndFrame", &m_endFrame, 0);

            ImGui::SameLine(0, 10);
            ImGui::TextDisabled("%d fps", m_fpsTarget);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void SceneBuilder::RenderBlenderStatusBar(
        const Scene& scene,
        const Timer& timer,
        const DX12Device& device,
        float x, float y, float width, float height)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 3.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));

        if (ImGui::Begin("##BlenderStatusBar", nullptr, flags))
        {

            const char* toolName = "Select Box";
            switch (m_activeTool)
            {
                case BlenderTool::Move: toolName = "Move"; break;
                case BlenderTool::Rotate: toolName = "Rotate"; break;
                case BlenderTool::Scale: toolName = "Scale"; break;
                case BlenderTool::Cursor: toolName = "3D Cursor"; break;
                default: break;
            }

            std::string selectedName = "None";
            if (m_selectedEntity.IsValid() && scene.IsAlive(m_selectedEntity))
            {
                if (scene.HasComponent<TagComponent>(m_selectedEntity))
                {
                    selectedName = scene.GetComponent<TagComponent>(m_selectedEntity).Tag;
                }
            }

            ImGui::TextDisabled("Tool: %s | Selected: %s | Mode: %s",
                                toolName,
                                selectedName.c_str(),
                                (m_mode == BlenderMode::ObjectMode) ? "Object Mode" : "Edit Mode");

            if (width > 950.0f)
            {
                ImGui::SameLine(width * 0.35f);
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f),
                                   "[G] Move  [R] Rotate  [S] Scale  [Shift+A] Add  [X] Del  [Space] Play  [Tab] Mode");
            }

            const float rightWidth = 320.0f;
            if (width > rightWidth + 300.0f)
            {
                ImGui::SameLine(width - rightWidth);
            }
            ImGui::TextDisabled("Objects: %zu | FPS: %u (%.1f ms) | GPU: %s",
                                scene.GetAliveEntityCount(),
                                timer.GetFPS(),
                                timer.GetFrameTimeMS(),
                                device.GetGpuInfo().architectureName.c_str());
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void SceneBuilder::RenderBlenderAddPopup(
        Scene& scene,
        [[maybe_unused]] ID3D12Device* device,
        [[maybe_unused]] RenderSystem* renderSystem,
        StressTest& stressTest)
    {
        ImGui::OpenPopup("##BlenderAddMenuPopup");
        if (ImGui::BeginPopup("##BlenderAddMenuPopup"))
        {
            ImGui::TextColored(ImVec4(0.91f, 0.49f, 0.051f, 1.0f), "Add Object");
            ImGui::Separator();

            if (ImGui::BeginMenu("Mesh"))
            {
                if (ImGui::MenuItem("Cube"))
                {
                    Entity e = scene.CreateEntity("Cube");
                    scene.AddComponent<MeshComponent>(e, "Cube");
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Sphere"))
                {
                    Entity e = scene.CreateEntity("Sphere");
                    scene.AddComponent<MeshComponent>(e, "Sphere");
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Plane"))
                {
                    Entity e = scene.CreateEntity("Plane");
                    scene.AddComponent<MeshComponent>(e, "Plane");
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Grid"))
                {
                    Entity e = scene.CreateEntity("Grid");
                    scene.AddComponent<MeshComponent>(e, "Grid");
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("2D Sprite Quad"))
                {
                    Entity e = scene.CreateEntity("SpriteQuad");
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light"))
            {
                if (ImGui::MenuItem("Point Light"))
                {
                    Entity e = scene.CreateEntity("PointLight");
                    scene.AddComponent<LightComponent>(e, LightType::Point, XMFLOAT3{1.0f, 0.95f, 0.8f}, 8.0f);
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Sun / Directional Light"))
                {
                    Entity e = scene.CreateEntity("SunLight");
                    scene.AddComponent<LightComponent>(e, LightType::Directional, XMFLOAT3{1.0f, 1.0f, 0.9f}, 5.0f);
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Camera"))
            {
                if (ImGui::MenuItem("Perspective Camera"))
                {
                    Entity e = scene.CreateEntity("Camera");
                    scene.AddComponent<CameraComponent>(e);
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Physics"))
            {
                if (ImGui::MenuItem("Rigid Body Dynamic Cube"))
                {
                    Entity e = scene.CreateEntity("RigidBodyCube");
                    scene.AddComponent<MeshComponent>(e, "Cube");
                    auto& rb = scene.AddComponent<RigidBodyComponent>(e);
                    rb.Type = BodyType::Dynamic;
                    rb.Mass = 1.0f;
                    rb.Restitution = 0.6f;
                    auto& bc = scene.AddComponent<BoxColliderComponent>(e);
                    bc.HalfSize = { 0.5f, 0.5f, 0.5f };
                    auto& t = scene.GetComponent<TransformComponent>(e);
                    t.Position.y = 5.0f;
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Rigid Body Dynamic Sphere"))
                {
                    Entity e = scene.CreateEntity("RigidBodySphere");
                    scene.AddComponent<MeshComponent>(e, "Sphere");
                    auto& rb = scene.AddComponent<RigidBodyComponent>(e);
                    rb.Type = BodyType::Dynamic;
                    rb.Mass = 1.0f;
                    rb.Restitution = 0.75f;
                    auto& sc = scene.AddComponent<SphereColliderComponent>(e);
                    sc.Radius = 0.5f;
                    auto& t = scene.GetComponent<TransformComponent>(e);
                    t.Position.y = 7.0f;
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Effects"))
            {
                if (ImGui::MenuItem("Particle Emitter"))
                {
                    Entity e = scene.CreateEntity("ParticleEmitter");
                    auto& pe = scene.AddComponent<ParticleEmitterComponent>(e);
                    pe.EmissionRate = 200.0f;
                    pe.Speed = 6.0f;
                    pe.FloorBounce = true;
                    auto& t = scene.GetComponent<TransformComponent>(e);
                    t.Position.y = 1.0f;
                    m_selectedEntity = e;
                    m_openAddPopup = false;
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::BeginMenu("GPU Stress Benchmark"))
            {
                if (ImGui::MenuItem("Spawn 5,000 Entities"))
                {
                    stressTest.Spawn(scene, 5000);
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Spawn 50,000 Entities"))
                {
                    stressTest.Spawn(scene, 50000);
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Spawn 100,000 Entities"))
                {
                    stressTest.Spawn(scene, 100000);
                    m_openAddPopup = false;
                }
                if (ImGui::MenuItem("Spawn 1,000,000 Entities (1M)"))
                {
                    stressTest.Spawn(scene, 1000000);
                    m_openAddPopup = false;
                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
        else
        {
            m_openAddPopup = false;
        }
    }

    void SceneBuilder::HandleBlenderHotkeys(Scene& scene)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput) return;

        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            m_openAddPopup = true;
        }

        if (io.KeyCtrl && !io.KeyAlt && !io.KeyShift)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            {
                SaveScene(scene, "assets/scenes/default.rlscene");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_O, false))
            {
                LoadScene(scene, m_dx12Device, "assets/scenes/default.rlscene");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                Editor::CommandHistory::Undo();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                Editor::CommandHistory::Redo();
            }
        }

        if (!io.KeyCtrl && !io.KeyAlt && !io.KeyShift)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_G, false)) { m_activeTool = BlenderTool::Move; }
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) { m_activeTool = BlenderTool::Rotate; }
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) { m_activeTool = BlenderTool::Scale; }
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) { m_activeTool = BlenderTool::SelectBox; }

            if (ImGui::IsKeyPressed(ImGuiKey_X, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                if (m_selectedEntity.IsValid() && scene.IsAlive(m_selectedEntity))
                {
                    scene.DestroyEntity(m_selectedEntity);
                    m_selectedEntity = Entity{};
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
            {
                if (m_simulationState == SimulationState::Playing)
                {
                    Pause();
                }
                else
                {
                    Play(scene);
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Tab, false))
            {
                m_mode = (m_mode == BlenderMode::ObjectMode) ? BlenderMode::EditMode : BlenderMode::ObjectMode;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_T, false))
            {
                m_showToolShelf = !m_showToolShelf;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_N, false))
            {
                m_showInspector = !m_showInspector;
            }

            auto& cam = scene.GetCamera();
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false))
            {
                cam.SetPosition(DirectX::XMFLOAT3(0.0f, 0.0f, -10.0f));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false))
            {
                cam.SetPosition(DirectX::XMFLOAT3(10.0f, 0.0f, -10.0f));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false))
            {
                cam.SetPosition(DirectX::XMFLOAT3(0.0f, 10.0f, -10.0f));
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Keypad5, false))
            {
                const auto curProj = cam.GetProjectionType();
                cam.SetProjectionType(curProj == CameraProjection::Perspective ? CameraProjection::Orthographic : CameraProjection::Perspective);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal, false) || ImGui::IsKeyPressed(ImGuiKey_F, false))
            {
                if (m_selectedEntity.IsValid() && scene.HasComponent<TransformComponent>(m_selectedEntity))
                {
                    const auto& t = scene.GetComponent<TransformComponent>(m_selectedEntity);
                    cam.SetPosition(DirectX::XMFLOAT3(t.Position.x, t.Position.y, t.Position.z - 5.0f));
                }
            }
        }
    }

    bool SceneBuilder::DrawVec3Control(
        const char* label,
        XMFLOAT3& values,
        float resetValue,
        float columnWidth)
    {
        bool changed = false;

        ImGui::PushID(label);

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text("%s", label);
        ImGui::NextColumn();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        const float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        const ImVec2 buttonSize = { lineHeight + 2.0f, lineHeight };
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float itemWidth = (std::max)(35.0f, (totalWidth - (buttonSize.x * 3.0f) - 8.0f) / 3.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button("X", buttonSize))
        {
            values.x = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
        {
            changed = true;
        }
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.63f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.75f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.63f, 0.28f, 1.0f));
        if (ImGui::Button("Y", buttonSize))
        {
            values.y = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
        {
            changed = true;
        }
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.45f, 0.78f, 1.0f));
        if (ImGui::Button("Z", buttonSize))
        {
            values.z = resetValue;
            changed = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f"))
        {
            changed = true;
        }

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();

        return changed;
    }

    Entity SceneBuilder::PickEntityAtScreenPos(
        Scene& scene,
        const ImVec2& screenPos,
        const ImVec2& vpPos,
        const ImVec2& vpSize)
    {
        if (vpSize.x <= 1.0f || vpSize.y <= 1.0f)
        {
            return Entity{};
        }

        const float ndcX = ((screenPos.x - vpPos.x) / vpSize.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((screenPos.y - vpPos.y) / vpSize.y) * 2.0f;

        const auto& cam = scene.GetCamera();
        const XMMATRIX view = cam.CalculateViewMatrix();
        const XMMATRIX proj = cam.CalculateProjectionMatrix();
        const XMMATRIX viewProj = XMMatrixMultiply(view, proj);
        XMVECTOR det;
        const XMMATRIX invViewProj = XMMatrixInverse(&det, viewProj);

        XMVECTOR nearPointClip = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
        XMVECTOR farPointClip = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);

        XMVECTOR nearWorld = XMVector4Transform(nearPointClip, invViewProj);
        XMVECTOR farWorld = XMVector4Transform(farPointClip, invViewProj);

        XMFLOAT4 nw, fw;
        XMStoreFloat4(&nw, nearWorld);
        XMStoreFloat4(&fw, farWorld);

        if (std::abs(nw.w) < 1e-6f || std::abs(fw.w) < 1e-6f)
        {
            return Entity{};
        }

        XMFLOAT3 rayOrigin(nw.x / nw.w, nw.y / nw.w, nw.z / nw.w);
        XMFLOAT3 rayFar(fw.x / fw.w, fw.y / fw.w, fw.z / fw.w);

        XMFLOAT3 rayDir(rayFar.x - rayOrigin.x, rayFar.y - rayOrigin.y, rayFar.z - rayOrigin.z);
        float len = std::sqrt(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);
        if (len < 1e-6f)
        {
            return Entity{};
        }
        rayDir.x /= len;
        rayDir.y /= len;
        rayDir.z /= len;

        auto hit = m_physicsSystem.Raycast(scene, rayOrigin, rayDir, 1000.0f);
        if (hit && hit->HitEntity.IsValid())
        {
            return hit->HitEntity;
        }

        return Entity{};
    }

    bool SceneBuilder::SaveScene(const Scene& scene, const std::filesystem::path& path)
    {
        return SceneSerializer::Save(scene, path);
    }

    bool SceneBuilder::LoadScene(Scene& scene, ID3D12Device* device, const std::filesystem::path& path)
    {
        m_selectedEntity = Entity{};
        return SceneSerializer::Load(scene, device, path);
    }
}
