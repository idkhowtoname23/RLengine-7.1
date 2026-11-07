#pragma once

#include "Core/Types.hpp"
#include <array>
#include <atomic>
#include <string_view>

namespace RL::Memory
{
    enum class MemoryCategory : u32
    {
        General = 0,
        Graphics,
        Textures,
        Meshes,
        Audio,
        Physics,
        Scripts,
        ECS,
        Editor,
        COUNT
    };

    struct CategoryStats
    {
        usize allocated   = 0;
        usize peak        = 0;
        u64   allocCount  = 0;
        u64   freeCount   = 0;
    };

    struct MemorySnapshot
    {
        std::array<CategoryStats, static_cast<usize>(MemoryCategory::COUNT)> categories;
        usize totalAllocated = 0;
        usize totalPeak      = 0;
    };

    class MemoryStats
    {
    public:
        static void Track(MemoryCategory cat, usize bytes) noexcept
        {
            const usize idx = static_cast<usize>(cat);
            s_allocated[idx].fetch_add(bytes, std::memory_order_relaxed);
            s_allocCount[idx].fetch_add(1,     std::memory_order_relaxed);

            usize current = s_allocated[idx].load(std::memory_order_relaxed);
            usize peak    = s_peak[idx].load(std::memory_order_relaxed);
            while (current > peak && !s_peak[idx].compare_exchange_weak(peak, current,
                std::memory_order_relaxed)) {}
        }

        static void Untrack(MemoryCategory cat, usize bytes) noexcept
        {
            const usize idx = static_cast<usize>(cat);
            s_allocated[idx].fetch_sub(bytes, std::memory_order_relaxed);
            s_freeCount[idx].fetch_add(1,     std::memory_order_relaxed);
        }

        [[nodiscard]] static MemorySnapshot GetSnapshot() noexcept
        {
            MemorySnapshot snap;
            for (usize i = 0; i < static_cast<usize>(MemoryCategory::COUNT); ++i)
            {
                snap.categories[i] = CategoryStats{
                    .allocated  = s_allocated[i].load(std::memory_order_relaxed),
                    .peak       = s_peak[i].load(std::memory_order_relaxed),
                    .allocCount = s_allocCount[i].load(std::memory_order_relaxed),
                    .freeCount  = s_freeCount[i].load(std::memory_order_relaxed),
                };
                snap.totalAllocated += snap.categories[i].allocated;
                snap.totalPeak       = std::max(snap.totalPeak, snap.categories[i].peak);
            }
            return snap;
        }

        [[nodiscard]] static constexpr std::string_view GetCategoryName(MemoryCategory cat) noexcept
        {
            switch (cat)
            {
                case MemoryCategory::General:  return "General";
                case MemoryCategory::Graphics: return "Graphics";
                case MemoryCategory::Textures: return "Textures";
                case MemoryCategory::Meshes:   return "Meshes";
                case MemoryCategory::Audio:    return "Audio";
                case MemoryCategory::Physics:  return "Physics";
                case MemoryCategory::Scripts:  return "Scripts";
                case MemoryCategory::ECS:      return "ECS";
                case MemoryCategory::Editor:   return "Editor";
                default:                       return "Unknown";
            }
        }

    private:
        static constexpr usize kCount = static_cast<usize>(MemoryCategory::COUNT);

        inline static std::array<std::atomic<usize>, kCount> s_allocated{};
        inline static std::array<std::atomic<usize>, kCount> s_peak{};
        inline static std::array<std::atomic<u64>,   kCount> s_allocCount{};
        inline static std::array<std::atomic<u64>,   kCount> s_freeCount{};
    };

}
