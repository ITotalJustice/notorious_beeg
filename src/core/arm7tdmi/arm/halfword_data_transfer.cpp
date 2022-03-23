// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cstdint>
#include <cassert>
#include <cstdio>

namespace gba::arm7tdmi::arm {

arm_instruction_template
auto halfword_data_transfer(Gba& gba, uint32_t opcode, uint32_t offset) -> void
{
    CONSTEXPR_ARM const auto H = bit_decoded_is_set_arm(5);
    CONSTEXPR_ARM const auto S = bit_decoded_is_set_arm(6);
    CONSTEXPR_ARM const auto L = bit_decoded_is_set_arm(20);
    CONSTEXPR_ARM const auto W = bit_decoded_is_set_arm(21);
    CONSTEXPR_ARM const auto U = bit_decoded_is_set_arm(23);
    CONSTEXPR_ARM const auto P = bit_decoded_is_set_arm(24);
    const auto Rd = bit::get_range<12, 15>(opcode);
    const auto Rn = bit::get_range<16, 19>(opcode);

    // assert(Rn != PC_INDEX);
    // assert(Rd != PC_INDEX);

    gba_log("halfword: %s Rn: %u Rd: %u \n", L ? "LDR" : "STR", Rn, Rd);

    auto addr = get_reg(gba, Rn);
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

        // if set, 16-bit transfer, else, 8-bit
        if CONSTEXPR_ARM (H)
        {
            result = mem::read16(gba, addr);
            // the result is actually rotated if not word alligned!!!
            // result = std::rotr(result, (addr & 0x7) * 8);
            if CONSTEXPR_ARM (S)
            {
                // actually loads a byte instead
                if (addr & 1) [[unlikely]]
                {
                    result = mem::read8(gba, addr);
                    result = bit::sign_extend<8>(result);
                }
                else
                {
                    result = bit::sign_extend<16>(result);
                }
            }
            else
            {
                result = std::rotr(result, (addr & 0x1) * 8);
            }
        }
        else
        {
            // todo: this is wrong i think
            result = mem::read8(gba, addr);
            if CONSTEXPR_ARM (S)
            {
                result = bit::sign_extend<8>(result);
            }
        }

        set_reg(gba, Rd, result);
    }
    else
    {
        // if set, 16-bit transfer, else, 8-bit
        if CONSTEXPR_ARM (H)
        {
            // std::printf("16 bit transfer, addr: 0x%08X final_addr: 0x%08X offset: 0x%08X Rd: %u Reg: 0x%08X\n", addr, final_addr, offset, Rd, get_reg(gba, Rd));
            mem::write16(gba, addr & ~0x1, get_reg(gba, Rd));
        }
        else
        {
            mem::write8(gba, addr, get_reg(gba, Rd));
        }
    }

    // if set, write back the final_addr to rn
    // if p is unset (post) then
    if ((W || !P) && (!L || Rd != Rn))
    {
        set_reg(gba, Rn, final_addr);
    }
}

// [4.10] (LDRH/STRH/LDRSB/LDRSH)
#if RELEASE_BUILD_ARM == 1
arm_instruction_template
auto halfword_data_transfer_register_offset(Gba& gba, uint32_t opcode) -> void
{
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto offset = get_reg(gba, Rm);
    assert(Rm != PC_INDEX);
    halfword_data_transfer<decoded_opcode>(gba, opcode, offset);
}

arm_instruction_template
auto halfword_data_transfer_immediate_offset(Gba& gba, uint32_t opcode) -> void
{
    const auto lo = bit::get_range<0, 3>(opcode);
    const auto hi = bit::get_range<8, 11>(opcode);
    const auto offset = (hi << 4) | lo;
    halfword_data_transfer<decoded_opcode>(gba, opcode, offset);
}
#else
auto halfword_data_transfer_register_offset(Gba& gba, uint32_t opcode) -> void
{
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto offset = get_reg(gba, Rm);
    assert(Rm != PC_INDEX);
    halfword_data_transfer(gba, opcode, offset);
}

auto halfword_data_transfer_immediate_offset(Gba& gba, uint32_t opcode) -> void
{
    const auto lo = bit::get_range<0, 3>(opcode);
    const auto hi = bit::get_range<8, 11>(opcode);
    const auto offset = (hi << 4) | lo;
    halfword_data_transfer(gba, opcode, offset);
}
#endif

} // namespace gba::arm7tdmi::arm
