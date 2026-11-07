#pragma once

#include "Editor/UI/Panel.hpp"
#include "Core/Logger.hpp"
#include "Core/StringID.hpp"

#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RL::Editor
{
    class PanelManager
    {
    public:

        template <typename PanelT, typename... Args>
        PanelT* AddPanel(Args&&... args)
        {
            auto panel = std::make_unique<PanelT>(std::forward<Args>(args)...);
            PanelT* raw = panel.get();
            raw->OnAttach();
            m_panels.push_back(std::move(panel));
            return raw;
        }

        void AddPanel(std::unique_ptr<Panel> panel)
        {
            panel->OnAttach();
            m_panels.push_back(std::move(panel));
        }

        void RemovePanel(std::string_view title)
        {
            m_panels.erase(
                std::remove_if(m_panels.begin(), m_panels.end(),
                    [title](const std::unique_ptr<Panel>& p) {
                        if (p->GetTitle() == title) {
                            p->OnDetach();
                            return true;
                        }
                        return false;
                    }),
                m_panels.end()
            );
        }

        [[nodiscard]] Panel* FindPanel(std::string_view title)
        {
            for (auto& p : m_panels)
                if (p->GetTitle() == title) return p.get();
            return nullptr;
        }

        template <typename PanelT>
        [[nodiscard]] PanelT* FindPanelAs(std::string_view title)
        {
            return dynamic_cast<PanelT*>(FindPanel(title));
        }

        void RenderAll()
        {
            for (auto& panel : m_panels)
            {
                panel->Render();
            }
        }

        void RenderMenuBar()
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("Windows"))
                {
                    for (auto& panel : m_panels)
                    {
                        bool visible = panel->IsVisible();
                        if (ImGui::MenuItem(panel->GetTitle().data(), nullptr, visible))
                        {
                            panel->SetVisible(!visible);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
        }

        void Shutdown()
        {
            for (auto& p : m_panels) p->OnDetach();
            m_panels.clear();
        }

        [[nodiscard]] usize GetPanelCount() const noexcept { return m_panels.size(); }

    private:
        std::vector<std::unique_ptr<Panel>> m_panels;
    };

}
