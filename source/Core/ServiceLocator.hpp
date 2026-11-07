#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"

#include <any>
#include <mutex>
#include <typeindex>
#include <unordered_map>

namespace RL
{
    class ServiceLocator
    {
    public:

        template <typename InterfaceT>
        static void Register(InterfaceT* service)
        {
            std::lock_guard lock(s_mutex);
            const std::type_index tid = typeid(InterfaceT);
            if (s_services.contains(tid))
            {
                RL_LOG_WARN("ServiceLocator: overwriting existing service '{}'", typeid(InterfaceT).name());
            }
            s_services[tid] = static_cast<void*>(service);
        }

        template <typename InterfaceT>
        [[nodiscard]] static InterfaceT* Get() noexcept
        {
            std::lock_guard lock(s_mutex);
            const auto it = s_services.find(typeid(InterfaceT));
            if (it == s_services.end()) return nullptr;
            return static_cast<InterfaceT*>(it->second);
        }

        template <typename InterfaceT>
        static void Unregister()
        {
            std::lock_guard lock(s_mutex);
            s_services.erase(typeid(InterfaceT));
        }

        template <typename InterfaceT>
        [[nodiscard]] static bool Has() noexcept
        {
            std::lock_guard lock(s_mutex);
            return s_services.contains(typeid(InterfaceT));
        }

        static void Clear()
        {
            std::lock_guard lock(s_mutex);
            s_services.clear();
        }

    private:
        inline static std::unordered_map<std::type_index, void*> s_services;
        inline static std::mutex                                   s_mutex;
    };

}
