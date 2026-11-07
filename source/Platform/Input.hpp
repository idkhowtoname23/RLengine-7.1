#pragma once

#include "Core/Types.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <bitset>
#include <string_view>
#include <DirectXMath.h>

namespace RL
{
    enum class KeyCode : u32
    {
        None = 0,

        A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G', H = 'H',
        I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N', O = 'O', P = 'P',
        Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X',
        Y = 'Y', Z = 'Z',

        D0 = '0', D1 = '1', D2 = '2', D3 = '3', D4 = '4',
        D5 = '5', D6 = '6', D7 = '7', D8 = '8', D9 = '9',

        Space     = VK_SPACE,
        Enter     = VK_RETURN,
        Escape    = VK_ESCAPE,
        Backspace = VK_BACK,
        Tab       = VK_TAB,
        Shift     = VK_SHIFT,
        Control   = VK_CONTROL,
        Alt       = VK_MENU,

        Left  = VK_LEFT,
        Up    = VK_UP,
        Right = VK_RIGHT,
        Down  = VK_DOWN,

        F1  = VK_F1,  F2  = VK_F2,  F3  = VK_F3,  F4  = VK_F4,
        F5  = VK_F5,  F6  = VK_F6,  F7  = VK_F7,  F8  = VK_F8,
        F9  = VK_F9,  F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,

        Count = 256
    };

    enum class MouseButton : u8
    {
        Left   = 0,
        Right  = 1,
        Middle = 2,
        X1     = 3,
        X2     = 4,
        Count  = 5
    };

    class Input
    {
    public:
        Input() = delete;

        static void NewFrame() noexcept;

        static bool ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

        [[nodiscard]] static bool IsKeyDown(KeyCode key) noexcept;
        [[nodiscard]] static bool IsKeyPressed(KeyCode key) noexcept;
        [[nodiscard]] static bool IsKeyReleased(KeyCode key) noexcept;

        [[nodiscard]] static DirectX::XMFLOAT2 GetMousePosition() noexcept;
        [[nodiscard]] static DirectX::XMFLOAT2 GetMouseDelta() noexcept;
        [[nodiscard]] static f32 GetMouseScroll() noexcept;

        [[nodiscard]] static bool IsMouseButtonDown(MouseButton button) noexcept;
        [[nodiscard]] static bool IsMouseButtonPressed(MouseButton button) noexcept;
        [[nodiscard]] static bool IsMouseButtonReleased(MouseButton button) noexcept;

        [[nodiscard]] static KeyCode StringToKeyCode(std::string_view name) noexcept;

    private:
        static inline std::bitset<256> s_currentKeys{};
        static inline std::bitset<256> s_previousKeys{};

        static inline std::array<bool, static_cast<usize>(MouseButton::Count)> s_currentMouseButtons{};
        static inline std::array<bool, static_cast<usize>(MouseButton::Count)> s_previousMouseButtons{};

        static inline DirectX::XMFLOAT2 s_mousePosition{ 0.0f, 0.0f };
        static inline DirectX::XMFLOAT2 s_mouseDelta{ 0.0f, 0.0f };
        static inline f32 s_mouseScroll = 0.0f;
        static inline bool s_firstMouseMove = true;
    };
}
