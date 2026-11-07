#pragma once

#include "Core/Types.hpp"
#include "Graphics/DX12/DX12Common.hpp"
#include "Editor/LuaConsole.hpp"
#include "Editor/SceneBuilder.hpp"
#include "Scene/StressTest.hpp"
#include "Editor/UI/PanelManager.hpp"
#include "Editor/Panels/SceneHierarchyPanel.hpp"
#include "Editor/Panels/InspectorPanel.hpp"
#include "Editor/Panels/ContentBrowserPanel.hpp"
#include "Editor/Panels/StatsPanel.hpp"

#include <expected>
#include <string>

struct HWND__;
typedef HWND__* HWND;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12DescriptorHeap;

namespace RL
{
    class Timer;
    class DX12Device;
    class Scene;
    class ScriptEngine;
    class RenderSystem;
    class TextureManager;

    class EditorLayer
    {
    public:
        EditorLayer() = default;
        ~EditorLayer();

        EditorLayer(const EditorLayer&) = delete;
        EditorLayer& operator=(const EditorLayer&) = delete;

        EditorLayer(EditorLayer&&) noexcept = default;
        EditorLayer& operator=(EditorLayer&&) noexcept = default;

        [[nodiscard]] std::expected<void, std::string> Initialize(
            HWND hwnd,
            ID3D12Device* device,
            u32 numFrames,
            DXGI_FORMAT rtvFormat,
            ScriptEngine* scriptEngine = nullptr);

        void Shutdown() noexcept;

        void BeginFrame();
        void RenderUI(
            const Timer& timer,
            const DX12Device& device,
            Scene& scene,
            u32 renderedEntities,
            RenderSystem* renderSystem = nullptr,
            TextureManager* textureManager = nullptr,
            ScriptEngine* scriptEngine = nullptr);
        void EndFrame(ID3D12GraphicsCommandList* cmdList);

        [[nodiscard]] LuaConsole& GetLuaConsole() noexcept { return m_luaConsole; }
        [[nodiscard]] const LuaConsole& GetLuaConsole() const noexcept { return m_luaConsole; }

        [[nodiscard]] SceneBuilder& GetSceneBuilder() noexcept { return m_sceneBuilder; }
        [[nodiscard]] const SceneBuilder& GetSceneBuilder() const noexcept { return m_sceneBuilder; }

        [[nodiscard]] StressTest& GetStressTest() noexcept { return m_stressTest; }
        [[nodiscard]] const StressTest& GetStressTest() const noexcept { return m_stressTest; }

        [[nodiscard]] Editor::PanelManager& GetPanelManager() noexcept { return m_panelManager; }
        void ToggleBlenderPanels() noexcept { m_showBlenderPanels = !m_showBlenderPanels; }

        [[nodiscard]] bool IsInitialized() const noexcept { return m_isInitialized; }

    private:
        void ApplyCyberpunkDarkStyle();
        void RenderDiagnosticsOverlay(const Timer& timer, const DX12Device& device, const Scene& scene, u32 renderedEntities);

    private:
        ComPtr<ID3D12DescriptorHeap> m_srvHeap;
        LuaConsole m_luaConsole;
        SceneBuilder m_sceneBuilder;
        StressTest m_stressTest;
        Editor::PanelManager m_panelManager;
        Editor::SceneHierarchyPanel* m_hierarchyPanel = nullptr;
        Editor::InspectorPanel* m_inspectorPanel = nullptr;
        Editor::ContentBrowserPanel* m_contentBrowserPanel = nullptr;
        Editor::StatsPanel* m_statsPanel = nullptr;
        bool m_isInitialized = false;
        bool m_showDiagnostics = false;
        bool m_showBlenderPanels = false;
    };
}
