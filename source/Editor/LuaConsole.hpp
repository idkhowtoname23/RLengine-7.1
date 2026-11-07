#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"

#include <string>
#include <vector>
#include <string_view>

struct lua_State;
struct ImGuiInputTextCallbackData;

namespace RL
{
    class ScriptEngine;

    struct ConsoleItem
    {
        std::string Text;
        LogLevel    Level   = LogLevel::Info;
        bool        IsError = false;
    };

    class LuaConsole
    {
    public:
        LuaConsole();
        ~LuaConsole() = default;

        void SetScriptEngine(ScriptEngine* scriptEngine) noexcept { m_scriptEngine = scriptEngine; }

        void Toggle() noexcept { m_isOpen = !m_isOpen; }
        [[nodiscard]] bool IsOpen() const noexcept { return m_isOpen; }
        void SetOpen(bool open) noexcept { m_isOpen = open; }

        void AddLog(std::string text, LogLevel level = LogLevel::Info, bool isError = false);
        void Clear();
        void Execute(const std::string& commandLine);

        void Render();

    private:
        static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
        int TextEditCallback(ImGuiInputTextCallbackData* data);

    private:
        bool m_isOpen = false;
        std::vector<ConsoleItem> m_items;
        std::vector<std::string> m_history;
        i32 m_historyPos = -1;

        char m_inputBuf[1024]{};
        bool m_scrollToBottom = true;
        bool m_reclaimFocus = false;

        ScriptEngine* m_scriptEngine = nullptr;
    };
}
