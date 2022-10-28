// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <bit>
#include <cassert>

namespace gba::arm7tdmi::arm {
namespace {

// TODO: N+S needs to be handled here (Page 73)

// page 70 [4.9]
template<
    bool P, // 0=post,1=pre
    bool U, // 0=sub,1=add
    bool L, // 0=str,1=ldr
    bool B, // 0=word,1=byte
    bool W  // 0=none,1=write
>
auto single_data_transfer(Gba& gba, const u32 opcode, u32 addr, const u32 offset, const u8 Rn) -> void
{
    const auto Rd = bit::get_range<12, 15>(opcode);

    if (Rn == PC_INDEX && W)
    {
        assert(!"can't write back to PC");
    }

    // auto addr = get_reg(gba, Rn);
    auto final_addr = addr;

    // if set, add offset to base, else subtract
    if constexpr(U)
    {
        final_addr += offset;
    }
    else
    {
        final_addr -= offset;
    }

    // if set, add the offset before transfer
    if constexpr(P)
    {
        addr = final_addr;
    }

    // if set, it's an LDR, else, STR
    if constexpr(L)
    {
        u32 result = 0;

        // if set, 8-bit transfer, else, 32-bit
        if constexpr(B)
        {
            result = mem::read8(gba, addr);
        }
        else
        {
            result = mem::read32(gba, addr);
            // the result is actually rotated if not word alligned!!!
            result = std::rotr(result, (addr & 0x3) * 8);
        }

        set_reg(gba, Rd, result);

        // Page 73
        gba.scheduler.tick(1);
    }
    else
    {
        auto result = get_reg(gba, Rd);
        // pc is 12 ahead
        if (Rd == PC_INDEX)
        {
            result += 4;
        }

        // if set, 8-bit transfer, else, 32-bit
        if constexpr(B)
        {
            mem::write8(gba, addr, result);
        }
        else
        {
            mem::write32(gba, addr, result);
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

template<
    bool P, // 0=post,1=pre
    bool U, // 0=sub,1=add
    bool L, // 0=str,1=ldr
    bool B, // 0=word,1=byte
    bool W  // 0=none,1=write
>
auto single_data_transfer_imm(Gba& gba, const u32 opcode) -> void
{
    const auto Rn = bit::get_range<16, 19>(opcode);
    const auto addr = get_reg(gba, Rn);
    const auto offset = bit::get_range<0, 11>(opcode);
    single_data_transfer<P, U, L, B, W>(gba, opcode, addr, offset, Rn);
}

template<
    bool P, // 0=post,1=pre
    bool U, // 0=sub,1=add
    bool L, // 0=str,1=ldr
    bool B, // 0=word,1=byte
    bool W, // 0=none,1=write
    barrel::type shift_type, // see barrel_shifter.hpp
    bool reg_shift // 0=shift reg by imm, 1=shift reg by reg
>
auto single_data_transfer_reg(Gba& gba, const u32 opcode) -> void
{
    const auto Rn = bit::get_range<16, 19>(opcode);
    auto addr = get_reg(gba, Rn);

    const auto [offset, _] = data_processing_reg_shift<shift_type, reg_shift>(gba, opcode, addr, Rn);
    single_data_transfer<P, U, L, B, W>(gba, opcode, addr, offset, Rn);
}

} // namespace
} // namespace gba::arm7tdmi::arm
