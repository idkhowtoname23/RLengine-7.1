#pragma once

#include "Core/Types.hpp"

#include <string_view>
#include <string>

namespace RL
{

    class StringID
    {
    public:
        static constexpr u64 kInvalidHash = 0;

        constexpr StringID() noexcept : m_hash(kInvalidHash) {}

        constexpr explicit StringID(std::string_view sv) noexcept
            : m_hash(Hash(sv)) {}

        [[nodiscard]] constexpr u64 GetHash() const noexcept { return m_hash; }
        [[nodiscard]] constexpr bool IsValid()  const noexcept { return m_hash != kInvalidHash; }

        constexpr bool operator==(const StringID& rhs) const noexcept = default;
        constexpr auto operator<=>(const StringID& rhs) const noexcept = default;

        [[nodiscard]] static constexpr u64 Hash(std::string_view s) noexcept
        {
            constexpr u64 FNV_OFFSET = 14695981039346656037ULL;
            constexpr u64 FNV_PRIME  = 1099511628211ULL;
            u64 h = FNV_OFFSET;
            for (const char c : s)
            {
                h ^= static_cast<u64>(static_cast<unsigned char>(c));
                h *= FNV_PRIME;
            }
            return h;
        }

    private:
        u64 m_hash;
    };

    consteval StringID operator""_sid(const char* s, std::size_t len) noexcept
    {
        return StringID(std::string_view(s, len));
    }

}

template <>
struct std::hash<RL::StringID>
{
    [[nodiscard]] std::size_t operator()(const RL::StringID& id) const noexcept
    {
        return static_cast<std::size_t>(id.GetHash());
    }
};
