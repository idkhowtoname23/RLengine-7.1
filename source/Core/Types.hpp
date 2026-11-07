#ifndef _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
#endif
#pragma warning(disable: 4996 4005 4702)
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace RL {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using byte = std::byte;

using StringView = std::string_view;

template <typename T> using Span = std::span<T>;

template <typename T, typename E = std::string>
using Result = std::expected<T, E>;

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;
}
