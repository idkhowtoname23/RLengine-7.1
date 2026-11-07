#pragma once

#include "Core/Types.hpp"

#include <array>
#include <format>
#include <random>
#include <string>
#include <string_view>

namespace RL
{

    class UUID
    {
    public:
        constexpr UUID() noexcept = default;
        constexpr UUID(u64 hi, u64 lo) noexcept : m_hi(hi), m_lo(lo) {}

        [[nodiscard]] static UUID Generate() noexcept
        {
            UUID id;
            std::mt19937_64 rng{ std::random_device{}() };
            std::uniform_int_distribution<u64> dist;

            u64 hi = dist(rng);
            u64 lo = dist(rng);

            hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;

            lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

            id.m_hi = hi;
            id.m_lo = lo;
            return id;
        }

        [[nodiscard]] static UUID FromString(std::string_view str) noexcept
        {
            u64 hi = 0;
            u64 lo = 0;
            u32 hexCount = 0;

            for (char c : str)
            {
                u8 val = 0;
                if (c >= '0' && c <= '9') val = static_cast<u8>(c - '0');
                else if (c >= 'a' && c <= 'f') val = static_cast<u8>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val = static_cast<u8>(c - 'A' + 10);
                else if (c == '-') continue;
                else return UUID::Nil();

                if (hexCount < 16)
                {
                    hi = (hi << 4) | val;
                }
                else if (hexCount < 32)
                {
                    lo = (lo << 4) | val;
                }
                hexCount++;
            }

            if (hexCount != 32)
            {
                return UUID::Nil();
            }

            return UUID{ hi, lo };
        }

        [[nodiscard]] static constexpr UUID Nil() noexcept { return UUID{}; }

        [[nodiscard]] bool IsNil() const noexcept { return m_hi == 0 && m_lo == 0; }

        [[nodiscard]] std::string ToString() const
        {
            return std::format(
                "{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
                static_cast<u32>(m_hi >> 32),
                static_cast<u16>(m_hi >> 16),
                static_cast<u16>(m_hi),
                static_cast<u16>(m_lo >> 48),
                m_lo & 0x0000FFFFFFFFFFFFULL
            );
        }

        bool operator==(const UUID&) const noexcept = default;
        auto operator<=>(const UUID&) const noexcept = default;

        [[nodiscard]] u64 GetHigh() const noexcept { return m_hi; }
        [[nodiscard]] u64 GetLow()  const noexcept { return m_lo; }

    private:
        u64 m_hi = 0;
        u64 m_lo = 0;
    };

}

template <>
struct std::hash<RL::UUID>
{
    [[nodiscard]] std::size_t operator()(const RL::UUID& uuid) const noexcept
    {

        const std::size_t h1 = std::hash<RL::u64>{}(uuid.GetHigh());
        const std::size_t h2 = std::hash<RL::u64>{}(uuid.GetLow());
        return h1 ^ (h2 << 1);
    }
};
