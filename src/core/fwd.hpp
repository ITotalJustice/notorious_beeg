// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>

namespace std
{
[[noreturn]] inline void unreachable()
{
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(__GNUC__) // GCC, Clang, ICC
    __builtin_unreachable();
#elif defined(_MSC_VER) // MSVC
    __assume(false);
#endif
}
}

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

#if SINGLE_FILE == 1
    #define STATIC_INLINE [[using gnu : always_inline, hot]] static inline
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
