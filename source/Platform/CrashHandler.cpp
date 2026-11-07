#include "Platform/CrashHandler.hpp"
#include "Core/Logger.hpp"

#include <DbgHelp.h>
#include <filesystem>
#include <format>
#include <string>

namespace RL
{
    void CrashHandler::Initialize()
    {
        SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    }

    LONG WINAPI CrashHandler::UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionPointers)
    {
        std::error_code ec;
        std::filesystem::create_directories("logs", ec);

        SYSTEMTIME st;
        GetLocalTime(&st);

        const std::string dumpPath = std::format(
            "logs/crash_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.dmp",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond
        );

        const std::filesystem::path pathObj(dumpPath);
        HANDLE hFile = CreateFileW(
            pathObj.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = pExceptionPointers;
            mei.ClientPointers = TRUE;

            MiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                hFile,
                static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData),
                &mei,
                nullptr,
                nullptr
            );

            CloseHandle(hFile);
        }

        RL_LOG_FATAL("A fatal crash occurred! MiniDump saved to {}", dumpPath);
        Logger::Flush();

        return EXCEPTION_EXECUTE_HANDLER;
    }
}
