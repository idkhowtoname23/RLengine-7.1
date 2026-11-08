#include "Platform/Window.hpp"
#include "Platform/Input.hpp"
#include "Core/Logger.hpp"
#include <imgui.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace RL
{
    Window::Window(Window&& other) noexcept
        : m_hwnd(other.m_hwnd)
        , m_hinstance(other.m_hinstance)
        , m_className(std::move(other.m_className))
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_isMinimized(other.m_isMinimized)
        , m_isResizing(other.m_isResizing)
        , m_shouldClose(other.m_shouldClose)
        , m_onResize(std::move(other.m_onResize))
        , m_onClose(std::move(other.m_onClose))
        , m_timer(other.m_timer)
    {
        other.m_hwnd = nullptr;
        other.m_hinstance = nullptr;

        if (m_hwnd)
        {
            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }

    Window& Window::operator=(Window&& other) noexcept
    {
        if (this != &other)
        {
            Shutdown();

            m_hwnd = other.m_hwnd;
            m_hinstance = other.m_hinstance;
            m_className = std::move(other.m_className);
            m_width = other.m_width;
            m_height = other.m_height;
            m_isMinimized = other.m_isMinimized;
            m_isResizing = other.m_isResizing;
            m_shouldClose = other.m_shouldClose;
            m_onResize = std::move(other.m_onResize);
            m_onClose = std::move(other.m_onClose);
            m_timer = other.m_timer;

            other.m_hwnd = nullptr;
            other.m_hinstance = nullptr;

            if (m_hwnd)
            {
                SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            }
        }
        return *this;
    }

    Window::~Window()
    {
        Shutdown();
    }

    std::expected<void, std::string> Window::Initialize(const WindowDesc& desc)
    {

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        m_hinstance = GetModuleHandleW(nullptr);
        m_width = desc.width;
        m_height = desc.height;

        HICON hIcon = LoadIconW(m_hinstance, MAKEINTRESOURCEW(101));
        if (!hIcon)
        {
            hIcon = static_cast<HICON>(LoadImageW(nullptr, L"assets/icons/rlengine.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
        }
        if (!hIcon)
        {
            hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        }

        HICON hIconSm = static_cast<HICON>(LoadImageW(m_hinstance, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
        if (!hIconSm)
        {
            hIconSm = static_cast<HICON>(LoadImageW(nullptr, L"assets/icons/rlengine.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE));
        }

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = Window::StaticWndProc;
        wc.hInstance     = m_hinstance;
        wc.hIcon         = hIcon;
        wc.hIconSm       = hIconSm ? hIconSm : hIcon;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = m_className.c_str();

        if (!RegisterClassExW(&wc))
        {

            const DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS)
            {
                return std::unexpected(std::format("Failed to register Win32 window class. Error: {}", err));
            }
        }

        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (desc.resizable)
        {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }

        const DWORD exStyle = WS_EX_APPWINDOW;

        RECT rc = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        AdjustWindowRectEx(&rc, style, FALSE, exStyle);

        const int windowWidth = rc.right - rc.left;
        const int windowHeight = rc.bottom - rc.top;

        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        const int posX = (screenWidth - windowWidth) / 2;
        const int posY = (screenHeight - windowHeight) / 2;

        m_hwnd = CreateWindowExW(
            exStyle,
            m_className.c_str(),
            desc.title.c_str(),
            style,
            posX,
            posY,
            windowWidth,
            windowHeight,
            nullptr,
            nullptr,
            m_hinstance,
            this
        );

        if (!m_hwnd)
        {
            return std::unexpected(std::format("Failed to create Win32 window. Error: {}", GetLastError()));
        }

        if (hIcon)
        {
            SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        }
        if (hIconSm)
        {
            SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSm));
        }

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        m_timer.Reset();

        RL_LOG_INFO("Platform window initialized: {}x{} [HWND: 0x{:016X}]", m_width, m_height, reinterpret_cast<u64>(m_hwnd));
        return {};
    }

    void Window::Shutdown() noexcept
    {
        if (m_hwnd)
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }

        if (m_hinstance)
        {
            UnregisterClassW(m_className.c_str(), m_hinstance);
            m_hinstance = nullptr;
        }
    }

    bool Window::ProcessMessages()
    {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_shouldClose = true;
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return !m_shouldClose;
    }

    f32 Window::GetAspectRatio() const noexcept
    {
        if (m_height == 0) return 1.0f;
        return static_cast<f32>(m_width) / static_cast<f32>(m_height);
    }

    void Window::SetTitle(std::string_view title)
    {
        if (!m_hwnd) return;

        const int len = MultiByteToWideChar(CP_UTF8, 0, title.data(), static_cast<int>(title.size()), nullptr, 0);
        if (len > 0)
        {
            std::wstring wideTitle(static_cast<usize>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, title.data(), static_cast<int>(title.size()), wideTitle.data(), len);
            SetWindowTextW(m_hwnd, wideTitle.c_str());
        }
    }

    LRESULT CALLBACK Window::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Window* window = nullptr;

        if (msg == WM_NCCREATE)
        {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            window = static_cast<Window*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            if (window)
            {
                window->m_hwnd = hwnd;
            }
        }
        else
        {
            window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (window)
        {
            return window->HandleMessage(hwnd, msg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        {
            return true;
        }

        bool skipInput = false;
        if (ImGui::GetCurrentContext())
        {
            const ImGuiIO& io = ImGui::GetIO();
            const bool isMouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
                                    (msg == WM_MOUSEWHEEL) || (msg == WM_MOUSEHWHEEL);
            const bool isKeyMsg   = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);

            if ((isMouseMsg && io.WantCaptureMouse) || (isKeyMsg && io.WantCaptureKeyboard))
            {
                skipInput = true;
            }
        }

        if (!skipInput)
        {
            Input::ProcessMessage(hwnd, msg, wParam, lParam);
        }

        switch (msg)
        {
            case WM_CLOSE:
            {
                m_shouldClose = true;
                if (m_onClose)
                {
                    m_onClose();
                }
                return 0;
            }

            case WM_DESTROY:
            {
                PostQuitMessage(0);
                return 0;
            }

            case WM_SIZE:
            {
                const u32 newWidth = static_cast<u32>(LOWORD(lParam));
                const u32 newHeight = static_cast<u32>(HIWORD(lParam));

                if (wParam == SIZE_MINIMIZED)
                {
                    m_isMinimized = true;
                }
                else
                {
                    m_isMinimized = false;
                    if ((newWidth != m_width || newHeight != m_height) && newWidth > 0 && newHeight > 0)
                    {
                        m_width = newWidth;
                        m_height = newHeight;

                        if (!m_isResizing && m_onResize)
                        {
                            m_onResize(m_width, m_height);
                        }
                    }
                }
                return 0;
            }

            case WM_ENTERSIZEMOVE:
            {
                m_isResizing = true;
                return 0;
            }

            case WM_EXITSIZEMOVE:
            {
                m_isResizing = false;
                if (m_onResize && m_width > 0 && m_height > 0)
                {
                    m_onResize(m_width, m_height);
                }
                return 0;
            }

            case WM_SYSKEYDOWN:
            {

                if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
                {
                    return 0;
                }
                break;
            }

            default:
                break;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
