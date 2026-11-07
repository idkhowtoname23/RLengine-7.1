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

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace RL
{
    enum class FileChangeType : u8
    {
        Created,
        Modified,
        Deleted,
        Renamed
    };

    struct FileChangeEvent
    {
        FileChangeType        type;
        std::filesystem::path path;
        std::filesystem::path oldPath;
    };

    class FileWatcher
    {
    public:
        using ChangeCallback = std::function<void(const FileChangeEvent&)>;

        FileWatcher()  = default;
        ~FileWatcher() { Stop(); }

        void Watch(const std::filesystem::path& dir, ChangeCallback callback, bool recursive = true)
        {
            std::lock_guard lock(m_mutex);
            m_watches.push_back(WatchEntry{
                .directory = std::filesystem::absolute(dir),
                .recursive = recursive,
                .callback  = std::move(callback)
            });
        }

        void Start()
        {
            if (m_running.exchange(true)) return;
            m_thread = std::jthread([this](std::stop_token st) { RunLoop(st); });
        }

        void Stop()
        {
            if (!m_running.exchange(false)) return;
            if (m_thread.joinable())
            {
                m_thread.request_stop();
                m_thread.join();
            }
        }

        [[nodiscard]] bool IsRunning() const noexcept { return m_running.load(); }

    private:
        struct WatchEntry
        {
            std::filesystem::path directory;
            bool                  recursive = true;
            ChangeCallback        callback;

            HANDLE hDir = INVALID_HANDLE_VALUE;
        };

        void RunLoop(std::stop_token st)
        {
            constexpr DWORD kBufferSize = 65536;
            alignas(DWORD) char buffer[kBufferSize];

            {
                std::lock_guard lock(m_mutex);
                for (auto& entry : m_watches)
                {
                    entry.hDir = CreateFileW(
                        entry.directory.c_str(),
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS,
                        nullptr
                    );
                    if (entry.hDir == INVALID_HANDLE_VALUE)
                    {
                        RL_LOG_ERROR("FileWatcher: cannot open dir '{}'", entry.directory.string());
                    }
                }
            }

            while (!st.stop_requested())
            {
                std::lock_guard lock(m_mutex);
                for (auto& entry : m_watches)
                {
                    if (entry.hDir == INVALID_HANDLE_VALUE) continue;

                    DWORD bytesReturned = 0;
                    const BOOL ok = ReadDirectoryChangesW(
                        entry.hDir,
                        buffer,
                        kBufferSize,
                        entry.recursive ? TRUE : FALSE,
                        FILE_NOTIFY_CHANGE_FILE_NAME |
                        FILE_NOTIFY_CHANGE_DIR_NAME  |
                        FILE_NOTIFY_CHANGE_LAST_WRITE|
                        FILE_NOTIFY_CHANGE_SIZE,
                        &bytesReturned,
                        nullptr,
                        nullptr
                    );

                    if (!ok || bytesReturned == 0 || bytesReturned > kBufferSize) continue;

                    const char* const bufEnd = buffer + bytesReturned;
                    const char* currentPtr = buffer;

                    while (currentPtr + sizeof(FILE_NOTIFY_INFORMATION) <= bufEnd)
                    {
                        const auto* fni = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(currentPtr);
                        const char* fileNameStart = reinterpret_cast<const char*>(fni->FileName);
                        const char* fileNameEnd = fileNameStart + fni->FileNameLength;
                        if (fileNameEnd > bufEnd)
                        {
                            break;
                        }

                        const std::wstring wname(fni->FileName, fni->FileNameLength / sizeof(wchar_t));

                        FileChangeEvent evt;
                        evt.path = entry.directory / wname;

                        switch (fni->Action)
                        {
                            case FILE_ACTION_ADDED:            evt.type = FileChangeType::Created;  break;
                            case FILE_ACTION_REMOVED:          evt.type = FileChangeType::Deleted;  break;
                            case FILE_ACTION_MODIFIED:         evt.type = FileChangeType::Modified; break;
                            case FILE_ACTION_RENAMED_OLD_NAME: evt.type = FileChangeType::Renamed;
                                                               evt.oldPath = evt.path;              break;
                            case FILE_ACTION_RENAMED_NEW_NAME: evt.type = FileChangeType::Renamed;  break;
                            default: break;
                        }

                        if (entry.callback)
                        {
                            entry.callback(evt);
                        }

                        if (fni->NextEntryOffset == 0) break;
                        if (currentPtr + fni->NextEntryOffset >= bufEnd) break;
                        currentPtr += fni->NextEntryOffset;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            std::lock_guard lock(m_mutex);
            for (auto& entry : m_watches)
            {
                if (entry.hDir != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(entry.hDir);
                    entry.hDir = INVALID_HANDLE_VALUE;
                }
            }
        }

        std::vector<WatchEntry> m_watches;
        std::mutex              m_mutex;
        std::jthread            m_thread;
        std::atomic<bool>       m_running{ false };
    };

}
