#pragma once

#include "Core/Types.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace RL
{
    class Timer
    {
    public:
        Timer() noexcept
        {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            m_secondsPerCount = 1.0 / static_cast<f64>(freq.QuadPart);
            Reset();
        }

        void Reset() noexcept
        {
            LARGE_INTEGER curTime;
            QueryPerformanceCounter(&curTime);

            m_baseTime = curTime.QuadPart;
            m_prevTime = curTime.QuadPart;
            m_stopTime = 0;
            m_stopped = false;

            m_deltaTime = 0.0f;
            m_totalTime = 0.0;
            m_fps = 0;
            m_frameTimeMS = 0.0f;
            m_frameCount = 0;
            m_fpsFrameCounter = 0;
            m_fpsAccumulator = 0.0;
        }

        void Start() noexcept
        {
            if (m_stopped)
            {
                LARGE_INTEGER startTime;
                QueryPerformanceCounter(&startTime);

                m_pausedTime += (startTime.QuadPart - m_stopTime);
                m_prevTime = startTime.QuadPart;
                m_stopTime = 0;
                m_stopped = false;
            }
        }

        void Stop() noexcept
        {
            if (!m_stopped)
            {
                LARGE_INTEGER curTime;
                QueryPerformanceCounter(&curTime);
                m_stopTime = curTime.QuadPart;
                m_stopped = true;
            }
        }

        void Tick() noexcept
        {
            if (m_stopped)
            {
                m_deltaTime = 0.0f;
                return;
            }

            LARGE_INTEGER curTime;
            QueryPerformanceCounter(&curTime);
            const i64 currentCount = curTime.QuadPart;

            f64 delta = static_cast<f64>(currentCount - m_prevTime) * m_secondsPerCount;
            m_prevTime = currentCount;

            if (delta < 0.0)
            {
                delta = 0.0;
            }
            if (delta > 0.25)
            {
                delta = 0.25;
            }

            m_deltaTime = static_cast<f32>(delta);
            m_totalTime = static_cast<f64>((currentCount - m_pausedTime) - m_baseTime) * m_secondsPerCount;

            m_frameCount++;
            m_fpsFrameCounter++;
            m_fpsAccumulator += delta;

            if (m_fpsAccumulator >= 0.5)
            {
                m_fps = static_cast<u32>(static_cast<f64>(m_fpsFrameCounter) / m_fpsAccumulator + 0.5);
                m_frameTimeMS = static_cast<f32>((m_fpsAccumulator / static_cast<f64>(m_fpsFrameCounter)) * 1000.0);
                m_fpsFrameCounter = 0;
                m_fpsAccumulator = 0.0;
            }
        }

        [[nodiscard]] f32 GetDeltaTime() const noexcept { return m_deltaTime; }
        [[nodiscard]] f64 GetTotalTime() const noexcept { return m_totalTime; }
        [[nodiscard]] u32 GetFPS() const noexcept { return m_fps; }
        [[nodiscard]] f32 GetFrameTimeMS() const noexcept { return m_frameTimeMS; }
        [[nodiscard]] u64 GetFrameCount() const noexcept { return m_frameCount; }

    private:
        f64 m_secondsPerCount = 0.0;
        f32 m_deltaTime = 0.0f;
        f64 m_totalTime = 0.0;

        i64 m_baseTime = 0;
        i64 m_pausedTime = 0;
        i64 m_stopTime = 0;
        i64 m_prevTime = 0;

        u32 m_fps = 0;
        f32 m_frameTimeMS = 0.0f;
        u64 m_frameCount = 0;
        u32 m_fpsFrameCounter = 0;
        f64 m_fpsAccumulator = 0.0;

        bool m_stopped = false;
    };
}
