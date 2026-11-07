#pragma once

#include "Core/Types.hpp"

#include <print>
#include <format>
#include <string_view>
#include <source_location>
#include <chrono>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <functional>
#include <windows.h>

namespace RL
{
    enum class LogLevel : u8
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    class Logger
    {
    public:
        using LogCallback = std::function<void(LogLevel level, std::string_view message)>;

        static void Flush() noexcept
        {
            fflush(stdout);
            fflush(stderr);
        }

        static void SetMinLogLevel(LogLevel level) noexcept
        {
            s_minLevel = level;
        }

        static LogLevel GetMinLogLevel() noexcept
        {
            return s_minLevel;
        }

        static void SetLogCallback(LogCallback callback) noexcept
        {
            std::lock_guard lock(s_callbackMutex);
            s_callback = std::move(callback);
        }

        static void RemoveLogCallback() noexcept
        {
            std::lock_guard lock(s_callbackMutex);
            s_callback = nullptr;
        }

        template <typename... Args>
        static void Log(LogLevel level,
                        const std::source_location& loc,
                        std::format_string<Args...> fmt,
                        Args&&... args)
        {
            if (level < s_minLevel)
            {
                return;
            }

            const auto now = std::chrono::system_clock::now();
            const auto formattedMsg = std::format(fmt, std::forward<Args>(args)...);

            std::string_view levelStr = "INFO";
            std::string_view colorCode = "\033[0m";

            switch (level)
            {
                case LogLevel::Trace:
                    levelStr = "TRACE";
                    colorCode = "\033[90m";
                    break;
                case LogLevel::Debug:
                    levelStr = "DEBUG";
                    colorCode = "\033[36m";
                    break;
                case LogLevel::Info:
                    levelStr = "INFO";
                    colorCode = "\033[32m";
                    break;
                case LogLevel::Warn:
                    levelStr = "WARN";
                    colorCode = "\033[33m";
                    break;
                case LogLevel::Error:
                    levelStr = "ERROR";
                    colorCode = "\033[31m";
                    break;
                case LogLevel::Fatal:
                    levelStr = "FATAL";
                    colorCode = "\033[1;31m";
                    break;
            }

            std::string_view fullPath = loc.file_name();
            const auto lastSlash = fullPath.find_last_of("/\\");
            std::string_view fileName = (lastSlash != std::string_view::npos)
                                        ? fullPath.substr(lastSlash + 1)
                                        : fullPath;

            std::println(stdout, "{}[{:%H:%M:%S}] [{}] [{}:{}] {}\033[0m",
                         colorCode,
                         now,
                         levelStr,
                         fileName,
                         loc.line(),
                         formattedMsg);
            fflush(stdout);

            const auto debugMsg = std::format("[{:%H:%M:%S}] [{}] [{}:{}] {}\n",
                                              now, levelStr, fileName, loc.line(), formattedMsg);
            OutputDebugStringA(debugMsg.c_str());

            LogCallback cbCopy;
            {
                std::lock_guard lock(s_callbackMutex);
                cbCopy = s_callback;
            }
            if (cbCopy)
            {
                cbCopy(level, formattedMsg);
            }
        }

    private:
        inline static LogLevel s_minLevel = LogLevel::Trace;
        inline static LogCallback s_callback = nullptr;
        inline static std::mutex s_callbackMutex;
    };
}

#define RL_LOG_TRACE(fmt, ...) \
    ::RL::Logger::Log(::RL::LogLevel::Trace, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#define RL_LOG_DEBUG(fmt, ...) \
    ::RL::Logger::Log(::RL::LogLevel::Debug, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#define RL_LOG_INFO(fmt, ...) \
    ::RL::Logger::Log(::RL::LogLevel::Info, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#define RL_LOG_WARN(fmt, ...) \
    ::RL::Logger::Log(::RL::LogLevel::Warn, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#define RL_LOG_ERROR(fmt, ...) \
    ::RL::Logger::Log(::RL::LogLevel::Error, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#define RL_LOG_FATAL(fmt, ...) \
    ::RL::Logger::Log(::RL::LogLevel::Fatal, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#if defined(_DEBUG) || !defined(NDEBUG)
    #define RL_ASSERT(expr, msg) \
        do { \
            if (!(expr)) { \
                ::RL::Logger::Log(::RL::LogLevel::Fatal, std::source_location::current(), "Assertion failed: ({}) - {}", #expr, msg); \
                __debugbreak(); \
            } \
        } while (0)
#else
    #define RL_ASSERT(expr, msg) \
        do { \
            if (!(expr)) { \
                ::RL::Logger::Log(::RL::LogLevel::Fatal, std::source_location::current(), "Assertion failed: ({}) - {}", #expr, msg); \
                std::terminate(); \
            } \
        } while (0)
#endif
