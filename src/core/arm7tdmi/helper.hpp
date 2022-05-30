// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "gba.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"

// generic enough functions used in multiple instructions.
// i am yet to decide on a better name for the file.
namespace gba::arm7tdmi {

constexpr auto calc_vflag(u32 a, u32 b, u32 r) -> bool
{
    return (bit::is_set<31>(a) == bit::is_set<31>(b)) && (bit::is_set<31>(a) != bit::is_set<31>(r));
}

template<bool S>
auto internal_add(Gba& gba, u32 a, u32 b) -> u32
{
    const u32 result = a + b;

    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = static_cast<u64>(a) + static_cast<u64>(b) > UINT32_MAX;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, b, result);
    }

    return result;
}


template<bool S>
auto internal_adc(Gba& gba, u32 a, u32 b, bool carry) -> u32
{
    const u32 result = a + b + carry;

    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = (static_cast<u64>(a) + static_cast<u64>(b) + carry) > UINT32_MAX;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, b, result);
    }

    return result;
}

template<bool S>
auto internal_sub(Gba& gba, u32 a, u32 b) -> u32
{
    const u32 result = a - b;

    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = a >= b;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, ~b, result);
    }

    return result;
}

template<bool S>
auto internal_sbc(Gba& gba, u32 a, u32 b, bool carry) -> u32
{
    const u32 result = a - b - carry;

    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = static_cast<u64>(a) >= static_cast<u64>(b) + carry;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, ~b, result);
    }

    return result;
}

template<bool S>
auto set_logical_flags(Gba& gba, u32 result, bool carry) -> void
{
    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = carry;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
}

// same as above but doesnt modify carry
template<bool S>
auto set_logical_flags2(Gba& gba, u32 result) -> void
{
    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
}

// marking this inline saves about 20KiB for some reason
template<
    u8 shift_type, // see barrel_shifter.hpp
    bool reg_shift // 0=shift reg by imm, 1=shift reg by reg
>
auto shift_thing2(Gba& gba, const u32 opcode, u32& oprand1, u8 Rn)
{
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto old_carry = CPU.cpsr.C;
    auto reg_to_shift = get_reg(gba, Rm);

    if constexpr(reg_shift)
    {
        if (Rm == PC_INDEX) [[unlikely]]
        {
            reg_to_shift += 4;
        }
        if (Rn == PC_INDEX) [[unlikely]]
        {
            oprand1 += 4;
        }
        const auto Rs = bit::get_range<8, 11>(opcode);
        assert(Rs != PC_INDEX);
        const auto shift_amount = get_reg(gba, Rs) & 0xFF;
        return barrel::shift_reg<shift_type>(reg_to_shift, shift_amount, old_carry);
    }
    else
    {
        const auto shift_amount = bit::get_range<7, 11>(opcode);
        return barrel::shift_imm<shift_type>(reg_to_shift, shift_amount, old_carry);
    }
}

} // namespace gba::arm7tdmi
