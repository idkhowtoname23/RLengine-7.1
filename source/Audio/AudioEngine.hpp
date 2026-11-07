#pragma once

#include "Core/Types.hpp"
#include <string>
#include <string_view>
#include <expected>
#include <memory>
#include <vector>
#include <unordered_map>

struct ma_sound;

namespace RL
{
    class AudioEngine
    {
    public:
        static AudioEngine& Get() noexcept;

        AudioEngine();
        ~AudioEngine();

        AudioEngine(const AudioEngine&) = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        [[nodiscard]] std::expected<void, std::string> Initialize();
        void Shutdown() noexcept;

        void Update() noexcept;

        void SetListenerPosition(f32 x, f32 y, f32 z) noexcept;
        void SetListenerDirection(f32 forwardX, f32 forwardY, f32 forwardZ, f32 upX = 0.0f, f32 upY = 1.0f, f32 upZ = 0.0f) noexcept;

        void PlaySound(std::string_view filepath, f32 volume = 1.0f);
        void PlaySound3D(std::string_view filepath, f32 x, f32 y, f32 z, f32 volume = 1.0f, f32 minDistance = 1.0f, f32 maxDistance = 50.0f);

        void SetMasterVolume(f32 volume) noexcept;
        [[nodiscard]] f32 GetMasterVolume() const noexcept { return m_masterVolume; }
        [[nodiscard]] bool IsInitialized() const noexcept { return m_isInitialized; }

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        std::vector<std::unique_ptr<ma_sound>> m_activeSounds;
        bool m_isInitialized = false;
        f32 m_masterVolume = 1.0f;
    };
}
