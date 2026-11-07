#pragma once

#include "Core/Types.hpp"
#include "Core/StringID.hpp"

#include <string>

namespace RL
{

    struct WindowResizedEvent
    {
        u32 width;
        u32 height;
    };

    struct WindowClosedEvent {};

    struct WindowFocusChangedEvent
    {
        bool focused;
    };

    struct KeyPressedEvent
    {
        i32 keyCode;
        bool repeated;
    };

    struct KeyReleasedEvent
    {
        i32 keyCode;
    };

    struct MouseButtonPressedEvent
    {
        i32 button;
        f32 x, y;
    };

    struct MouseButtonReleasedEvent
    {
        i32 button;
        f32 x, y;
    };

    struct MouseMovedEvent
    {
        f32 x, y;
        f32 deltaX, deltaY;
    };

    struct MouseScrolledEvent
    {
        f32 offsetX;
        f32 offsetY;
    };

    struct EntityCreatedEvent
    {
        u64 entityID;
        StringID tag;
    };

    struct EntityDestroyedEvent
    {
        u64 entityID;
    };

    struct SceneLoadedEvent
    {
        std::string sceneName;
    };

    struct SceneUnloadedEvent
    {
        std::string sceneName;
    };

    struct SimulationStartedEvent {};
    struct SimulationPausedEvent  {};
    struct SimulationStoppedEvent {};

    struct AssetReloadedEvent
    {
        std::string assetPath;
        StringID    assetType;
    };

    struct EngineShutdownEvent {};

}
