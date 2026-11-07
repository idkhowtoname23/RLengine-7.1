#pragma once

#include "Core/Types.hpp"

#include <any>
#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <atomic>

namespace RL
{

    using EventToken = u64;

    namespace Internal
    {
        struct HandlerEntry
        {
            EventToken token;
            std::function<void(const std::any&)> handler;
        };

        struct EventChannel
        {
            std::vector<HandlerEntry> handlers;
            std::mutex               mtx;
            std::atomic<EventToken>  nextToken{ 1 };
        };

        inline std::unordered_map<std::type_index, std::shared_ptr<EventChannel>> s_channels;
        inline std::mutex s_channelsMtx;

        inline std::vector<std::pair<std::type_index, std::any>> s_deferredQueue;
        inline std::mutex s_deferredMtx;

        [[nodiscard]] inline std::shared_ptr<EventChannel> GetOrCreateChannel(std::type_index tid)
        {
            std::lock_guard lock(s_channelsMtx);
            auto it = s_channels.find(tid);
            if (it == s_channels.end())
            {
                auto ch = std::make_shared<EventChannel>();
                s_channels[tid] = ch;
                return ch;
            }
            return it->second;
        }
    }

    class EventBus
    {
    public:

        template <typename EventT>
        [[nodiscard]] static EventToken Subscribe(std::function<void(const EventT&)> handler)
        {
            auto channel = Internal::GetOrCreateChannel(typeid(EventT));
            std::lock_guard lock(channel->mtx);

            const EventToken tok = channel->nextToken.fetch_add(1, std::memory_order_relaxed);
            channel->handlers.push_back(Internal::HandlerEntry{
                .token   = tok,
                .handler = [h = std::move(handler)](const std::any& a) {
                    h(std::any_cast<const EventT&>(a));
                }
            });
            return tok;
        }

        template <typename EventT>
        static void Unsubscribe(EventToken token)
        {
            auto channel = Internal::GetOrCreateChannel(typeid(EventT));
            std::lock_guard lock(channel->mtx);
            auto& v = channel->handlers;
            v.erase(std::remove_if(v.begin(), v.end(),
                [token](const Internal::HandlerEntry& e) { return e.token == token; }),
                v.end());
        }

        template <typename EventT>
        static void Publish(const EventT& event)
        {
            auto channel = Internal::GetOrCreateChannel(typeid(EventT));
            std::vector<Internal::HandlerEntry> handlersCopy;
            {
                std::lock_guard lock(channel->mtx);
                handlersCopy = channel->handlers;
            }
            const std::any wrapped = event;
            for (auto& entry : handlersCopy)
            {
                entry.handler(wrapped);
            }
        }

        template <typename EventT>
        static void Enqueue(EventT event)
        {
            std::lock_guard lock(Internal::s_deferredMtx);
            Internal::s_deferredQueue.emplace_back(typeid(EventT), std::move(event));
        }

        static void FlushDeferred()
        {

            std::vector<std::pair<std::type_index, std::any>> local;
            {
                std::lock_guard lock(Internal::s_deferredMtx);
                local.swap(Internal::s_deferredQueue);
            }

            for (auto& [tid, wrapped] : local)
            {
                std::shared_ptr<Internal::EventChannel> channel;
                {
                    std::lock_guard chanLock(Internal::s_channelsMtx);
                    auto it = Internal::s_channels.find(tid);
                    if (it != Internal::s_channels.end())
                    {
                        channel = it->second;
                    }
                }
                if (!channel) continue;

                std::vector<Internal::HandlerEntry> handlersCopy;
                {
                    std::lock_guard hLock(channel->mtx);
                    handlersCopy = channel->handlers;
                }
                for (auto& entry : handlersCopy)
                {
                    entry.handler(wrapped);
                }
            }
        }

        static void DispatchDeferred()
        {
            FlushDeferred();
        }

        template <typename EventT>
        static void ClearAll()
        {
            auto channel = Internal::GetOrCreateChannel(typeid(EventT));
            std::lock_guard lock(channel->mtx);
            channel->handlers.clear();
        }
    };

}
