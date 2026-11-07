#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"

#include <expected>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RL
{
    class VFS
    {
    public:

        static void Mount(std::string_view prefix, const std::filesystem::path& realPath)
        {
            std::lock_guard lock(s_mutex);

            std::string key(prefix);
            std::replace(key.begin(), key.end(), '\\', '/');
            if (!key.ends_with('/')) key += '/';

            std::error_code ec;
            const std::filesystem::path absPath = std::filesystem::weakly_canonical(std::filesystem::absolute(realPath), ec);
            s_mounts[key] = absPath;

            RL_LOG_INFO("VFS: Mounted '{}' -> '{}'", key, absPath.string());
        }

        static void Unmount(std::string_view prefix)
        {
            std::lock_guard lock(s_mutex);
            std::string key(prefix);
            std::replace(key.begin(), key.end(), '\\', '/');
            if (!key.ends_with('/')) key += '/';
            s_mounts.erase(key);
        }

        [[nodiscard]] static bool IsInsideSandbox(const std::filesystem::path& candidate, const std::filesystem::path& base)
        {
            std::error_code ec;
            const auto normCandidate = std::filesystem::weakly_canonical(candidate, ec);
            if (ec) return false;
            const auto normBase = std::filesystem::weakly_canonical(base, ec);
            if (ec) return false;
            const auto rel = normCandidate.lexically_relative(normBase);
            if (rel.empty() || rel == ".") return true;
            const std::string relStr = rel.string();
            return !relStr.starts_with("..") && !rel.is_absolute();
        }

        [[nodiscard]] static std::optional<std::filesystem::path> Resolve(std::string_view virtualPath)
        {
            if (virtualPath.empty() || virtualPath.find('\0') != std::string_view::npos)
            {
                return std::nullopt;
            }

            std::lock_guard lock(s_mutex);

            std::string vp(virtualPath);
            std::replace(vp.begin(), vp.end(), '\\', '/');
            std::string bestPrefix;
            std::filesystem::path bestBase;

            for (const auto& [prefix, base] : s_mounts)
            {
                if (vp.starts_with(prefix) && prefix.size() > bestPrefix.size())
                {
                    bestPrefix = prefix;
                    bestBase   = base;
                }
            }

            if (bestPrefix.empty())
            {
                if (vp.find("..") != std::string::npos)
                {
                    return std::nullopt;
                }
                std::error_code ec;
                const auto raw = std::filesystem::weakly_canonical(vp, ec);
                if (ec || !std::filesystem::exists(raw)) return std::nullopt;
                if (!IsInsideSandbox(raw, std::filesystem::current_path())) return std::nullopt;
                return raw;
            }

            const std::string relativePart = vp.substr(bestPrefix.size());
            std::error_code ec;
            const std::filesystem::path candidate = std::filesystem::weakly_canonical(bestBase / relativePart, ec);
            if (ec) return std::nullopt;

            if (IsInsideSandbox(candidate, bestBase) && std::filesystem::exists(candidate))
            {
                return candidate;
            }
            return std::nullopt;
        }

        [[nodiscard]] static bool Exists(std::string_view virtualPath)
        {
            return Resolve(virtualPath).has_value();
        }

        [[nodiscard]] static Result<std::vector<byte>> ReadFile(std::string_view virtualPath)
        {
            const auto realPath = Resolve(virtualPath);
            if (!realPath || !std::filesystem::is_regular_file(*realPath))
            {
                return std::unexpected(
                    std::format("VFS::ReadFile: cannot resolve or not a regular file '{}'", virtualPath));
            }

            std::ifstream file(*realPath, std::ios::binary | std::ios::ate);
            if (!file.is_open())
            {
                return std::unexpected(
                    std::format("VFS::ReadFile: cannot open '{}'", realPath->string()));
            }

            const auto tellPos = file.tellg();
            if (tellPos < 0)
            {
                return std::unexpected(
                    std::format("VFS::ReadFile: invalid file size for '{}'", realPath->string()));
            }

            const auto size = static_cast<usize>(tellPos);
            file.seekg(0, std::ios::beg);

            std::vector<byte> buffer(size);
            if (size > 0 && !file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size)))
            {
                return std::unexpected(
                    std::format("VFS::ReadFile: read error for '{}'", realPath->string()));
            }

            return buffer;
        }

        [[nodiscard]] static Result<std::string> ReadText(std::string_view virtualPath)
        {
            const auto realPath = Resolve(virtualPath);
            if (!realPath || !std::filesystem::is_regular_file(*realPath))
            {
                return std::unexpected(
                    std::format("VFS::ReadText: cannot resolve or not a regular file '{}'", virtualPath));
            }

            std::ifstream file(*realPath);
            if (!file.is_open())
            {
                return std::unexpected(
                    std::format("VFS::ReadText: cannot open '{}'", realPath->string()));
            }

            std::string content{ std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>() };
            return content;
        }

        [[nodiscard]] static Result<void> WriteFile(std::string_view virtualPath,
                                                     const std::vector<byte>& data)
        {
            if (virtualPath.empty() || virtualPath.find('\0') != std::string_view::npos)
            {
                return std::unexpected("VFS::WriteFile: invalid virtual path");
            }

            std::lock_guard lock(s_mutex);

            std::string vp(virtualPath);
            std::replace(vp.begin(), vp.end(), '\\', '/');
            std::string bestPrefix;
            std::filesystem::path bestBase;

            for (const auto& [prefix, base] : s_mounts)
            {
                if (vp.starts_with(prefix) && prefix.size() > bestPrefix.size())
                {
                    bestPrefix = prefix;
                    bestBase   = base;
                }
            }

            if (bestPrefix.empty())
            {
                return std::unexpected(std::format("VFS::WriteFile: access denied, unmounted path '{}'", virtualPath));
            }

            const std::string relativePart = vp.substr(bestPrefix.size());
            std::error_code ec;
            const std::filesystem::path outPath = std::filesystem::weakly_canonical(bestBase / relativePart, ec);
            if (ec || !IsInsideSandbox(outPath, bestBase))
            {
                return std::unexpected(std::format("VFS::WriteFile: access denied, path escapes sandbox '{}'", virtualPath));
            }

            std::filesystem::create_directories(outPath.parent_path(), ec);
            std::ofstream file(outPath, std::ios::binary);
            if (!file.is_open())
            {
                return std::unexpected(
                    std::format("VFS::WriteFile: cannot open '{}' for write", outPath.string()));
            }

            file.write(reinterpret_cast<const char*>(data.data()),
                       static_cast<std::streamsize>(data.size()));
            return {};
        }

        [[nodiscard]] static std::vector<std::filesystem::path> ListDirectory(
            std::string_view virtualPath, bool recursive = false)
        {
            std::vector<std::filesystem::path> result;
            const auto realPath = Resolve(virtualPath);
            if (!realPath || !std::filesystem::is_directory(*realPath)) return result;

            if (recursive)
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(*realPath))
                    result.push_back(entry.path());
            }
            else
            {
                for (const auto& entry : std::filesystem::directory_iterator(*realPath))
                    result.push_back(entry.path());
            }
            return result;
        }

        static void PrintMounts()
        {
            std::lock_guard lock(s_mutex);
            RL_LOG_INFO("VFS mounts ({}):", s_mounts.size());
            for (const auto& [prefix, path] : s_mounts)
            {
                RL_LOG_INFO("  '{}' -> '{}'", prefix, path.string());
            }
        }

    private:
        inline static std::unordered_map<std::string, std::filesystem::path> s_mounts;
        inline static std::mutex                                               s_mutex;
    };

}
