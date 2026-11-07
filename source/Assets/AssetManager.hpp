#pragma once

#include "Assets/AssetHandle.hpp"
#include "Core/Logger.hpp"
#include "Core/UUID.hpp"
#include "Platform/FileSystem/VFS.hpp"
#include "Platform/FileSystem/FileWatcher.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace RL
{

    struct IAssetImporter
    {
        virtual ~IAssetImporter() = default;

        virtual std::shared_ptr<AssetBase> Import(const std::string& virtualPath,
                                                   const std::filesystem::path& realPath) = 0;

        virtual bool Handles(std::string_view extension) const = 0;
    };

    class AssetManager
    {
    public:

        static void Mount(std::string_view prefix, const std::filesystem::path& realDir)
        {
            VFS::Mount(prefix, realDir);
        }

        static void RegisterImporter(std::unique_ptr<IAssetImporter> importer)
        {
            std::lock_guard lock(s_mutex);
            s_importers.push_back(std::move(importer));
        }

        static void EnableHotReload(bool enable = true)
        {
            if (enable && !s_watcher.IsRunning())
            {
                s_watcher.Start();
            }
            else if (!enable && s_watcher.IsRunning())
            {
                s_watcher.Stop();
            }
        }

        template <typename AssetT>
        [[nodiscard]] static AssetHandle<AssetT> Load(std::string_view virtualPath)
        {
            const std::string key(virtualPath);

            {
                std::lock_guard lock(s_mutex);
                if (const auto it = s_cache.find(key); it != s_cache.end())
                {

                    auto typed = std::dynamic_pointer_cast<AssetT>(it->second);
                    if (typed) return AssetHandle<AssetT>(typed);
                }
            }

            const auto realPath = VFS::Resolve(virtualPath);
            if (!realPath)
            {
                RL_LOG_ERROR("AssetManager::Load: cannot resolve '{}'", virtualPath);
                return {};
            }

            std::shared_ptr<AssetBase> base;
            {
                std::lock_guard lock(s_mutex);
                IAssetImporter* importer = nullptr;
                for (auto& imp : s_importers)
                {
                    if (imp->Handles(realPath->extension().string()))
                    {
                        importer = imp.get();
                        break;
                    }
                }
                if (!importer)
                {
                    RL_LOG_ERROR("AssetManager::Load: no importer for '{}'", realPath->extension().string());
                    return {};
                }
                base = importer->Import(key, *realPath);
            }
            if (!base)
            {
                RL_LOG_ERROR("AssetManager::Load: import failed for '{}'", key);
                return {};
            }

            base->uuid        = UUID::Generate();
            base->virtualPath = key;
            base->absolutePath= realPath->string();
            base->isLoaded    = true;

            auto typed = std::dynamic_pointer_cast<AssetT>(base);
            if (!typed)
            {
                RL_LOG_ERROR("AssetManager::Load: type mismatch for '{}'", key);
                return {};
            }

            {
                std::lock_guard lock(s_mutex);
                s_cache[key]             = base;
                s_uuidIndex[base->uuid]  = base;
            }

            RL_LOG_DEBUG("AssetManager: loaded '{}' [{}]", key, base->uuid.ToString());
            return AssetHandle<AssetT>(typed);
        }

        static void Reload(std::string_view virtualPath)
        {
            {
                std::lock_guard lock(s_mutex);
                s_cache.erase(std::string(virtualPath));
            }
            RL_LOG_INFO("AssetManager: hot-reloaded '{}'", virtualPath);

        }

        static void Unload(std::string_view virtualPath)
        {
            std::lock_guard lock(s_mutex);
            const auto it = s_cache.find(std::string(virtualPath));
            if (it != s_cache.end())
            {
                s_uuidIndex.erase(it->second->uuid);
                s_cache.erase(it);
                RL_LOG_DEBUG("AssetManager: unloaded '{}'", virtualPath);
            }
        }

        static void Clear()
        {
            std::lock_guard lock(s_mutex);
            s_cache.clear();
            s_uuidIndex.clear();
            RL_LOG_INFO("AssetManager: all assets cleared");
        }

        static void Shutdown()
        {
            s_watcher.Stop();
            Clear();
        }

        [[nodiscard]] static bool IsLoaded(std::string_view virtualPath)
        {
            std::lock_guard lock(s_mutex);
            return s_cache.contains(std::string(virtualPath));
        }

        [[nodiscard]] static usize GetCacheSize()
        {
            std::lock_guard lock(s_mutex);
            return s_cache.size();
        }

    private:
        [[nodiscard]] static IAssetImporter* FindImporter(std::string_view ext)
        {
            std::lock_guard lock(s_mutex);
            for (auto& imp : s_importers)
            {
                if (imp->Handles(ext)) return imp.get();
            }
            return nullptr;
        }

        inline static std::unordered_map<std::string, std::shared_ptr<AssetBase>> s_cache;

        inline static std::unordered_map<UUID, std::shared_ptr<AssetBase>>        s_uuidIndex;

        inline static std::vector<std::unique_ptr<IAssetImporter>> s_importers;
        inline static std::mutex  s_mutex;
        inline static FileWatcher s_watcher;
    };

}
