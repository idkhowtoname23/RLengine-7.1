#pragma once

#include "Core/Types.hpp"
#include "Core/Timer.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <string_view>
#include <functional>
#include <expected>

namespace RL
{
    struct WindowDesc
    {
        std::wstring title = L"RLengine 7.1";
        u32 width = 1600;
        u32 height = 900;
        bool resizable = true;
    };

    class Window
    {
    public:
        using ResizeCallback = std::function<void(u32 width, u32 height)>;
        using CloseCallback  = std::function<void()>;

        Window() noexcept = default;
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        Window(Window&& other) noexcept;
        Window& operator=(Window&& other) noexcept;

        [[nodiscard]] std::expected<void, std::string> Initialize(const WindowDesc& desc);
        void Shutdown() noexcept;

        [[nodiscard]] bool ProcessMessages();

        [[nodiscard]] HWND GetHWND() const noexcept { return m_hwnd; }
        [[nodiscard]] u32 GetWidth() const noexcept { return m_width; }
        [[nodiscard]] u32 GetHeight() const noexcept { return m_height; }
        [[nodiscard]] f32 GetAspectRatio() const noexcept;
        [[nodiscard]] bool IsMinimized() const noexcept { return m_isMinimized; }
        [[nodiscard]] bool ShouldClose() const noexcept { return m_shouldClose; }

        void SetTitle(std::string_view title);
        void SetResizeCallback(ResizeCallback callback) { m_onResize = std::move(callback); }
        void SetCloseCallback(CloseCallback callback) { m_onClose = std::move(callback); }

        [[nodiscard]] Timer& GetTimer() noexcept { return m_timer; }
        [[nodiscard]] const Timer& GetTimer() const noexcept { return m_timer; }

    private:
        static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        HWND m_hwnd = nullptr;
        HINSTANCE m_hinstance = nullptr;
        std::wstring m_className = L"RLengine7_1_WindowClass";

        u32 m_width = 0;
        u32 m_height = 0;

        bool m_isMinimized = false;
        bool m_isResizing = false;
        bool m_shouldClose = false;

        ResizeCallback m_onResize;
        CloseCallback m_onClose;

        Timer m_timer;
    };
}
