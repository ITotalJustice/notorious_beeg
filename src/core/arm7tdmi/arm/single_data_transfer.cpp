// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include <cstdio>
#include <cassert>

namespace gba::arm7tdmi::arm {

namespace {
#if RELEASE_BUILD_ARM == 1
arm_instruction_template
auto shift_thing2(Gba& gba, const uint32_t opcode, std::uint32_t& oprand1, std::uint8_t Rn)
{
    CONSTEXPR_ARM const auto reg_shift = bit_decoded_is_set_arm(4);
    CONSTEXPR_ARM const auto shift_type = bit_decoded_get_range_arm(5, 6);
    // const auto shift_value = bit::get_range<4, 11>(opcode);
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto old_carry = CPU.cpsr.C;
    auto reg_to_shift = get_reg(gba, Rm);

    if CONSTEXPR_ARM (reg_shift)
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
#else
auto shift_thing2(Gba& gba, const uint32_t opcode, std::uint32_t& oprand1, std::uint8_t Rn)
{
    CONSTEXPR_ARM const auto reg_shift = bit_decoded_is_set_arm(4);
    CONSTEXPR_ARM const auto shift_type = bit_decoded_get_range_arm(5, 6);
    // const auto shift_value = bit::get_range<4, 11>(opcode);
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto old_carry = CPU.cpsr.C;
    auto reg_to_shift = get_reg(gba, Rm);

    if CONSTEXPR_ARM (reg_shift)
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
        return barrel::shift_reg(shift_type, reg_to_shift, shift_amount, old_carry);
    }
    else
    {
        const auto shift_amount = bit::get_range<7, 11>(opcode);
        return barrel::shift_imm(shift_type, reg_to_shift, shift_amount, old_carry);
    }
}
#endif
}

// page 70 [4.9]
arm_instruction_template
auto single_data_transfer(Gba& gba, uint32_t opcode) -> void
{
    CONSTEXPR_ARM const auto L = bit_decoded_is_set_arm(20); // 0=str,1=ldr
    CONSTEXPR_ARM const auto W = bit_decoded_is_set_arm(21); // 0=none,1=write
    CONSTEXPR_ARM const auto B = bit_decoded_is_set_arm(22); // 0=byte,1=word
    CONSTEXPR_ARM const auto U = bit_decoded_is_set_arm(23); // 0=sub,1=add
    CONSTEXPR_ARM const auto P = bit_decoded_is_set_arm(24); // 0=post,1=pre
    CONSTEXPR_ARM const auto I = bit_decoded_is_set_arm(25); // 0=imm,1=reg
    const auto Rd = bit::get_range<12, 15>(opcode);
    auto Rn = bit::get_range<16, 19>(opcode);
    auto addr = get_reg(gba, Rn);

    if (Rn == PC_INDEX && W)
    {
        assert(!"can't write back to PC");
    }

    const auto offset = [&]()
    {
        if CONSTEXPR_ARM (I)
        {
            #if RELEASE_BUILD_ARM == 1
            const auto [result, _] = shift_thing2<decoded_opcode>(gba, opcode, addr, Rn);
            #else
            const auto [result, _] = shift_thing2(gba, opcode, addr, Rn);
            #endif
            return result;
        }
        else
        {
            return bit::get_range<0, 11>(opcode);
        }
    }();

    // auto addr = get_reg(gba, Rn);
    auto final_addr = addr;

    // if set, add offset to base, else subtract
    if CONSTEXPR_ARM (U)
    {
        final_addr += offset;
    }
    else
    {
        final_addr -= offset;
    }

    // if set, add the offset before transfer
    if CONSTEXPR_ARM (P)
    {
        addr = final_addr;
    }

    // if set, it's an LDR, else, STR
    if CONSTEXPR_ARM (L)
    {
        uint32_t result = 0;

        // if set, 8-bit transfer, else, 32-bit
        if CONSTEXPR_ARM (B)
        {
            result = mem::read8(gba, addr);
        }
        else
        {
            result = mem::read32(gba, addr &~ 0x3);
            // the result is actually rotated if not word alligned!!!
            result = std::rotr(result, (addr & 0x3) * 8);
        }

        gba_log("single data: %s Rn: %u Rd: %u, offset: 0x%08X addr: 0x%08X final_addr: 0x%08X result: 0x%08X\n", L ? "LDR" : "STR", Rn, Rd, offset, addr, final_addr, result);

        set_reg(gba, Rd, result);
    }
    else
    {
        auto result = get_reg(gba, Rd);
        // pc is 12 ahead
        if (Rd == PC_INDEX)
        {
            result += 4;
        }

        gba_log("single data: %s Rn: %u Rd: %u, offset: 0x%08X addr: 0x%08X final_addr: 0x%08X result: 0x%08X\n", L ? "LDR" : "STR", Rn, Rd, offset, addr, final_addr, result);

        // if set, 8-bit transfer, else, 32-bit
        if CONSTEXPR_ARM (B)
        {
            mem::write8(gba, addr, result);
        }
        else
        {
            mem::write32(gba, addr & ~0x3, result);
        }
    }

    // if set, write back the final_addr to rn
    // if p is unset (post) then write back
    // NOTE: write back actually happens before written to the register
    // W = W || !P
    // STR || Rd != Rn
    if ((W || !P) && (!L || Rd != Rn))
    {
        set_reg(gba, Rn, final_addr);
    }
}

} // namespace gba::arm7tdmi::arm
