#pragma once

#include "Core/Types.hpp"
#include "Scene/Entity.hpp"

struct lua_State;

namespace RL
{
    class Scene;
    class TextureManager;

    class LuaEntityBridge
    {
    public:
        static void Register(lua_State* L, Scene* scene, TextureManager* textureManager = nullptr);
        static void SetScene(Scene* scene) noexcept { s_scene = scene; }
        [[nodiscard]] static Scene* GetScene() noexcept { return s_scene; }

        static void SetTextureManager(TextureManager* textureManager) noexcept { s_textureManager = textureManager; }
        [[nodiscard]] static TextureManager* GetTextureManager() noexcept { return s_textureManager; }

        static void SetStressTest(class StressTest* stressTest) noexcept { s_stressTest = stressTest; }
        [[nodiscard]] static class StressTest* GetStressTest() noexcept { return s_stressTest; }

        static void PushEntity(lua_State* L, Entity entity);

    private:
        struct LuaEntityUserData
        {
            u64 entityID;
        };

        static LuaEntityUserData* CheckEntity(lua_State* L, int arg);

        static int Entity_SetPosition(lua_State* L);
        static int Entity_GetPosition(lua_State* L);
        static int Entity_SetRotation(lua_State* L);
        static int Entity_GetRotation(lua_State* L);
        static int Entity_SetScale(lua_State* L);
        static int Entity_GetScale(lua_State* L);
        static int Entity_SetColor(lua_State* L);
        static int Entity_GetColor(lua_State* L);
        static int Entity_SetTexture(lua_State* L);
        static int Entity_GetTextureIndex(lua_State* L);
        static int Entity_GetTag(lua_State* L);
        static int Entity_SetTag(lua_State* L);
        static int Entity_SetVisible(lua_State* L);
        static int Entity_IsVisible(lua_State* L);
        static int Entity_IsValid(lua_State* L);
        static int Entity_ToString(lua_State* L);
        static int Entity_Equal(lua_State* L);

        static int Scene_CreateEntity(lua_State* L);
        static int Scene_DestroyEntity(lua_State* L);
        static int Scene_IsValid(lua_State* L);
        static int Scene_Clear(lua_State* L);
        static int Scene_GetEntityCount(lua_State* L);
        static int Scene_StressTest(lua_State* L);
        static int Scene_ClearStressTest(lua_State* L);

        static int Texture_Load(lua_State* L);
        static int Texture_CreateColor(lua_State* L);

        static int Audio_Play(lua_State* L);
        static int Audio_Play3D(lua_State* L);
        static int Audio_SetMasterVolume(lua_State* L);

    private:
        inline static Scene* s_scene = nullptr;
        inline static TextureManager* s_textureManager = nullptr;
        inline static class StressTest* s_stressTest = nullptr;
    };
}
