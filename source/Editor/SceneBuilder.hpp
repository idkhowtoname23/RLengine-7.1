#pragma once

#include "Core/Types.hpp"
#include "Scene/Entity.hpp"
#include "Scene/StressTest.hpp"
#include "Scene/Systems/PhysicsSystem.hpp"
#include "Graphics/RHI/RHICommon.hpp"
#include "Graphics/DX12/ParticleSystem.hpp"
#include "Editor/Gizmo/GizmoSystem.hpp"
#include "Editor/History/CommandHistory.hpp"

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

struct ID3D12Device;
struct ImDrawList;
struct ImVec2;

namespace RL
{
    class Scene;
    class DX12Device;
    class Timer;
    class RenderSystem;
    class TextureManager;
    class ScriptEngine;

    enum class BlenderWorkspace : u8
    {
        Layout = 0,
        Modeling,
        Shading,
        Animation,
        Scripting,
        Diagnostics
    };

    enum class BlenderMode : u8
    {
        ObjectMode = 0,
        EditMode,
        SculptMode
    };

    enum class ViewportShadingMode : u8
    {
        Wireframe = 0,
        Solid,
        MaterialPreview,
        Rendered
    };

    enum class BlenderTool : u8
    {
        SelectBox = 0,
        Cursor,
        Move,
        Rotate,
        Scale,
        Transform,
        Annotate,
        Measure,
        AddPrimitive
    };

    enum class PropertiesTab : u8
    {
        Render = 0,
        Output,
        World,
        Object,
        Modifiers,
        Material,
        Data
    };

    enum class TransformSpace : u8
    {
        Global = 0,
        Local
    };

    enum class SimulationState : u8
    {
        Edit = 0,
        Playing,
        Paused
    };

    class SceneBuilder
    {
    public:
        SceneBuilder() = default;
        ~SceneBuilder() = default;

        void Render(
            Scene& scene,
            const DX12Device& device,
            const Timer& timer,
            RenderSystem* renderSystem,
            TextureManager* textureManager,
            ScriptEngine* scriptEngine,
            StressTest& stressTest);

        [[nodiscard]] bool IsSimulationRunning() const noexcept { return m_simulationState == SimulationState::Playing; }
        void SetSimulationRunning(bool running) noexcept { m_simulationState = running ? SimulationState::Playing : SimulationState::Edit; }

        [[nodiscard]] SimulationState GetSimulationState() const noexcept { return m_simulationState; }
        void SetSimulationState(SimulationState state) noexcept { m_simulationState = state; }

        void Play(Scene& scene) noexcept;
        void Pause() noexcept;
        void Stop(Scene& scene) noexcept;

        [[nodiscard]] Entity GetSelectedEntity() const noexcept { return m_selectedEntity; }
        void SelectEntity(Entity entity) noexcept { m_selectedEntity = entity; }

        [[nodiscard]] bool IsOpen() const noexcept { return m_isOpen; }
        void SetOpen(bool open) noexcept { m_isOpen = open; }
        void ToggleOpen() noexcept { m_isOpen = !m_isOpen; }

        [[nodiscard]] BlenderWorkspace GetWorkspace() const noexcept { return m_workspace; }
        void SetWorkspace(BlenderWorkspace ws) noexcept { m_workspace = ws; }

        [[nodiscard]] BlenderTool GetActiveTool() const noexcept { return m_activeTool; }
        void SetActiveTool(BlenderTool tool) noexcept { m_activeTool = tool; }

        [[nodiscard]] PhysicsSystem& GetPhysicsSystem() noexcept { return m_physicsSystem; }
        [[nodiscard]] ParticleSystem& GetParticleSystem() noexcept { return m_particleSystem; }

        [[nodiscard]] bool IsVSyncEnabled() const noexcept { return m_vsync; }
        void SetVSyncEnabled(bool enabled) noexcept { m_vsync = enabled; }

        bool SaveScene(const Scene& scene, const std::filesystem::path& path);
        bool LoadScene(Scene& scene, ID3D12Device* device, const std::filesystem::path& path);

    private:

