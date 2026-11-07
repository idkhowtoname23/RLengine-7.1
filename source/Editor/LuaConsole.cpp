#include "Editor/LuaConsole.hpp"
#include "Scripting/ScriptEngine.hpp"

#include <imgui.h>
#include <algorithm>
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace RL
{
    LuaConsole::LuaConsole()
    {
        Clear();
        AddLog("RLengine 7.1 In-Game Lua Console initialized. Ready for REPL commands.", LogLevel::Info);
        AddLog("Type Lua code (e.g. 'RL.Log(\"Hello\")' or '=1+1') and press Enter.", LogLevel::Info);
    }

    void LuaConsole::Clear()
    {
        m_items.clear();
    }

    void LuaConsole::AddLog(std::string text, LogLevel level, bool isError)
    {

        if (m_items.size() > 2048)
        {
            m_items.erase(m_items.begin(), m_items.begin() + 256);
        }

        m_items.push_back(ConsoleItem{
            .Text = std::move(text),
            .Level = level,
            .IsError = isError
        });

        m_scrollToBottom = true;
    }

    void LuaConsole::Execute(const std::string& commandLine)
    {

        const auto start = commandLine.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
        {
            return;
        }
        const auto end = commandLine.find_last_not_of(" \t\r\n");
        const std::string cmd = commandLine.substr(start, end - start + 1);

        m_historyPos = -1;
        if (m_history.empty() || m_history.back() != cmd)
        {
            m_history.push_back(cmd);
        }

        AddLog("> " + cmd, LogLevel::Info, false);

        if (!m_scriptEngine)
        {
            AddLog("[Error] ScriptEngine is not connected to LuaConsole.", LogLevel::Error, true);
            return;
        }

        lua_State* L = m_scriptEngine->GetLuaState();
        if (!L)
        {
            AddLog("[Error] Lua State is null.", LogLevel::Error, true);
            return;
        }

        std::string codeToRun = cmd;
        if (codeToRun.starts_with("="))
        {
            codeToRun = "RL.Log(tostring(" + codeToRun.substr(1) + "))";
        }

        const int status = luaL_dostring(L, codeToRun.c_str());
        if (status != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            AddLog(std::string("[Lua Error] ") + (err ? err : "Unknown error"), LogLevel::Error, true);
            lua_pop(L, 1);
        }
    }

    void LuaConsole::Render()
    {
        if (!m_isOpen)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(800, 460), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RLengine 7.1 - In-Game Lua REPL Console [~ or F1]", &m_isOpen))
        {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Clear"))
        {
            Clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy All"))
        {
            ImGui::LogToClipboard();
            for (const auto& item : m_items)
            {
                ImGui::LogText("%s\n", item.Text.c_str());
            }
            ImGui::LogFinish();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| Total Entries: %zu", m_items.size());

        ImGui::Separator();

        const f32 footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

            for (const auto& item : m_items)
            {
                ImVec4 col;
                if (item.IsError || item.Level == LogLevel::Error || item.Level == LogLevel::Fatal)
                {
                    col = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                }
                else if (item.Level == LogLevel::Warn)
                {
                    col = ImVec4(1.0f, 0.85f, 0.25f, 1.0f);
                }
                else if (item.Text.starts_with("> "))
                {
                    col = ImVec4(0.3f, 0.95f, 1.0f, 1.0f);
                }
                else
                {
                    col = ImVec4(0.88f, 0.88f, 0.92f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(item.Text.c_str());
                ImGui::PopStyleColor();
            }

            if (m_scrollToBottom || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            {
                ImGui::SetScrollHereY(1.0f);
            }
            m_scrollToBottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        ImGui::Separator();

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.3f, 0.95f, 1.0f, 1.0f), "lua>");
        ImGui::SameLine();

        ImGui::PushItemWidth(-1);
        const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;

        if (ImGui::InputText("##ConsoleInput", m_inputBuf, sizeof(m_inputBuf), inputFlags, &LuaConsole::TextEditCallbackStub, this))
        {
            Execute(m_inputBuf);
            m_inputBuf[0] = '\0';
            m_reclaimFocus = true;
        }

        ImGui::SetItemDefaultFocus();
        if (m_reclaimFocus)
        {
            ImGui::SetKeyboardFocusHere(-1);
            m_reclaimFocus = false;
        }
        ImGui::PopItemWidth();

        ImGui::End();
    }

    int LuaConsole::TextEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        auto* console = static_cast<LuaConsole*>(data->UserData);
        return console->TextEditCallback(data);
    }

    int LuaConsole::TextEditCallback(ImGuiInputTextCallbackData* cbData)
    {
        if (cbData->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        {
            const i32 prevHistoryPos = m_historyPos;

            if (cbData->EventKey == ImGuiKey_UpArrow)
            {
                if (m_historyPos == -1)
                {
                    m_historyPos = static_cast<i32>(m_history.size()) - 1;
                }
                else if (m_historyPos > 0)
                {
                    m_historyPos--;
                }
            }
            else if (cbData->EventKey == ImGuiKey_DownArrow)
            {
                if (m_historyPos != -1)
                {
                    m_historyPos++;
                    if (m_historyPos >= static_cast<i32>(m_history.size()))
                    {
                        m_historyPos = -1;
                    }
                }
            }

            if (prevHistoryPos != m_historyPos)
            {
                const char* historyStr = (m_historyPos >= 0) ? m_history[static_cast<usize>(m_historyPos)].c_str() : "";
                cbData->DeleteChars(0, cbData->BufTextLen);
                cbData->InsertChars(0, historyStr);
            }
        }

        return 0;
    }
}
