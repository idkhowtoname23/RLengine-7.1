#include "Scripting/LuaInputBridge.hpp"
#include "Platform/Input.hpp"
#include "Platform/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Core/Logger.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace RL
{
    void LuaInputBridge::SetContext(Scene* scene, Window* window) noexcept
    {
        s_scene = scene;
        s_window = window;
    }

    void LuaInputBridge::Register(lua_State* L, Scene* scene, Window* window)
    {
        SetContext(scene, window);

        lua_getglobal(L, "RL");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, "RL");
        }

        lua_newtable(L);

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_IsKeyDown);
        lua_setfield(L, -2, "IsKeyDown");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_IsKeyPressed);
        lua_setfield(L, -2, "IsKeyPressed");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_IsKeyReleased);
        lua_setfield(L, -2, "IsKeyReleased");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_GetMousePos);
        lua_setfield(L, -2, "GetMousePos");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_GetMouseDelta);
        lua_setfield(L, -2, "GetMouseDelta");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_GetMouseScroll);
        lua_setfield(L, -2, "GetMouseScroll");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_IsMouseDown);
        lua_setfield(L, -2, "IsMouseDown");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_IsMousePressed);
        lua_setfield(L, -2, "IsMousePressed");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Input_IsMouseReleased);
        lua_setfield(L, -2, "IsMouseReleased");

        lua_setfield(L, -2, "Input");

        lua_newtable(L);

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_SetPosition);
        lua_setfield(L, -2, "SetPosition");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_GetPosition);
        lua_setfield(L, -2, "GetPosition");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_Move);
        lua_setfield(L, -2, "Move");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_SetZoom);
        lua_setfield(L, -2, "SetZoom");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_GetZoom);
        lua_setfield(L, -2, "GetZoom");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_SetRotation);
        lua_setfield(L, -2, "SetRotation");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_GetRotation);
        lua_setfield(L, -2, "GetRotation");

        lua_pushcfunction(L, &LuaInputBridge::Lua_Camera_ScreenToWorld);
        lua_setfield(L, -2, "ScreenToWorld");

        lua_setfield(L, -2, "Camera");

        lua_pop(L, 1);

        RL_LOG_INFO("[RLScript] Lua RL.Input and RL.Camera subsystems registered successfully.");
    }

    int LuaInputBridge::Lua_Input_IsKeyDown(lua_State* L)
    {
        const char* keyName = luaL_checkstring(L, 1);
        const KeyCode key = Input::StringToKeyCode(keyName);
        lua_pushboolean(L, Input::IsKeyDown(key));
        return 1;
    }

    int LuaInputBridge::Lua_Input_IsKeyPressed(lua_State* L)
    {
        const char* keyName = luaL_checkstring(L, 1);
        const KeyCode key = Input::StringToKeyCode(keyName);
        lua_pushboolean(L, Input::IsKeyPressed(key));
        return 1;
    }

    int LuaInputBridge::Lua_Input_IsKeyReleased(lua_State* L)
    {
        const char* keyName = luaL_checkstring(L, 1);
        const KeyCode key = Input::StringToKeyCode(keyName);
        lua_pushboolean(L, Input::IsKeyReleased(key));
        return 1;
    }

    int LuaInputBridge::Lua_Input_GetMousePos(lua_State* L)
    {
        const auto pos = Input::GetMousePosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        return 2;
    }

    int LuaInputBridge::Lua_Input_GetMouseDelta(lua_State* L)
    {
        const auto delta = Input::GetMouseDelta();
        lua_pushnumber(L, delta.x);
        lua_pushnumber(L, delta.y);
        return 2;
    }

    int LuaInputBridge::Lua_Input_GetMouseScroll(lua_State* L)
    {
        lua_pushnumber(L, Input::GetMouseScroll());
        return 1;
    }

    int LuaInputBridge::Lua_Input_IsMouseDown(lua_State* L)
    {
        const int btn = static_cast<int>(luaL_optinteger(L, 1, 1));
        const MouseButton mb = (btn == 2) ? MouseButton::Right
                             : (btn == 3) ? MouseButton::Middle
                             : MouseButton::Left;
        lua_pushboolean(L, Input::IsMouseButtonDown(mb));
        return 1;
    }

    int LuaInputBridge::Lua_Input_IsMousePressed(lua_State* L)
    {
        const int btn = static_cast<int>(luaL_optinteger(L, 1, 1));
        const MouseButton mb = (btn == 2) ? MouseButton::Right
                             : (btn == 3) ? MouseButton::Middle
                             : MouseButton::Left;
        lua_pushboolean(L, Input::IsMouseButtonPressed(mb));
        return 1;
    }

    int LuaInputBridge::Lua_Input_IsMouseReleased(lua_State* L)
    {
        const int btn = static_cast<int>(luaL_optinteger(L, 1, 1));
        const MouseButton mb = (btn == 2) ? MouseButton::Right
                             : (btn == 3) ? MouseButton::Middle
                             : MouseButton::Left;
        lua_pushboolean(L, Input::IsMouseButtonReleased(mb));
        return 1;
    }

    int LuaInputBridge::Lua_Camera_SetPosition(lua_State* L)
    {
        if (!s_scene) return 0;
        const f32 x = static_cast<f32>(luaL_checknumber(L, 1));
        const f32 y = static_cast<f32>(luaL_checknumber(L, 2));
        const f32 z = static_cast<f32>(luaL_optnumber(L, 3, -10.0));
        s_scene->GetCamera().SetPosition(x, y, z);
        return 0;
    }

    int LuaInputBridge::Lua_Camera_GetPosition(lua_State* L)
    {
        if (!s_scene)
        {
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, -10.0);
            return 3;
        }
        const auto& pos = s_scene->GetCamera().GetPosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 3;
    }

    int LuaInputBridge::Lua_Camera_Move(lua_State* L)
    {
        if (!s_scene) return 0;
        const f32 dx = static_cast<f32>(luaL_checknumber(L, 1));
        const f32 dy = static_cast<f32>(luaL_checknumber(L, 2));
        const f32 dz = static_cast<f32>(luaL_optnumber(L, 3, 0.0));
        s_scene->GetCamera().Move(dx, dy, dz);
        return 0;
    }

    int LuaInputBridge::Lua_Camera_SetZoom(lua_State* L)
    {
        if (!s_scene) return 0;
        const f32 zoom = static_cast<f32>(luaL_checknumber(L, 1));
        s_scene->GetCamera().SetZoom(zoom);
        return 0;
    }

    int LuaInputBridge::Lua_Camera_GetZoom(lua_State* L)
    {
        if (!s_scene)
        {
            lua_pushnumber(L, 1.0);
            return 1;
        }
        lua_pushnumber(L, s_scene->GetCamera().GetZoom());
        return 1;
    }

    int LuaInputBridge::Lua_Camera_SetRotation(lua_State* L)
    {
        if (!s_scene) return 0;
        const f32 pitch = static_cast<f32>(luaL_checknumber(L, 1));
        const f32 yaw   = static_cast<f32>(luaL_checknumber(L, 2));
        const f32 roll  = static_cast<f32>(luaL_optnumber(L, 3, 0.0));
        s_scene->GetCamera().SetRotation(pitch, yaw, roll);
        return 0;
    }

    int LuaInputBridge::Lua_Camera_GetRotation(lua_State* L)
    {
        if (!s_scene)
        {
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            return 3;
        }
        const auto& rot = s_scene->GetCamera().GetRotation();
        lua_pushnumber(L, rot.x);
        lua_pushnumber(L, rot.y);
        lua_pushnumber(L, rot.z);
        return 3;
    }

    int LuaInputBridge::Lua_Camera_ScreenToWorld(lua_State* L)
    {
        if (!s_scene)
        {
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            return 2;
        }

        const f32 screenX = static_cast<f32>(luaL_checknumber(L, 1));
        const f32 screenY = static_cast<f32>(luaL_checknumber(L, 2));

        const f32 winW = (s_window) ? static_cast<f32>(s_window->GetWidth()) : 1600.0f;
        const f32 winH = (s_window) ? static_cast<f32>(s_window->GetHeight()) : 900.0f;

        const auto worldPos = s_scene->GetCamera().ScreenToWorld2D(screenX, screenY, winW, winH);
        lua_pushnumber(L, worldPos.x);
        lua_pushnumber(L, worldPos.y);
        return 2;
    }
}
