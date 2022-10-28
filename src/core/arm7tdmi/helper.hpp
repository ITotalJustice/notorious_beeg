// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "gba.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"

// generic enough functions used in multiple instructions.
// i am yet to decide on a better name for the file.
namespace gba::arm7tdmi {

[[nodiscard]]
constexpr auto calc_vflag(const u32 a, const u32 b, const u32 r) -> bool
{
    return (bit::is_set<31>(a) == bit::is_set<31>(b)) && (bit::is_set<31>(a) != bit::is_set<31>(r));
}

template<bool modify_flags> [[nodiscard("returns the result, don't ignore unless flag only instruction")]]
constexpr auto internal_add(Gba& gba, const u32 a, const u32 b) -> u32
{
    const u32 result = a + b;

    if constexpr(modify_flags)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = static_cast<u64>(a) + static_cast<u64>(b) > UINT32_MAX;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, b, result);
    }

    return result;
}


template<bool modify_flags> [[nodiscard("returns the result, don't ignore unless flag only instruction")]]
constexpr auto internal_adc(Gba& gba, const u32 a, const u32 b, const bool carry) -> u32
{
    const u32 result = a + b + carry;

    if constexpr(modify_flags)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = (static_cast<u64>(a) + static_cast<u64>(b) + carry) > UINT32_MAX;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, b, result);
    }

    return result;
}

template<bool modify_flags> [[nodiscard("returns the result, don't ignore unless flag only instruction")]]
constexpr auto internal_sub(Gba& gba, const u32 a, const u32 b) -> u32
{
    const u32 result = a - b;

    if constexpr(modify_flags)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = a >= b;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, ~b, result);
    }

    return result;
}

template<bool modify_flags> [[nodiscard("returns the result, don't ignore unless flag only instruction")]]
constexpr auto internal_sbc(Gba& gba, const u32 a, const u32 b, const bool carry) -> u32
{
    const u32 result = a - b - carry;

    if constexpr(modify_flags)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = a >= static_cast<u64>(b) + carry; // cast because b could overflow with carry added
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, ~b, result);
    }

    return result;
}

template<bool modify_flags>
constexpr auto set_logical_flags(Gba& gba, const u32 result, const bool carry) -> void
{
    if constexpr(modify_flags)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = carry;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
}

// same as above but doesnt modify carry
template<bool modify_flags>
constexpr auto set_logical_flags_without_carry(Gba& gba, const u32 result) -> void
{
    if constexpr(modify_flags)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
}

template<
    barrel::type shift_type, // see barrel_shifter.hpp
    bool reg_shift // 0=shift reg by imm, 1=shift reg by reg
> [[nodiscard]]
constexpr auto data_processing_reg_shift(Gba& gba, const u32 opcode, u32& oprand1, const u8 Rn) -> barrel::shift_result
{
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto old_carry = CPU.cpsr.C;
    auto reg_to_shift = get_reg(gba, Rm);

    if constexpr(reg_shift)
    {
        // Page 58
        gba.scheduler.tick(1);

        if (Rm == PC_INDEX) [[unlikely]]
        {
            reg_to_shift += 4;
        }
        if (Rn == PC_INDEX) [[unlikely]]
        {
            oprand1 += 4;
        }

        const auto Rs = bit::get_range<8, 11>(opcode);
        assert(Rs != PC_INDEX && "Rs cannot be R15 (i think)");

        const auto shift_amount = get_reg(gba, Rs) & 0xFF;
        return barrel::shift_reg<shift_type>(reg_to_shift, shift_amount, old_carry);
    }
    else
    {
        const auto shift_amount = bit::get_range<7, 11>(opcode);
        return barrel::shift_imm<shift_type>(reg_to_shift, shift_amount, old_carry);
    }
}

// Page 66 (MUL)
// Page 68 (MULL)
template<bool is_accumulate, bool is_signed>
constexpr auto get_multiply_cycles(u32 Rs_reg) -> u8
{
    u8 cycles = 4; // default

    if constexpr(is_signed)
    {
        u32 comp = 0xFFFFFFFF;

        Rs_reg >>= 8; comp >>= 8;
        if (!Rs_reg || Rs_reg == comp) // 7-31 all 0 | all 1
        {
            cycles = 1;
        }

        Rs_reg >>= 8; comp >>= 8;
        if (!Rs_reg || Rs_reg == comp) // 15-31 all 0 | all 1
        {
            cycles = 2;
        }

        Rs_reg >>= 8; comp >>= 8;
        if (!Rs_reg || Rs_reg == comp) // 23-31 all 0 | all 1
        {
            cycles = 3;
        }
    }
    else
    {
        Rs_reg >>= 8;
        if (!Rs_reg) // 7-31 all 0
        {
            cycles = 1;
        }

        Rs_reg >>= 8;
        if (!Rs_reg) // 15-31 all 0
        {
            cycles = 2;
        }

        Rs_reg >>= 8;
        if (!Rs_reg) // 23-31 all 0
        {
            cycles = 3;
        }
    }

    return cycles + is_accumulate;
}

} // namespace gba::arm7tdmi
