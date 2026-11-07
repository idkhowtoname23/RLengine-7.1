#pragma warning(disable: 4100 4189 4244 4267 4305 4456 4458 4701)
#pragma once

#include "Core/Types.hpp"
#include <DirectXMath.h>
#include <imgui.h>
#include <string_view>
#include <algorithm>
#include <cmath>

namespace RL::Editor
{
    using namespace DirectX;

    enum class GizmoMode : u8
    {
        Translate = 0,
        Rotate,
        Scale,
        Universal,
    };

    enum class GizmoSpace : u8
    {
        World = 0,
        Local,
    };

    enum class GizmoAxis : u8
    {
        None = 0,
        X, Y, Z,
        XY, XZ, YZ,
        View,
        Center,
    };

    struct GizmoResult
    {
        bool       used       = false;
        bool       changed    = false;
        XMFLOAT3   deltaPos   = {};
        XMFLOAT3   deltaRot   = {};
        XMFLOAT3   deltaScale = {};
        GizmoAxis  activeAxis = GizmoAxis::None;
    };

    class GizmoSystem
    {
    public:

        void SetMode(GizmoMode mode)   noexcept { m_mode  = mode;  }
        void SetSpace(GizmoSpace space) noexcept { m_space = space; }

        [[nodiscard]] GizmoMode  GetMode()  const noexcept { return m_mode;  }
        [[nodiscard]] GizmoSpace GetSpace() const noexcept { return m_space; }

        GizmoResult Manipulate(
            XMFLOAT4X4&    worldMatrix,
            const XMMATRIX& viewMatrix,
            const XMMATRIX& projMatrix,
            ImVec2          viewportPos,
            ImVec2          viewportSize)
        {
            GizmoResult result;

            if (!m_enabled) return result;

            XMVECTOR origin = XMVectorSet(worldMatrix._41, worldMatrix._42, worldMatrix._43, 1.0f);
            XMFLOAT2 screenOrigin;
            if (!WorldToScreen(origin, viewMatrix, projMatrix, viewportPos, viewportSize, screenOrigin))
                return result;

            ImDrawList* dl = ImGui::GetWindowDrawList();

            switch (m_mode)
            {
                case GizmoMode::Translate:
                    result = RenderTranslateGizmo(dl, worldMatrix, viewMatrix, projMatrix,
                                                   viewportPos, viewportSize, screenOrigin);
                    break;
                case GizmoMode::Rotate:
                    result = RenderRotateGizmo(dl, worldMatrix, viewMatrix, projMatrix,
                                               viewportPos, viewportSize, screenOrigin);
                    break;
                case GizmoMode::Scale:
                    result = RenderScaleGizmo(dl, worldMatrix, viewMatrix, projMatrix,
                                              viewportPos, viewportSize, screenOrigin);
                    break;
                default:
                    break;
            }

            HandleKeyboardShortcuts();

            return result;
        }

