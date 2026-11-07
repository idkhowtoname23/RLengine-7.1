#include "Editor/EditorLayer.hpp"
#include "Editor/UI/Theme.hpp"
#include "Graphics/DX12/DX12Device.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Core/Timer.hpp"
#include "Core/Logger.hpp"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
#include <format>

namespace RL
{
    EditorLayer::~EditorLayer()
    {
        Shutdown();
    }

    std::expected<void, std::string> EditorLayer::Initialize(
        HWND hwnd,
        ID3D12Device* device,
        u32 numFrames,
        DXGI_FORMAT rtvFormat,
        ScriptEngine* scriptEngine)
    {
        if (m_isInitialized)
        {
            return {};
        }

        if (!hwnd || !device)
        {
            return std::unexpected("Invalid HWND or ID3D12Device passed to EditorLayer::Initialize.");
        }

        D3D12_DESCRIPTOR_HEAP_DESC descHeap{};
        descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descHeap.NumDescriptors = 16;
        descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descHeap.NodeMask = 0;

        HRESULT hr = device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&m_srvHeap));
        if (FAILED(hr))
        {
            return std::unexpected(std::format("Failed to create ImGui descriptor heap. HRESULT: 0x{:08X}", static_cast<u32>(hr)));
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        Editor::Theme::Apply(Editor::EditorTheme::BlenderDark);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        if (!ImGui_ImplWin32_Init(hwnd))
        {
            return std::unexpected("ImGui_ImplWin32_Init failed.");
        }

        const auto cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        const auto gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

        if (!ImGui_ImplDX12_Init(device, static_cast<int>(numFrames), rtvFormat, m_srvHeap.Get(), cpuHandle, gpuHandle))
        {
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            return std::unexpected("ImGui_ImplDX12_Init failed.");
        }

        m_luaConsole.SetScriptEngine(scriptEngine);

        m_hierarchyPanel = m_panelManager.AddPanel<Editor::SceneHierarchyPanel>(nullptr);
        m_inspectorPanel = m_panelManager.AddPanel<Editor::InspectorPanel>(nullptr);
        m_contentBrowserPanel = m_panelManager.AddPanel<Editor::ContentBrowserPanel>();
        m_statsPanel = m_panelManager.AddPanel<Editor::StatsPanel>();

        m_hierarchyPanel->SetSelectionCallback([this](Entity ent) {
            if (m_inspectorPanel)
            {
                m_inspectorPanel->SetSelected(ent.IsValid() ? std::make_optional(ent) : std::nullopt);
            }
            m_sceneBuilder.SelectEntity(ent);
        });

        Logger::SetLogCallback([this](LogLevel level, std::string_view message) {
            m_luaConsole.AddLog(std::string(message), level, level >= LogLevel::Error);
        });

        m_isInitialized = true;
        RL_LOG_INFO("[EditorLayer] Dear ImGui (v{}) with DX12 backend initialized successfully.", IMGUI_VERSION);
        return {};
    }

    void EditorLayer::Shutdown() noexcept
    {
        if (!m_isInitialized)
        {
            return;
        }

        Logger::RemoveLogCallback();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        m_srvHeap.Reset();
        m_isInitialized = false;

        RL_LOG_INFO("[EditorLayer] Dear ImGui subsystem shut down cleanly.");
    }

    void EditorLayer::BeginFrame()
    {
        if (!m_isInitialized)
        {
            return;
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false) || ImGui::IsKeyPressed(ImGuiKey_F1, false))
        {
            m_luaConsole.Toggle();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            m_sceneBuilder.ToggleOpen();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false))
        {
            m_showBlenderPanels = !m_showBlenderPanels;
        }
    }

    void EditorLayer::RenderUI(
        const Timer& timer,
        const DX12Device& device,
        Scene& scene,
        u32 renderedEntities,
        RenderSystem* renderSystem,
        TextureManager* textureManager,
        ScriptEngine* scriptEngine)
    {
        if (!m_isInitialized)
        {
            return;
        }

        m_sceneBuilder.Render(scene, device, timer, renderSystem, textureManager, scriptEngine, m_stressTest);

        if (m_showBlenderPanels)
        {
            if (m_hierarchyPanel)
            {
                m_hierarchyPanel->SetScene(&scene);
                const Entity sel = m_sceneBuilder.GetSelectedEntity();
                m_hierarchyPanel->SetSelected(sel.IsValid() ? std::make_optional(sel) : std::nullopt);
            }
            if (m_inspectorPanel)
            {
                m_inspectorPanel->SetScene(&scene);
                const Entity sel = m_sceneBuilder.GetSelectedEntity();
                m_inspectorPanel->SetSelected(sel.IsValid() ? std::make_optional(sel) : std::nullopt);
            }
            if (m_statsPanel)
            {
                m_statsPanel->SetStats(
                    timer.GetFPS(),
                    timer.GetDeltaTime() * 1000.0f,
                    scene.GetAliveEntityCount(),
                    static_cast<u64>(renderedEntities),
                    1u,
                    static_cast<u64>(renderedEntities * 12)
                );
            }
            m_panelManager.RenderAll();
        }

        RenderDiagnosticsOverlay(timer, device, scene, renderedEntities);

        m_luaConsole.Render();
    }

    void EditorLayer::EndFrame(ID3D12GraphicsCommandList* cmdList)
    {
        if (!m_isInitialized || !cmdList)
        {
            return;
        }

        ImGui::Render();

        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        cmdList->SetDescriptorHeaps(1, heaps);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
    }

    void EditorLayer::ApplyCyberpunkDarkStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding    = 8.0f;
        style.FrameRounding     = 5.0f;
        style.PopupRounding     = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding      = 4.0f;
        style.TabRounding       = 6.0f;
        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 1.0f;
        style.WindowPadding     = ImVec2(10.0f, 10.0f);
        style.FramePadding      = ImVec2(8.0f, 4.0f);
        style.ItemSpacing       = ImVec2(8.0f, 6.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.65f, 1.00f);
        colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.08f, 0.12f, 0.92f);
        colors[ImGuiCol_ChildBg]               = ImVec4(0.04f, 0.05f, 0.08f, 0.90f);
        colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.10f, 0.15f, 0.96f);
        colors[ImGuiCol_Border]                = ImVec4(0.18f, 0.25f, 0.38f, 0.70f);
        colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        colors[ImGuiCol_FrameBg]               = ImVec4(0.10f, 0.14f, 0.20f, 0.85f);
        colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.15f, 0.22f, 0.32f, 1.00f);
        colors[ImGuiCol_FrameBgActive]         = ImVec4(0.20f, 0.30f, 0.44f, 1.00f);

        colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.10f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.04f, 0.05f, 0.08f, 0.75f);

        colors[ImGuiCol_MenuBarBg]             = ImVec4(0.08f, 0.10f, 0.15f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.04f, 0.05f, 0.08f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.28f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.25f, 0.38f, 0.55f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.30f, 0.45f, 0.65f, 1.00f);

        colors[ImGuiCol_CheckMark]             = ImVec4(0.25f, 0.95f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]            = ImVec4(0.25f, 0.85f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.40f, 0.95f, 1.00f, 1.00f);

        colors[ImGuiCol_Button]                = ImVec4(0.14f, 0.22f, 0.35f, 0.85f);
        colors[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.32f, 0.50f, 1.00f);
        colors[ImGuiCol_ButtonActive]          = ImVec4(0.16f, 0.48f, 0.75f, 1.00f);

        colors[ImGuiCol_Header]                = ImVec4(0.15f, 0.24f, 0.36f, 0.80f);
        colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.35f, 0.52f, 1.00f);
        colors[ImGuiCol_HeaderActive]          = ImVec4(0.20f, 0.42f, 0.65f, 1.00f);

        colors[ImGuiCol_Separator]             = ImVec4(0.18f, 0.25f, 0.38f, 0.70f);
        colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.25f, 0.45f, 0.70f, 1.00f);
        colors[ImGuiCol_SeparatorActive]       = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);
    }

    void EditorLayer::RenderDiagnosticsOverlay(
        const Timer& timer,
        const DX12Device& device,
        const Scene& scene,
        u32 renderedEntities)
    {
        if (!m_showDiagnostics)
        {
            return;
        }

        const ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.78f);

        if (ImGui::Begin("##DiagnosticsOverlay", &m_showDiagnostics, windowFlags))
        {
            ImGui::TextColored(ImVec4(0.25f, 0.95f, 1.00f, 1.0f), "RLengine 7.1 [DX11/DX12 + Lua 5.4 | Blender Editor]");
            ImGui::Separator();

            ImGui::Text("GPU: %.*s", static_cast<int>(device.GetGpuDescription().size()), device.GetGpuDescription().data());
            ImGui::Text("FPS: %u (%.2f ms)", timer.GetFPS(), timer.GetFrameTimeMS());
            ImGui::Text("ECS Entities: %zu (Rendered: %u)", scene.GetAliveEntityCount(), renderedEntities);

            const auto& cam = scene.GetCamera();
            const auto& camPos = cam.GetPosition();
            ImGui::Text("Camera Pos: (%.2f, %.2f) | Zoom: %.2fx", camPos.x, camPos.y, cam.GetZoom());

            ImGui::Separator();
            ImGui::TextDisabled("Console: [~] or [F1] | Editor: [F2] | Extra Panels: [F3]");
            if (ImGui::SmallButton(m_luaConsole.IsOpen() ? "Hide Console" : "Console"))
            {
                m_luaConsole.Toggle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(m_sceneBuilder.IsOpen() ? "Hide Editor" : "Editor (F2)"))
            {
                m_sceneBuilder.ToggleOpen();
            }

            ImGui::End();
        }
    }
}
