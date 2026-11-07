#pragma once

#include "Core/Types.hpp"
#include <imgui.h>

namespace RL::Editor
{
    enum class EditorTheme : u8
    {
        BlenderDark = 0,
        DarkModern,
        Light,
    };

    class Theme
    {
    public:
        static void Apply(EditorTheme theme = EditorTheme::BlenderDark)
        {
            switch (theme)
            {
                case EditorTheme::BlenderDark:  ApplyBlenderDark();  break;
                case EditorTheme::DarkModern:   ApplyDarkModern();   break;
                case EditorTheme::Light:        ApplyLight();        break;
            }
        }

    private:

        static void ApplyBlenderDark()
        {
            ImGuiStyle& style = ImGui::GetStyle();

            style.WindowRounding    = 4.0f;
            style.ChildRounding     = 3.0f;
            style.FrameRounding     = 3.0f;
            style.PopupRounding     = 3.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding      = 3.0f;
            style.TabRounding       = 4.0f;

            style.WindowPadding     = ImVec2(8.0f, 8.0f);
            style.FramePadding      = ImVec2(5.0f, 3.0f);
            style.ItemSpacing       = ImVec2(6.0f, 4.0f);
            style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
            style.IndentSpacing     = 18.0f;
            style.ScrollbarSize     = 12.0f;
            style.GrabMinSize       = 10.0f;
            style.WindowBorderSize  = 1.0f;
            style.FrameBorderSize   = 0.0f;
            style.TabBorderSize     = 0.0f;

            ImVec4* c = style.Colors;

            c[ImGuiCol_WindowBg]            = ImVec4(0.114f, 0.114f, 0.114f, 1.00f);
            c[ImGuiCol_ChildBg]             = ImVec4(0.098f, 0.098f, 0.098f, 1.00f);
            c[ImGuiCol_PopupBg]             = ImVec4(0.114f, 0.114f, 0.114f, 0.98f);
            c[ImGuiCol_MenuBarBg]           = ImVec4(0.133f, 0.133f, 0.133f, 1.00f);

            c[ImGuiCol_Border]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            c[ImGuiCol_BorderShadow]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

            c[ImGuiCol_FrameBg]             = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            c[ImGuiCol_FrameBgHovered]      = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
            c[ImGuiCol_FrameBgActive]       = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

            c[ImGuiCol_TitleBg]             = ImVec4(0.133f, 0.133f, 0.133f, 1.00f);
            c[ImGuiCol_TitleBgActive]       = ImVec4(0.150f, 0.150f, 0.150f, 1.00f);
            c[ImGuiCol_TitleBgCollapsed]    = ImVec4(0.100f, 0.100f, 0.100f, 1.00f);

            c[ImGuiCol_ScrollbarBg]         = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            c[ImGuiCol_ScrollbarGrab]       = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
            c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
            c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

            c[ImGuiCol_CheckMark]           = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);
            c[ImGuiCol_SliderGrab]          = ImVec4(0.910f, 0.490f, 0.051f, 0.90f);
            c[ImGuiCol_SliderGrabActive]    = ImVec4(1.000f, 0.600f, 0.100f, 1.00f);

            c[ImGuiCol_Button]              = ImVec4(0.220f, 0.220f, 0.220f, 1.00f);
            c[ImGuiCol_ButtonHovered]       = ImVec4(0.910f, 0.490f, 0.051f, 0.85f);
            c[ImGuiCol_ButtonActive]        = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);

            c[ImGuiCol_Header]              = ImVec4(0.910f, 0.490f, 0.051f, 0.35f);
            c[ImGuiCol_HeaderHovered]       = ImVec4(0.910f, 0.490f, 0.051f, 0.50f);
            c[ImGuiCol_HeaderActive]        = ImVec4(0.910f, 0.490f, 0.051f, 0.75f);

            c[ImGuiCol_Separator]           = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            c[ImGuiCol_SeparatorHovered]    = ImVec4(0.910f, 0.490f, 0.051f, 0.80f);
            c[ImGuiCol_SeparatorActive]     = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);

            c[ImGuiCol_ResizeGrip]          = ImVec4(0.910f, 0.490f, 0.051f, 0.20f);
            c[ImGuiCol_ResizeGripHovered]   = ImVec4(0.910f, 0.490f, 0.051f, 0.70f);
            c[ImGuiCol_ResizeGripActive]    = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);

            c[ImGuiCol_Tab]                 = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
            c[ImGuiCol_TabHovered]          = ImVec4(0.910f, 0.490f, 0.051f, 0.80f);
            c[ImGuiCol_TabActive]         = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);

            c[ImGuiCol_TabUnfocused]           = ImVec4(0.133f, 0.133f, 0.133f, 1.00f);
            c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);

            c[ImGuiCol_Text]                = ImVec4(0.860f, 0.860f, 0.860f, 1.00f);
            c[ImGuiCol_TextDisabled]        = ImVec4(0.450f, 0.450f, 0.450f, 1.00f);

            c[ImGuiCol_DragDropTarget]      = ImVec4(0.910f, 0.490f, 0.051f, 0.90f);

            c[ImGuiCol_NavHighlight]        = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);
            c[ImGuiCol_NavWindowingHighlight]= ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            c[ImGuiCol_NavWindowingDimBg]   = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            c[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);

            c[ImGuiCol_PlotLines]           = ImVec4(0.560f, 0.760f, 0.280f, 1.00f);
            c[ImGuiCol_PlotLinesHovered]    = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);
            c[ImGuiCol_PlotHistogram]       = ImVec4(0.910f, 0.490f, 0.051f, 1.00f);
            c[ImGuiCol_PlotHistogramHovered]= ImVec4(1.000f, 0.600f, 0.100f, 1.00f);
        }

        static void ApplyDarkModern()
        {
            ImGui::StyleColorsDark();
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 6.0f;
            style.FrameRounding  = 4.0f;
            style.GrabRounding   = 4.0f;
        }

        static void ApplyLight()
        {
            ImGui::StyleColorsLight();
        }
    };

}