        void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
        [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    private:

        static constexpr ImU32 kColorX       = IM_COL32(230,  50,  50, 255);
        static constexpr ImU32 kColorY       = IM_COL32( 50, 200,  50, 255);
        static constexpr ImU32 kColorZ       = IM_COL32( 50, 100, 230, 255);
        static constexpr ImU32 kColorXY      = IM_COL32(230, 200,  50, 120);
        static constexpr ImU32 kColorXZ      = IM_COL32( 50, 200,  50, 120);
        static constexpr ImU32 kColorYZ      = IM_COL32( 50, 100, 230, 120);
        static constexpr ImU32 kColorHover   = IM_COL32(255, 230,  50, 255);
        static constexpr ImU32 kColorCenter  = IM_COL32(220, 220, 220, 255);
        static constexpr f32   kAxisLen      = 80.0f;
        static constexpr f32   kArrowHead    = 8.0f;
        static constexpr f32   kLineThick    = 2.5f;
        static constexpr f32   kHitRadius    = 10.0f;

        [[nodiscard]] static bool WorldToScreen(
            XMVECTOR         worldPos,
            const XMMATRIX&  view,
            const XMMATRIX&  proj,
            ImVec2           vpPos,
            ImVec2           vpSize,
            XMFLOAT2&        outScreen)
        {
            XMVECTOR clip = XMVector4Transform(worldPos, XMMatrixMultiply(view, proj));
            XMFLOAT4 c;
            XMStoreFloat4(&c, clip);
            if (c.w <= 0.001f) return false;

            const f32 ndcX =  c.x / c.w;
            const f32 ndcY = -c.y / c.w;

            outScreen.x = vpPos.x + (ndcX * 0.5f + 0.5f) * vpSize.x;
            outScreen.y = vpPos.y + (ndcY * 0.5f + 0.5f) * vpSize.y;
            return true;
        }

        [[nodiscard]] XMVECTOR GetAxis(const XMFLOAT4X4& mat, GizmoAxis axis) const
        {
            if (m_space == GizmoSpace::World)
            {
                switch (axis)
                {
                    case GizmoAxis::X: return XMVectorSet(1, 0, 0, 0);
                    case GizmoAxis::Y: return XMVectorSet(0, 1, 0, 0);
                    case GizmoAxis::Z: return XMVectorSet(0, 0, 1, 0);
                    default:           return XMVectorZero();
                }
            }
            else
            {
                const XMMATRIX m = XMLoadFloat4x4(&mat);
                switch (axis)
                {
                    case GizmoAxis::X: return XMVector3Normalize(m.r[0]);
                    case GizmoAxis::Y: return XMVector3Normalize(m.r[1]);
                    case GizmoAxis::Z: return XMVector3Normalize(m.r[2]);
                    default:           return XMVectorZero();
                }
            }
        }

        GizmoResult RenderTranslateGizmo(
            ImDrawList*    dl,
            XMFLOAT4X4&    world,
            const XMMATRIX& view,
            const XMMATRIX& proj,
            ImVec2         vpPos,
            ImVec2         vpSize,
            XMFLOAT2       screenOrg)
        {
            GizmoResult result;
            const ImVec2 org{ screenOrg.x, screenOrg.y };

            auto ProjectAxisTip = [&](GizmoAxis ax) -> ImVec2 {
                XMVECTOR worldPos = XMVectorSet(world._41, world._42, world._43, 1.0f);
                XMVECTOR dir = XMVectorScale(GetAxis(world, ax), 0.8f);
                XMFLOAT2 screen;
                if (WorldToScreen(worldPos + dir, view, proj, vpPos, vpSize, screen))
                    return { screen.x, screen.y };
                return org;
            };

            const ImVec2 tipX = ProjectAxisTip(GizmoAxis::X);
            const ImVec2 tipY = ProjectAxisTip(GizmoAxis::Y);
            const ImVec2 tipZ = ProjectAxisTip(GizmoAxis::Z);

            const ImVec2 mouse = ImGui::GetMousePos();
            auto DistToSegment = [](ImVec2 p, ImVec2 a, ImVec2 b) -> f32 {
                ImVec2 ab{ b.x - a.x, b.y - a.y };
                ImVec2 ap{ p.x - a.x, p.y - a.y };
                const f32 t = std::clamp((ap.x*ab.x + ap.y*ab.y) / (ab.x*ab.x + ab.y*ab.y + 1e-6f), 0.f, 1.f);
                const f32 dx = ap.x - t*ab.x, dy = ap.y - t*ab.y;
                return std::sqrt(dx*dx + dy*dy);
            };

            GizmoAxis hovered = GizmoAxis::None;
            if (DistToSegment(mouse, org, tipX) < kHitRadius) hovered = GizmoAxis::X;
            else if (DistToSegment(mouse, org, tipY) < kHitRadius) hovered = GizmoAxis::Y;
            else if (DistToSegment(mouse, org, tipZ) < kHitRadius) hovered = GizmoAxis::Z;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered != GizmoAxis::None)
            {
                m_dragAxis   = hovered;
                m_dragActive = true;
                m_dragStart  = mouse;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_dragActive = false;

            if (m_dragActive)
            {
                const ImVec2 delta{ mouse.x - m_dragStart.x, mouse.y - m_dragStart.y };
                m_dragStart = mouse;

                constexpr f32 kScale = 0.01f;
                result.used    = true;
                result.changed = (delta.x != 0 || delta.y != 0);

                switch (m_dragAxis)
                {
                    case GizmoAxis::X: result.deltaPos.x =  delta.x * kScale; break;
                    case GizmoAxis::Y: result.deltaPos.y = -delta.y * kScale; break;
                    case GizmoAxis::Z: result.deltaPos.z = -delta.y * kScale; break;
                    default: break;
                }
                result.activeAxis = m_dragAxis;
            }

            auto DrawArrow = [&](ImVec2 from, ImVec2 to, ImU32 col, bool hov) {
                const ImU32 c = hov ? kColorHover : col;
                dl->AddLine(from, to, c, kLineThick);

                const float dx = to.x - from.x, dy = to.y - from.y;
                const float len = std::sqrt(dx*dx + dy*dy) + 1e-6f;
                const float nx = dx/len, ny = dy/len;
                const float px = -ny * kArrowHead * 0.4f, py = nx * kArrowHead * 0.4f;
                dl->AddTriangleFilled(
                    to,
                    ImVec2(to.x - nx*kArrowHead + px, to.y - ny*kArrowHead + py),
                    ImVec2(to.x - nx*kArrowHead - px, to.y - ny*kArrowHead - py),
                    c);
            };

            DrawArrow(org, tipX, kColorX, hovered == GizmoAxis::X || m_dragAxis == GizmoAxis::X);
            DrawArrow(org, tipY, kColorY, hovered == GizmoAxis::Y || m_dragAxis == GizmoAxis::Y);
            DrawArrow(org, tipZ, kColorZ, hovered == GizmoAxis::Z || m_dragAxis == GizmoAxis::Z);

            dl->AddCircleFilled(org, 4.5f, kColorCenter);

            return result;
        }

        GizmoResult RenderRotateGizmo(
            ImDrawList*    dl,
            XMFLOAT4X4&    world,
            const XMMATRIX& view,
            const XMMATRIX& proj,
            ImVec2         vpPos,
            ImVec2         vpSize,
            XMFLOAT2       screenOrg)
        {
            GizmoResult result;
            const ImVec2 org{ screenOrg.x, screenOrg.y };

            constexpr f32 kRadius = 55.0f;
            const ImVec2 mouse = ImGui::GetMousePos();
            const f32 distToCenter = std::sqrt(
                (mouse.x - org.x)*(mouse.x - org.x) +
                (mouse.y - org.y)*(mouse.y - org.y));

            auto RingHit = [&](f32 r) {
                return std::abs(distToCenter - r) < kHitRadius;
            };

            GizmoAxis hovered = GizmoAxis::None;

            if (RingHit(kRadius * 1.10f)) hovered = GizmoAxis::View;
            else if (RingHit(kRadius))    hovered = GizmoAxis::Y;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered != GizmoAxis::None)
            {
                m_dragAxis   = hovered;
                m_dragActive = true;
                m_dragStart  = mouse;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_dragActive = false;

            if (m_dragActive)
            {
                const ImVec2 prevDir{ m_dragStart.x - org.x, m_dragStart.y - org.y };
                const ImVec2 currDir{ mouse.x        - org.x, mouse.y        - org.y };
                const f32 angle = std::atan2(currDir.y, currDir.x) - std::atan2(prevDir.y, prevDir.x);
                m_dragStart = mouse;
                result.used = result.changed = true;

                switch (m_dragAxis)
                {
                    case GizmoAxis::X:    result.deltaRot.x = angle; break;
                    case GizmoAxis::Y:    result.deltaRot.y = angle; break;
                    case GizmoAxis::Z:    result.deltaRot.z = angle; break;
                    case GizmoAxis::View: result.deltaRot.z = angle; break;
                    default: break;
                }
                result.activeAxis = m_dragAxis;
            }

            dl->AddCircle(org, kRadius * 1.10f,
                hovered == GizmoAxis::View ? kColorHover : IM_COL32(180,180,180,200), 64, kLineThick);
            dl->AddCircle(org, kRadius,
                hovered == GizmoAxis::Y ? kColorHover : kColorY, 64, kLineThick);
            dl->AddCircle(org, kRadius * 0.90f,
                hovered == GizmoAxis::X ? kColorHover : kColorX, 64, kLineThick);
            dl->AddCircle(org, kRadius * 0.80f,
                hovered == GizmoAxis::Z ? kColorHover : kColorZ, 64, kLineThick);

            return result;
        }

        GizmoResult RenderScaleGizmo(
            ImDrawList*    dl,
            XMFLOAT4X4&    world,
            const XMMATRIX& view,
            const XMMATRIX& proj,
            ImVec2         vpPos,
            ImVec2         vpSize,
            XMFLOAT2       screenOrg)
        {
            GizmoResult result;
            const ImVec2 org{ screenOrg.x, screenOrg.y };

            const ImVec2 tips[3] = {
                { org.x + kAxisLen, org.y },
                { org.x,            org.y - kAxisLen },
                { org.x + kAxisLen * 0.707f, org.y + kAxisLen * 0.707f },
            };
            const ImU32 colors[3] = { kColorX, kColorY, kColorZ };

            const ImVec2 mouse = ImGui::GetMousePos();
            GizmoAxis hovered = GizmoAxis::None;
            for (int i = 0; i < 3; ++i)
            {
                const f32 dx = mouse.x - tips[i].x, dy = mouse.y - tips[i].y;
                if (dx*dx + dy*dy < kHitRadius*kHitRadius)
                    hovered = static_cast<GizmoAxis>(static_cast<u8>(GizmoAxis::X) + i);
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered != GizmoAxis::None)
            {
                m_dragAxis   = hovered;
                m_dragActive = true;
                m_dragStart  = mouse;
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_dragActive = false;

            if (m_dragActive)
            {
                const ImVec2 delta{ mouse.x - m_dragStart.x, mouse.y - m_dragStart.y };
                m_dragStart = mouse;
                constexpr f32 kScale = 0.01f;
                result.used    = true;
                result.changed = (delta.x != 0 || delta.y != 0);
                switch (m_dragAxis)
                {
                    case GizmoAxis::X: result.deltaScale.x =  delta.x * kScale; break;
                    case GizmoAxis::Y: result.deltaScale.y = -delta.y * kScale; break;
                    case GizmoAxis::Z: result.deltaScale.z = -delta.y * kScale; break;
                    default: break;
                }
                result.activeAxis = m_dragAxis;
            }

            for (int i = 0; i < 3; ++i)
            {
                const ImU32 c = (hovered == static_cast<GizmoAxis>(static_cast<u8>(GizmoAxis::X)+i) ||
                                 m_dragAxis == static_cast<GizmoAxis>(static_cast<u8>(GizmoAxis::X)+i))
                                 ? kColorHover : colors[i];
                dl->AddLine(org, tips[i], c, kLineThick);

                dl->AddRectFilled(
                    ImVec2(tips[i].x - 5, tips[i].y - 5),
                    ImVec2(tips[i].x + 5, tips[i].y + 5), c);
            }
            dl->AddCircleFilled(org, 5.0f, kColorCenter);

            return result;
        }

        void HandleKeyboardShortcuts()
        {
            if (!ImGui::IsAnyItemActive())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_G)) m_mode = GizmoMode::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) m_mode = GizmoMode::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_S)) m_mode = GizmoMode::Scale;
            }
        }

        GizmoMode  m_mode      = GizmoMode::Translate;
        GizmoSpace m_space     = GizmoSpace::World;
        bool       m_enabled   = true;
        bool       m_dragActive= false;
        GizmoAxis  m_dragAxis  = GizmoAxis::None;
        ImVec2     m_dragStart = {};
    };

}
