#include "Scripting/ScriptEngine.hpp"
#include "Scripting/LuaEntityBridge.hpp"
#include "Scripting/LuaInputBridge.hpp"
#include "Scene/Scene.hpp"

#include <lua.hpp>
#include <format>

namespace RL
{
    ScriptEngine::ScriptEngine(ScriptEngine&& other) noexcept
        : m_L(other.m_L)
        , m_scene(other.m_scene)
        , m_textureManager(other.m_textureManager)
        , m_window(other.m_window)
        , m_scriptPath(std::move(other.m_scriptPath))
        , m_lastWriteTime(other.m_lastWriteTime)
        , m_reloadCheckTimer(other.m_reloadCheckTimer)
    {
        other.m_L = nullptr;
        other.m_scene = nullptr;
        other.m_textureManager = nullptr;
        other.m_window = nullptr;
        if (s_activeInstance == &other)
        {
            s_activeInstance = this;
        }
    }

    ScriptEngine& ScriptEngine::operator=(ScriptEngine&& other) noexcept
    {
        if (this != &other)
        {
            Shutdown();
            m_L = other.m_L;
            m_scene = other.m_scene;
            m_textureManager = other.m_textureManager;
            m_window = other.m_window;
            m_scriptPath = std::move(other.m_scriptPath);
            m_lastWriteTime = other.m_lastWriteTime;
            m_reloadCheckTimer = other.m_reloadCheckTimer;
            other.m_L = nullptr;
            other.m_scene = nullptr;
            other.m_textureManager = nullptr;
            other.m_window = nullptr;
            if (s_activeInstance == &other)
            {
                s_activeInstance = this;
            }
        }
        return *this;
    }

    ScriptEngine::~ScriptEngine()
    {
        Shutdown();
    }

    void ScriptEngine::SetScene(Scene* scene) noexcept
    {
        m_scene = scene;
        LuaEntityBridge::SetScene(scene);
        LuaInputBridge::SetContext(scene, m_window);
    }

    void ScriptEngine::SetTextureManager(TextureManager* textureManager) noexcept
    {
        m_textureManager = textureManager;
        LuaEntityBridge::SetTextureManager(textureManager);
    }

