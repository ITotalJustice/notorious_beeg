// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "bit.hpp"
#include "fwd.hpp"

namespace gba::arm7tdmi::barrel {

enum class type : u8
{
    lsl, lsr, asr, ror
};

struct [[nodiscard]] shift_result
{
    u32 result;
    bool carry;
};

// All shifts are mask 31 (&31) as to not cause UB when shifting by more
// bits than possible.
[[nodiscard]]
constexpr auto shift_logical_left(const u32 v, const u8 shift) -> u32
{
    return v << shift;
}

[[nodiscard]]
constexpr auto shift_logical_right(const u32 v, const u8 shift) -> u32
{
    return v >> shift;
}

[[nodiscard]]
constexpr auto shift_arithmetic_right(const u32 v, const u8 shift) -> u32
{
    // signed bit should always be preserved, so casting works...
    static_assert(
        -16 >> 1U == -8,
        "arithemtic shift is not supported for this platform!"
    );

    return static_cast<u32>(static_cast<s32>(v) >> shift);
}

[[nodiscard]]
constexpr auto shift_rotate_right(const u32 v, const u8 shift) -> u32
{
    return std::rotr(v, shift);
}

[[nodiscard]]
constexpr auto shift_rrx(const u32 v, const bool old_carry) -> shift_result
{
    // ror imm #0 is a special RRX shift, which is >> 1 with the carry being shifted in
    return { (v >> 1) | (static_cast<int>(old_carry) << 31), bit::is_set<0>(v) };
}

// sepcial case for when shifting by imm and shift_value == 0
// in this case, it behaves the same as shift_value == 32
template<type t>
constexpr auto shift_imm_lsr_asr_0(const u32 v) -> shift_result
{
    if constexpr (t == type::lsr)
    {
        return { 0, bit::is_set<31>(v) };
    }
    else if constexpr(t == type::asr)
    {
        return { bit::is_set<31>(v) ? 0xFFFFFFFF : 0, bit::is_set<31>(v) };
    }
}

template<type t> [[nodiscard]]
constexpr auto shift(const u32 v, const u8 shift_v, const bool old_carry) -> shift_result
{
    if (shift_v == 0) [[unlikely]]
    {
        return { v, old_carry };
    }

    if constexpr(t == type::lsl)
    {
        // for <= 31 && NOT == 32
        // return { shift_logical_left(v, shift_v), !!(v & (0x80'00'00'00 >> (shift_v-1))) };
        if (shift_v <= 31) [[likely]]
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
    else if constexpr(t == type::lsr)
    {
        if (shift_v <= 31) [[likely]]
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
    else if constexpr(t == type::asr)
    {
        if (shift_v <= 31) [[likely]]
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
    else if constexpr(t == type::ror)
    {
        const auto result = shift_rotate_right(v, shift_v);
        return { result, bit::is_set<31>(result) };
    }
}

template<type t> [[nodiscard]]
constexpr auto shift_imm(const u32 v, const u8 shift_v, const bool old_carry) -> shift_result
{
    // shift by zero check
    if (!shift_v) [[unlikely]]
    {
        if constexpr(t == type::lsr || t == type::asr)
        {
            return shift_imm_lsr_asr_0<t>(v);
        }
        else if constexpr(t == type::ror) // ror
        {
            // convert to rrx
            return shift_rrx(v, old_carry);
        }
    }

    return shift<t>(v, shift_v, old_carry);
}

template<type t> [[nodiscard]]
constexpr auto shift_reg(const u32 v, const u8 shift_v, const bool old_carry) -> shift_result
{
    return shift<t>(v, shift_v, old_carry);
}

} // namespace gba::arm7tdmi::barrel
