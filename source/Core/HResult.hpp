#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winerror.h>

#include <string>
#include <source_location>
#include <expected>

namespace RL
{
    struct HResultError
    {
        HRESULT hr = S_OK;
        std::string expression;
        std::string message;
        std::source_location location;
    };

    [[nodiscard]] inline std::string FormatHResult(HRESULT hr)
    {
        LPSTR buffer = nullptr;
        const DWORD length = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(hr),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr
        );

        std::string result;
        if (length > 0 && buffer != nullptr)
        {
            result = std::string(buffer, length);
            LocalFree(buffer);
            while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' '))
            {
                result.pop_back();
            }
        }
        else
        {
            result = std::format("Unknown HRESULT error code: 0x{:08X}", static_cast<u32>(hr));
        }

        return result;
    }

    [[nodiscard]] inline std::expected<void, HResultError> VerifyHResult(
        HRESULT hr,
        std::string_view expression,
        std::source_location loc = std::source_location::current())
    {
        if (SUCCEEDED(hr))
        {
            return {};
        }

        HResultError err{
            .hr = hr,
            .expression = std::string(expression),
            .message = FormatHResult(hr),
            .location = loc
        };

        Logger::Log(LogLevel::Fatal, loc, "DirectX call failed: '{}' [HRESULT 0x{:08X}]: {}",
                    err.expression, static_cast<u32>(hr), err.message);

        return std::unexpected(std::move(err));
    }
}

#define RL_CHECK_HR(expr) \
    do { \
        HRESULT _hr_val = (expr); \
        auto _res = ::RL::VerifyHResult(_hr_val, #expr, std::source_location::current()); \
        if (!_res) { \
            return std::unexpected(_res.error().message); \
        } \
    } while (0)

#define RL_ASSERT_HR(expr) \
    do { \
        HRESULT _hr_val = (expr); \
        if (FAILED(_hr_val)) { \
            auto _msg = ::RL::FormatHResult(_hr_val); \
            ::RL::Logger::Log(::RL::LogLevel::Fatal, std::source_location::current(), \
                "CRITICAL DX12 ASSERTION FAILED: '{}' [0x{:08X}]: {}", #expr, static_cast<::RL::u32>(_hr_val), _msg); \
            std::terminate(); \
        } \
    } while (0)
