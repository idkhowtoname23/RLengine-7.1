#pragma once

#include "Core/Types.hpp"

struct lua_State;

namespace RL
{
    class Scene;
    class Window;

    class LuaInputBridge
    {
    public:
        LuaInputBridge() = delete;

        static void Register(lua_State* L, Scene* scene, Window* window);
        static void SetContext(Scene* scene, Window* window) noexcept;

    private:

        static int Lua_Input_IsKeyDown(lua_State* L);
        static int Lua_Input_IsKeyPressed(lua_State* L);
        static int Lua_Input_IsKeyReleased(lua_State* L);
        static int Lua_Input_GetMousePos(lua_State* L);
        static int Lua_Input_GetMouseDelta(lua_State* L);
        static int Lua_Input_GetMouseScroll(lua_State* L);
        static int Lua_Input_IsMouseDown(lua_State* L);
        static int Lua_Input_IsMousePressed(lua_State* L);
        static int Lua_Input_IsMouseReleased(lua_State* L);

        static int Lua_Camera_SetPosition(lua_State* L);
        static int Lua_Camera_GetPosition(lua_State* L);
        static int Lua_Camera_Move(lua_State* L);
        static int Lua_Camera_SetZoom(lua_State* L);
        static int Lua_Camera_GetZoom(lua_State* L);
        static int Lua_Camera_SetRotation(lua_State* L);
        static int Lua_Camera_GetRotation(lua_State* L);
        static int Lua_Camera_ScreenToWorld(lua_State* L);

    private:
        static inline Scene* s_scene = nullptr;
        static inline Window* s_window = nullptr;
    };
}
