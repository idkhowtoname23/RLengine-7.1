#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244 4245 4310 4456 4457 4701 4706 4100)
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "Audio/AudioEngine.hpp"
#include "Core/Logger.hpp"

#include <format>
#include <algorithm>

namespace RL
{
    struct AudioEngine::Impl
    {
        ma_engine engine{};
    };

    AudioEngine& AudioEngine::Get() noexcept
    {
        static AudioEngine instance;
        return instance;
    }

    AudioEngine::AudioEngine() = default;

    AudioEngine::~AudioEngine()
    {
        Shutdown();
    }

    std::expected<void, std::string> AudioEngine::Initialize()
    {
        if (m_isInitialized) return {};

        m_impl = std::make_unique<Impl>();
        ma_result res = ma_engine_init(nullptr, &m_impl->engine);
        if (res != MA_SUCCESS)
        {
            m_impl.reset();
            return std::unexpected(std::format("Failed to initialize miniaudio engine: error code {}", static_cast<int>(res)));
        }

        m_isInitialized = true;
        RL_LOG_INFO("[AudioEngine] 3D Spatial Audio subsystem initialized successfully via miniaudio.");
        return {};
    }

    void AudioEngine::Shutdown() noexcept
    {
        if (!m_isInitialized || !m_impl) return;

        for (auto& sound : m_activeSounds)
        {
            if (sound)
            {
                ma_sound_uninit(sound.get());
            }
        }
        m_activeSounds.clear();

        ma_engine_uninit(&m_impl->engine);
        m_impl.reset();
        m_isInitialized = false;
        RL_LOG_INFO("[AudioEngine] Audio subsystem shut down cleanly.");
    }

    void AudioEngine::Update() noexcept
    {
        if (!m_isInitialized || !m_impl) return;

        std::erase_if(m_activeSounds, [](const std::unique_ptr<ma_sound>& sound) {
            if (!sound) return true;
            if (ma_sound_at_end(sound.get()) || !ma_sound_is_playing(sound.get()))
            {
                ma_sound_uninit(sound.get());
                return true;
            }
            return false;
        });
    }

    void AudioEngine::SetListenerPosition(f32 x, f32 y, f32 z) noexcept
    {
        if (!m_isInitialized || !m_impl) return;
        ma_engine_listener_set_position(&m_impl->engine, 0, x, y, z);
    }

    void AudioEngine::SetListenerDirection(f32 forwardX, f32 forwardY, f32 forwardZ, f32 upX, f32 upY, f32 upZ) noexcept
    {
        if (!m_isInitialized || !m_impl) return;
        ma_engine_listener_set_direction(&m_impl->engine, 0, forwardX, forwardY, forwardZ);
        ma_engine_listener_set_world_up(&m_impl->engine, 0, upX, upY, upZ);
    }

    void AudioEngine::PlaySound(std::string_view filepath, f32 volume)
    {
        if (!m_isInitialized || !m_impl) return;
        std::string path(filepath);
        ma_engine_play_sound(&m_impl->engine, path.c_str(), nullptr);
    }

    void AudioEngine::PlaySound3D(std::string_view filepath, f32 x, f32 y, f32 z, f32 volume, f32 minDistance, f32 maxDistance)
    {
        if (!m_isInitialized || !m_impl) return;
        std::string path(filepath);
        auto sound = std::make_unique<ma_sound>();
        ma_result res = ma_sound_init_from_file(&m_impl->engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, sound.get());
        if (res == MA_SUCCESS)
        {
            ma_sound_set_position(sound.get(), x, y, z);
            ma_sound_set_min_distance(sound.get(), minDistance);
            ma_sound_set_max_distance(sound.get(), maxDistance);
            ma_sound_set_volume(sound.get(), volume);
            ma_sound_start(sound.get());
            m_activeSounds.push_back(std::move(sound));
        }
    }

    void AudioEngine::SetMasterVolume(f32 volume) noexcept
    {
        m_masterVolume = volume;
        if (m_isInitialized && m_impl)
        {
            ma_engine_set_volume(&m_impl->engine, volume);
        }
    }
}
