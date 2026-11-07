#include "Platform/Input.hpp"
#include <algorithm>
#include <cctype>

namespace RL
{
    void Input::NewFrame() noexcept
    {
        s_previousKeys = s_currentKeys;
        s_previousMouseButtons = s_currentMouseButtons;
        s_mouseDelta = { 0.0f, 0.0f };
        s_mouseScroll = 0.0f;
    }

    bool Input::ProcessMessage([[maybe_unused]] HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
    {
        switch (msg)
        {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                if (wParam < 256)
                {
                    s_currentKeys.set(static_cast<usize>(wParam), true);
                }
                return false;
            }

            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                if (wParam < 256)
                {
                    s_currentKeys.set(static_cast<usize>(wParam), false);
                }
                return false;
            }

            case WM_MOUSEMOVE:
            {
                const f32 mouseX = static_cast<f32>(static_cast<i16>(LOWORD(lParam)));
                const f32 mouseY = static_cast<f32>(static_cast<i16>(HIWORD(lParam)));

                if (s_firstMouseMove)
                {
                    s_mousePosition = { mouseX, mouseY };
                    s_mouseDelta = { 0.0f, 0.0f };
                    s_firstMouseMove = false;
                }
                else
                {
                    s_mouseDelta.x += (mouseX - s_mousePosition.x);
                    s_mouseDelta.y += (mouseY - s_mousePosition.y);
                    s_mousePosition = { mouseX, mouseY };
                }
                return false;
            }

            case WM_LBUTTONDOWN:
            {
                s_currentMouseButtons[static_cast<usize>(MouseButton::Left)] = true;
                return false;
            }
            case WM_LBUTTONUP:
            {
                s_currentMouseButtons[static_cast<usize>(MouseButton::Left)] = false;
                return false;
            }

            case WM_RBUTTONDOWN:
            {
                s_currentMouseButtons[static_cast<usize>(MouseButton::Right)] = true;
                return false;
            }
            case WM_RBUTTONUP:
            {
                s_currentMouseButtons[static_cast<usize>(MouseButton::Right)] = false;
                return false;
            }

            case WM_MBUTTONDOWN:
            {
                s_currentMouseButtons[static_cast<usize>(MouseButton::Middle)] = true;
                return false;
            }
            case WM_MBUTTONUP:
            {
                s_currentMouseButtons[static_cast<usize>(MouseButton::Middle)] = false;
                return false;
            }

            case WM_MOUSEWHEEL:
            {
                const i16 wheelDelta = static_cast<i16>(HIWORD(wParam));
                s_mouseScroll += static_cast<f32>(wheelDelta) / static_cast<f32>(WHEEL_DELTA);
                return false;
            }

            default:
                break;
        }

        return false;
    }

    bool Input::IsKeyDown(KeyCode key) noexcept
    {
        const usize idx = static_cast<usize>(key);
        return (idx < 256) ? s_currentKeys.test(idx) : false;
    }

    bool Input::IsKeyPressed(KeyCode key) noexcept
    {
        const usize idx = static_cast<usize>(key);
        return (idx < 256) ? (s_currentKeys.test(idx) && !s_previousKeys.test(idx)) : false;
    }

    bool Input::IsKeyReleased(KeyCode key) noexcept
    {
        const usize idx = static_cast<usize>(key);
        return (idx < 256) ? (!s_currentKeys.test(idx) && s_previousKeys.test(idx)) : false;
    }

    DirectX::XMFLOAT2 Input::GetMousePosition() noexcept
    {
        return s_mousePosition;
    }

    DirectX::XMFLOAT2 Input::GetMouseDelta() noexcept
    {
        return s_mouseDelta;
    }

    f32 Input::GetMouseScroll() noexcept
    {
        return s_mouseScroll;
    }

    bool Input::IsMouseButtonDown(MouseButton button) noexcept
    {
        const usize idx = static_cast<usize>(button);
        return (idx < s_currentMouseButtons.size()) ? s_currentMouseButtons[idx] : false;
    }

    bool Input::IsMouseButtonPressed(MouseButton button) noexcept
    {
        const usize idx = static_cast<usize>(button);
        return (idx < s_currentMouseButtons.size()) ? (s_currentMouseButtons[idx] && !s_previousMouseButtons[idx]) : false;
    }

    bool Input::IsMouseButtonReleased(MouseButton button) noexcept
    {
        const usize idx = static_cast<usize>(button);
        return (idx < s_currentMouseButtons.size()) ? (!s_currentMouseButtons[idx] && s_previousMouseButtons[idx]) : false;
    }

    KeyCode Input::StringToKeyCode(std::string_view name) noexcept
    {
        if (name.empty())
        {
            return KeyCode::None;
        }

        if (name.size() == 1)
        {
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            if (c >= 'A' && c <= 'Z')
            {
                return static_cast<KeyCode>(c);
            }
            if (c >= '0' && c <= '9')
            {
                return static_cast<KeyCode>(c);
            }
        }

        char upperBuffer[32]{};
        const usize copyLen = (std::min)(name.size(), sizeof(upperBuffer) - 1);
        for (usize i = 0; i < copyLen; ++i)
        {
            upperBuffer[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
        }
        const std::string_view upper(upperBuffer, copyLen);

        if (upper == "SPACE") return KeyCode::Space;
        if (upper == "SHIFT") return KeyCode::Shift;
        if (upper == "ESCAPE" || upper == "ESC") return KeyCode::Escape;
        if (upper == "ENTER" || upper == "RETURN") return KeyCode::Enter;
        if (upper == "CTRL" || upper == "CONTROL") return KeyCode::Control;
        if (upper == "ALT") return KeyCode::Alt;
        if (upper == "TAB") return KeyCode::Tab;
        if (upper == "BACKSPACE") return KeyCode::Backspace;
        if (upper == "UP") return KeyCode::Up;
        if (upper == "DOWN") return KeyCode::Down;
        if (upper == "LEFT") return KeyCode::Left;
        if (upper == "RIGHT") return KeyCode::Right;

        if (upper == "F1") return KeyCode::F1;
        if (upper == "F2") return KeyCode::F2;
        if (upper == "F3") return KeyCode::F3;
        if (upper == "F4") return KeyCode::F4;
        if (upper == "F5") return KeyCode::F5;
        if (upper == "F6") return KeyCode::F6;
        if (upper == "F7") return KeyCode::F7;
        if (upper == "F8") return KeyCode::F8;
        if (upper == "F9") return KeyCode::F9;
        if (upper == "F10") return KeyCode::F10;
        if (upper == "F11") return KeyCode::F11;
        if (upper == "F12") return KeyCode::F12;

        return KeyCode::None;
    }
}
