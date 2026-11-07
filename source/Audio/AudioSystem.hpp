#pragma once

#include "Core/Types.hpp"
#include <string_view>

namespace RL
{
    class Scene;

    class AudioSystem
    {
    public:
        AudioSystem() = default;
        ~AudioSystem() = default;

        void Update(Scene& scene, f32 dt);
    };
}
