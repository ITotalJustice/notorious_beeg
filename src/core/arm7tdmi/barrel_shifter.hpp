// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "bit.hpp"
#include "fwd.hpp"
#include <cassert>
#include <cstdint>

namespace gba::arm7tdmi::barrel
{

// NOTE: might make these templated args as i imagine that
// there are signed shifted values.
// however signed shifted values in c/c++ are by default arithmetic shifts
// this means the sign bit is presvered, so logicals will all be arithmetic.
// this likely isn't the desired behaviour...

// All shifts are mask 31 (&31) as to not cause UB when shifting by more
// bits than possible.
[[using gnu : always_inline]] [[nodiscard]]
constexpr uint32_t shift_logical_left(uint32_t v, uint8_t shift)
{
    return v << (shift & 31U);
}

[[using gnu : always_inline]] [[nodiscard]]
constexpr uint32_t shift_logical_right(uint32_t v, uint8_t shift)
{
    return v >> (shift & 31U);
}

[[using gnu : always_inline]] [[nodiscard]]
constexpr uint32_t shift_arithmetic_right(uint32_t v, uint8_t shift)
{
    // signed bit should always be preserved, so casting works...
    static_assert(
        -16 >> 1U == -8,
        "arithemtic shift is not supported for this platform!"
    );

    return static_cast<uint32_t>(static_cast<int32_t>(v) >> shift);//(shift & 31U));
}

[[using gnu : always_inline]] [[nodiscard]]
constexpr uint32_t shift_rotate_right(uint32_t v, uint8_t shift)
{
    return std::rotr(v, shift);
}

enum type : std::uint8_t
{
    lsl, lsr, asr, ror
};

struct [[nodiscard]] shift_result
{
    uint32_t result;
    bool carry;
};

constexpr auto shift_rrx(uint32_t v, const bool old_carry) -> shift_result
{
    // ror imm #0 is a special RRX shift, which is >> 1 with the carry being shifted in
    return { (v >> 1) | (old_carry << 31), bit::is_set(v, 0) };
}

// sepcial case for when shifting by imm and shift_value == 0
// in this case, it behaves the same as shift_value == 32
template<auto t>
constexpr auto shift_imm_lsr_asr_0(uint32_t v, const bool old_carry) -> shift_result
{
    if constexpr (static_cast<type>(t) == type::lsr)
    {
        return { 0, bit::is_set<31>(v) };
    }
    else if constexpr(static_cast<type>(t) == type::asr)
    {
        return { bit::is_set<31>(v) ? 0xFFFFFFFF : 0, bit::is_set<31>(v) };
    }
}

// here
template<auto t>
constexpr auto shift(uint32_t v, uint8_t shift_v, const bool old_carry) -> shift_result
{
    if constexpr (static_cast<type>(t) == type::lsl) {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31) [[likely]]
        {
            return { shift_logical_left(v, shift_v), bit::is_set(v, 32 - shift_v) };
        }
        else if (shift_v == 32)
        {
            return { 0, bit::is_set<0>(v) };
        }
        else
        {
            return { 0, false };
        }
    }
    else if constexpr (static_cast<type>(t) == type::lsr)
    {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31) [[likely]]
        {
            return { shift_logical_right(v, shift_v), bit::is_set(v, shift_v - 1) };
        }
        else if (shift_v == 32)
        {
            return { 0, bit::is_set<31>(v) };
        }
        else
        {
            return { 0, false };
        }
    }
    else if constexpr(static_cast<type>(t) == type::asr)
    {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31) [[likely]]
        {
            return { shift_arithmetic_right(v, shift_v), bit::is_set(v, shift_v - 1) };
        }
        else if (shift_v == 32)
        {
            return { bit::is_set<31>(v) ? 0xFFFFFFFF : 0, bit::is_set<31>(v) };
        }
        else
        {
            return { shift_arithmetic_right(v, 31), bit::is_set<31>(v) };
        }
    }
    else if constexpr(static_cast<type>(t) == type::ror)
    {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31) [[likely]]
        {
            return { std::rotr(v, shift_v), bit::is_set(v, shift_v - 1) };
        }
        else if (shift_v == 32)
        {
            return { v, bit::is_set(v, 31) };
        }
        else
        {
            return shift<t>(v, shift_v - 32, old_carry);
        }
    }

    std::unreachable();
}

template<auto t>
constexpr auto shift_imm(uint32_t v, uint8_t shift_v, bool old_carry) -> shift_result
{
    // shift by zero check
    if (!shift_v) [[unlikely]]
    {
        if constexpr(t == type::lsr || t == type::asr)
        {
            return shift_imm_lsr_asr_0<t>(v, old_carry);
        }
        else if constexpr(t == type::ror) // ror
        {
            // convert to rrx
            return shift_rrx(v, old_carry);
        }
    }

    return shift<t>(v, shift_v, old_carry);
}

template<auto t>
constexpr auto shift_reg(uint32_t v, uint8_t shift_v, bool old_carry) -> shift_result
{
    return shift<t>(v, shift_v, old_carry);
}
// end

constexpr auto shift(type type, uint32_t v, uint8_t shift_v, const bool old_carry) -> shift_result {
    if (type == type::lsl) {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31)
        {
            return { shift_logical_left(v, shift_v), bit::is_set(v, 32 - shift_v) };
        }
        else if (shift_v == 32)
        {
            return { 0, bit::is_set<0>(v) };
        }
        else
        {
            return { 0, false };
        }
    }
    if (type == type::lsr) {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31)
        {
            return { shift_logical_right(v, shift_v), bit::is_set(v, shift_v - 1) };
        }
        else if (shift_v == 32)
        {
            return { 0, bit::is_set<31>(v) };
        }
        else
        {
            return { 0, false };
        }
    }
    if (type == type::asr) {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31)
        {
            return { shift_arithmetic_right(v, shift_v), bit::is_set(v, shift_v - 1) };
        }
        else if (shift_v == 32)
        {
            return { bit::is_set<31>(v) ? 0xFFFFFFFF : 0, bit::is_set<31>(v) };
        }
        else
        {
            return { shift_arithmetic_right(v, 31), bit::is_set<31>(v) };
        }
    }
    if (type == type::ror) {
        if (shift_v == 0)
        {
            return { v, old_carry };
        }
        else if (shift_v <= 31)
        {
            return { std::rotr(v, shift_v), bit::is_set(v, shift_v - 1) };
        }
        else if (shift_v == 32)
        {
            return { v, bit::is_set(v, 31) };
        }
        else
        {
            return shift(type, v, shift_v - 32, old_carry);
        }
    }

    std::unreachable();
}

constexpr auto shift(uint8_t shift_type, uint32_t v, uint8_t shift_v, const bool old_carry) -> shift_result {
    return shift(static_cast<type>(shift_type), v, shift_v, old_carry);
}

constexpr auto shift_imm(uint8_t shift_type, uint32_t v, uint8_t shift_v, bool old_carry) -> shift_result {
    // shift by zero check
    if (!shift_v)
    {
        if ((shift_type == 1 || shift_type == 2)) // lsr, asr
        {
            shift_v = 32;
        }
        else if (shift_type == 3) // ror
        {
            // convert to rrx
            return shift_rrx(v, old_carry);
        }
    }

    return shift(shift_type, v, shift_v, old_carry);
}

constexpr auto shift_reg(uint8_t shift_type, uint32_t v, uint8_t shift_v, bool old_carry) -> shift_result {
    return shift(shift_type, v, shift_v, old_carry);
}

} // namespace gba::arm7tdmi::barrel