        void RenderBlenderTopHeader(
            Scene& scene,
            const DX12Device& device,
            RenderSystem* renderSystem,
            StressTest& stressTest);

        void RenderBlenderToolShelf(float x, float y);

        void RenderBlenderViewportOverlays(
            Scene& scene,
            const Timer& timer,
            const DX12Device& device,
            RenderSystem* renderSystem,
            float x, float y, float width, float height);

        void RenderBlenderOutliner(Scene& scene, float x, float y, float width, float height);

        void RenderBlenderProperties(
            Scene& scene,
            const DX12Device& device,
            const Timer& timer,
            RenderSystem* renderSystem,
            TextureManager* textureManager,
            float x, float y, float width, float height);

        void RenderBlenderTimeline(Scene& scene, float x, float y, float width, float height);

        void RenderBlenderStatusBar(
            const Scene& scene,
            const Timer& timer,
            const DX12Device& device,
            float x, float y, float width, float height);

        void RenderBlenderAddPopup(
            Scene& scene,
            ID3D12Device* device,
            RenderSystem* renderSystem,
            StressTest& stressTest);

        void HandleBlenderHotkeys(Scene& scene);

        void Draw3DCompassGizmo(
            Scene& scene,
            const DirectX::XMFLOAT2& screenCenter,
            float radius);

        bool DrawVec3Control(
            const char* label,
            DirectX::XMFLOAT3& values,
            float resetValue = 0.0f,
            float columnWidth = 80.0f);

        Entity PickEntityAtScreenPos(
            Scene& scene,
            const ImVec2& screenPos,
            const ImVec2& vpPos,
            const ImVec2& vpSize);

    private:
        Editor::GizmoSystem m_gizmo;
        PhysicsSystem m_physicsSystem;
        ParticleSystem m_particleSystem;

        bool m_isOpen = true;
        bool m_showHierarchy = true;
        bool m_showInspector = true;
        bool m_showAssetBrowser = true;
        bool m_showViewportSettings = true;
        bool m_showStressTest = true;
        bool m_showToolShelf = true;
        bool m_showTimeline = true;
        bool m_showStatusBar = true;
        bool m_show3DCompass = true;

        SimulationState m_simulationState = SimulationState::Edit;
        Entity m_selectedEntity{};

        BlenderWorkspace m_workspace = BlenderWorkspace::Layout;
        BlenderMode m_mode = BlenderMode::ObjectMode;
        ViewportShadingMode m_shadingMode = ViewportShadingMode::Solid;
        BlenderTool m_activeTool = BlenderTool::SelectBox;
        PropertiesTab m_propertiesTab = PropertiesTab::Object;
        TransformSpace m_transformSpace = TransformSpace::Global;

        DirectX::XMFLOAT3 m_cursor3D{ 0.0f, 0.0f, 0.0f };
        int m_currentFrame = 1;
        int m_startFrame = 1;
        int m_endFrame = 250;
        int m_fpsTarget = 60;
        float m_playbackSpeed = 1.0f;

        DirectX::XMFLOAT4 m_worldSkyColor{ 0.04f, 0.06f, 0.09f, 1.0f };
        float m_ambientStrength = 0.5f;
        bool m_enableFog = false;
        float m_fogDensity = 0.02f;
        bool m_vsync = true;
        int m_msaaSamples = 1;

        RHI::GraphicsAPI m_activeAPI = RHI::GraphicsAPI::DirectX12;

        char m_searchFilter[128]{};
        char m_gltfPathInput[256] = "assets/models/cube.gltf";
        int m_stressCountSlider = 5000;
        bool m_uniformScale = true;
        bool m_openAddPopup = false;
        ID3D12Device* m_dx12Device = nullptr;
        bool m_gizmoDragging = false;
        TransformComponent m_gizmoStartTransform{};
        Entity m_gizmoTargetEntity{};

        void RenderEntityNode(Scene& scene, Entity ent, std::string_view filter);
        [[nodiscard]] bool HasMatchingDescendant(const Scene& scene, Entity ent, std::string_view filter) const;
    };
}
