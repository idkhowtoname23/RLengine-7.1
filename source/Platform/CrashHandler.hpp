#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace RL
{
    class CrashHandler
    {
    public:
        static void Initialize();
        static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionPointers);
    };
}

using RL::CrashHandler;
