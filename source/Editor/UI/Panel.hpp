#pragma once
#include <d3d12.h>

#include "Core/Types.hpp"
#include "Core/StringID.hpp"

#include <imgui.h>
#include <string>
#include <string_view>

namespace RL::Editor
{
    enum class PanelFlags : u32
    {
        None          = 0,
        NoClose       = 1 << 0,
        NoCollapse    = 1 << 1,
        NoResize      = 1 << 2,
        NoScrollbar   = 1 << 3,
        AlwaysVisible = 1 << 4,
        Overlay       = 1 << 5,
    };

    class Panel
    {
    public:
        explicit Panel(std::string_view title, PanelFlags flags = PanelFlags::None)
            : m_title(title)
            , m_id(title)
            , m_flags(flags)
            , m_visible(true)
        {}

        virtual ~Panel() = default;

        virtual void OnRender() = 0;

        virtual void OnAttach() {}

        virtual void OnDetach() {}

        [[nodiscard]] std::string_view GetTitle()   const noexcept { return m_title; }
        [[nodiscard]] const StringID&  GetID()      const noexcept { return m_id;    }
        [[nodiscard]] bool& IsVisible() noexcept { return m_visible; } bool IsVisible() const noexcept { return m_visible; }
        [[nodiscard]] PanelFlags       GetFlags()   const noexcept { return m_flags;   }

        void SetVisible(bool visible) noexcept { m_visible = visible; }
        void ToggleVisible()          noexcept { m_visible = !m_visible; }

        void Render()
        {
            if (!m_visible) return;

            ImGuiWindowFlags imguiFlags = ImGuiWindowFlags_None;
            if (static_cast<u32>(m_flags) & static_cast<u32>(PanelFlags::NoScrollbar))
                imguiFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            if (static_cast<u32>(m_flags) & static_cast<u32>(PanelFlags::NoCollapse))
                imguiFlags |= ImGuiWindowFlags_NoCollapse;
            if (static_cast<u32>(m_flags) & static_cast<u32>(PanelFlags::NoResize))
                imguiFlags |= ImGuiWindowFlags_NoResize;

            bool* pOpen = (static_cast<u32>(m_flags) & static_cast<u32>(PanelFlags::NoClose))
                          ? nullptr : &m_visible;

            if (m_title.find("Viewport") != std::string::npos) { ImGui::SetNextWindowPos(ImVec2(270, 22), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(1010, 638), ImGuiCond_FirstUseEver); ImGui::SetNextWindowBgAlpha(0.0f); }
            else if (m_title.find("Hierarchy") != std::string::npos) { ImGui::SetNextWindowPos(ImVec2(0, 22), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(270, 520), ImGuiCond_FirstUseEver); }
            else if (m_title.find("Stats") != std::string::npos) { ImGui::SetNextWindowPos(ImVec2(0, 542), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(270, 358), ImGuiCond_FirstUseEver); }
            else if (m_title.find("Content") != std::string::npos) { ImGui::SetNextWindowPos(ImVec2(270, 660), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(1010, 240), ImGuiCond_FirstUseEver); }
            else if (m_title.find("Inspector") != std::string::npos) { ImGui::SetNextWindowPos(ImVec2(1280, 22), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(320, 878), ImGuiCond_FirstUseEver); }
            if (ImGui::Begin(m_title.c_str(), pOpen, imguiFlags))
            {
                OnRender();
            }
            ImGui::End();
        }

    protected:
        std::string m_title;
        StringID    m_id;
        PanelFlags  m_flags;
        bool        m_visible;
    };

}
