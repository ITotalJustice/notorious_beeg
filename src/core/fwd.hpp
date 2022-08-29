// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>

#ifdef EMSCRIPTEN
#include <ranges>

namespace std::ranges {

constexpr auto fill(auto& array, auto value) -> void
{
    for (auto& entry : array)
    {
        entry = value;
    }
}

constexpr auto copy(const auto& src, auto dst) -> void
{
    std::size_t i = 0;

    for (auto& entry : src)
    {
        dst[i++] = entry;
    }
}

} // namespace std::ranges

namespace std {

[[noreturn]] inline void unreachable()
{
    __builtin_unreachable();
}

} // namespace std
#endif // EMSCRIPTEN

#if 0
    #include <cstdio>
    #include <cassert>
    #define gba_log(...) if (CPU.breakpoint) { std::fprintf(stdout, __VA_ARGS__); }
    #define gba_log_err(...) if (CPU.breakpoint) { std::fprintf(stderr, __VA_ARGS__) }
    #define gba_log_fatal(...) if (CPU.breakpoint) { do { std::fprintf(stderr, __VA_ARGS__); assert(0); } while(0) }
#else
    #define gba_log(...)
    #define gba_log_err(...)
    #define gba_log_fatal(...)
#endif // gba_DEBUG

// cmake always sets this variable, however the frontend
// will not have this macro defined, so we need to define it
// to silence warnings.
#ifndef SINGLE_FILE
    #define SINGLE_FILE 0
#endif

#if SINGLE_FILE == 1
    #define STATIC_INLINE [[gnu::always_inline, gnu::hot, msvc::forceinline]] static inline
    #define STATIC static
#else
    #define STATIC_INLINE
    #define STATIC
#endif

namespace gba {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

struct Gba;

} // namespace gba