    std::expected<void, std::string> ScriptEngine::Initialize(
        const std::filesystem::path& mainScriptPath,
        Scene* scene,
        TextureManager* textureManager,
        Window* window)
    {
        if (m_L)
        {
            Shutdown();
        }

        RL_LOG_INFO("[RLScript] Initializing Lua 5.4 scripting environment...");

        s_activeInstance = this;
        m_scene = scene;
        m_textureManager = textureManager;
        m_window = window;

        m_L = luaL_newstate();
        if (!m_L)
        {
            return std::unexpected("Failed to create Lua state (luaL_newstate returned null).");
        }

        static const luaL_Reg safeLibs[] = {
            { "_G", luaopen_base },
            { LUA_TABLIBNAME, luaopen_table },
            { LUA_STRLIBNAME, luaopen_string },
            { LUA_MATHLIBNAME, luaopen_math },
            { LUA_UTF8LIBNAME, luaopen_utf8 },
            { LUA_COLIBNAME, luaopen_coroutine },
            { nullptr, nullptr }
        };

        for (const auto* lib = safeLibs; lib->func; ++lib)
        {
            luaL_requiref(m_L, lib->name, lib->func, 1);
            lua_pop(m_L, 1);
        }

        lua_pushnil(m_L);
        lua_setglobal(m_L, "dofile");
        lua_pushnil(m_L);
        lua_setglobal(m_L, "loadfile");
        lua_pushnil(m_L);
        lua_setglobal(m_L, "require");
        lua_pushnil(m_L);
        lua_setglobal(m_L, "package");
        lua_pushnil(m_L);
        lua_setglobal(m_L, "io");
        lua_pushnil(m_L);
        lua_setglobal(m_L, "os");
        lua_pushnil(m_L);
        lua_setglobal(m_L, "debug");

        lua_getfield(m_L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
        if (lua_istable(m_L, -1))
        {
            lua_pushnil(m_L); lua_setfield(m_L, -2, "io");
            lua_pushnil(m_L); lua_setfield(m_L, -2, "os");
            lua_pushnil(m_L); lua_setfield(m_L, -2, "package");
            lua_pushnil(m_L); lua_setfield(m_L, -2, "debug");
        }
        lua_pop(m_L, 1);

        lua_pushcfunction(m_L, &ScriptEngine::Lua_Log);
        lua_setglobal(m_L, "print");

        luaL_dostring(m_L, "local _load = load; load = function(c, n, m, e) return _load(c, n, 't', e) end");

        RegisterEngineBindings();

        m_scriptPath = mainScriptPath;

        if (!std::filesystem::exists(m_scriptPath))
        {
            return std::unexpected(std::format("Main Lua script not found: '{}'", m_scriptPath.string()));
        }

        std::error_code ec;
        m_lastWriteTime = std::filesystem::last_write_time(m_scriptPath, ec);

        if (!ReloadScript())
        {
            return std::unexpected(std::format("Failed to execute initial Lua script '{}'.", m_scriptPath.string()));
        }

        RL_LOG_INFO("[RLScript] Scripting environment initialized successfully. File: '{}'", m_scriptPath.string());
        return {};
    }

    void ScriptEngine::Shutdown() noexcept
    {
        if (s_activeInstance == this)
        {
            s_activeInstance = nullptr;
        }

        if (m_L)
        {
            lua_close(m_L);
            m_L = nullptr;
            RL_LOG_INFO("[RLScript] Lua scripting environment closed.");
        }
    }

    void ScriptEngine::RegisterEngineBindings()
    {

        lua_newtable(m_L);

        lua_pushcfunction(m_L, &ScriptEngine::Lua_Log);
        lua_setfield(m_L, -2, "Log");

        lua_setglobal(m_L, "RL");

        LuaEntityBridge::Register(m_L, m_scene, m_textureManager);

        LuaInputBridge::Register(m_L, m_scene, m_window);

        RL_LOG_INFO("[RLScript] Engine bindings registered into Lua global scope 'RL'.");
    }

    int ScriptEngine::Lua_Log(lua_State* L)
    {
        const char* str = luaL_checkstring(L, 1);
        if (str)
        {
            RL_LOG_INFO("[Lua] {}", str);
        }
        return 0;
    }

    bool ScriptEngine::ReloadScript()
    {
        if (!m_L) return false;

        const std::string pathStr = m_scriptPath.string();
        RL_LOG_INFO("[RLScript] Loading script file '{}'...", pathStr);

        const int loadStatus = luaL_loadfile(m_L, pathStr.c_str());
        if (loadStatus != LUA_OK)
        {
            const char* err = lua_tostring(m_L, -1);
            RL_LOG_ERROR("[RLScript Compilation Error] {}", err ? err : "Syntax error");
            lua_pop(m_L, 1);
            return false;
        }

        const int execStatus = lua_pcall(m_L, 0, 0, 0);
        if (execStatus != LUA_OK)
        {
            const char* err = lua_tostring(m_L, -1);
            RL_LOG_ERROR("[RLScript Execution Error] {}", err ? err : "Runtime error");
            lua_pop(m_L, 1);
            return false;
        }

        RL_LOG_INFO("[RLScript] Script '{}' loaded and executed successfully.", pathStr);

        lua_getglobal(m_L, "Init");
        if (lua_isfunction(m_L, -1))
        {
            RL_LOG_INFO("[RLScript] Invoking Lua Init() entrypoint...");
            if (lua_pcall(m_L, 0, 0, 0) != LUA_OK)
            {
                const char* err = lua_tostring(m_L, -1);
                RL_LOG_ERROR("[RLScript Runtime Error in Init()] {}", err ? err : "Unknown error");
                lua_pop(m_L, 1);
                return false;
            }
        }
        else
        {
            lua_pop(m_L, 1);
        }

        return true;
    }

    void ScriptEngine::Tick(f32 dt)
    {
        if (!m_L) return;

        m_reloadCheckTimer += dt;
        if (m_reloadCheckTimer >= 0.1f)
        {
            m_reloadCheckTimer = 0.0f;

            std::error_code ec;
            const auto currentWriteTime = std::filesystem::last_write_time(m_scriptPath, ec);
            if (!ec && currentWriteTime != m_lastWriteTime)
            {
                m_lastWriteTime = currentWriteTime;
                RL_LOG_INFO("[RLScript] Detected file modification in '{}'. Triggering Hot-Reload...", m_scriptPath.string());
                ReloadScript();
            }
        }

        lua_getglobal(m_L, "Update");
        if (lua_isfunction(m_L, -1))
        {
            lua_pushnumber(m_L, static_cast<lua_Number>(dt));
            if (lua_pcall(m_L, 1, 0, 0) != LUA_OK)
            {
                const char* err = lua_tostring(m_L, -1);
                RL_LOG_ERROR("[RLScript Runtime Error in Update()] {}", err ? err : "Unknown error");
                lua_pop(m_L, 1);
            }
        }
        else
        {
            lua_pop(m_L, 1);
        }
    }

    bool ScriptEngine::ExecuteString(std::string_view code)
    {
        if (!m_L)
        {
            return false;
        }

        const std::string codeStr(code);
        const int status = luaL_dostring(m_L, codeStr.c_str());
        if (status != LUA_OK)
        {
            const char* err = lua_tostring(m_L, -1);
            RL_LOG_ERROR("[Lua] {}", err ? err : "Unknown error");
            lua_pop(m_L, 1);
            return false;
        }

        return true;
    }
}
