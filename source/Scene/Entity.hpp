#pragma once

#include "Core/Types.hpp"
#include <compare>

namespace RL
{
    class Entity
    {
    public:
        static constexpr u64 NullID = 0xFFFFFFFFFFFFFFFFULL;

        constexpr Entity() noexcept = default;
        constexpr explicit Entity(u64 id) noexcept : m_id(id) {}
        constexpr Entity(u32 index, u32 version) noexcept
            : m_id((static_cast<u64>(version) << 32) | static_cast<u64>(index))
        {
        }

        [[nodiscard]] constexpr u32 GetIndex() const noexcept
        {
            return static_cast<u32>(m_id & 0xFFFFFFFF);
        }

        [[nodiscard]] constexpr u32 GetVersion() const noexcept
        {
            return static_cast<u32>((m_id >> 32) & 0xFFFFFFFF);
        }

        [[nodiscard]] constexpr u64 GetID() const noexcept
        {
            return m_id;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_id != NullID;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        constexpr auto operator<=>(const Entity&) const noexcept = default;

    private:
        u64 m_id = NullID;
    };
}
