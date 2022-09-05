// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cassert>

namespace gba::arm7tdmi::arm {
namespace {

// [4.10] (LDRH/STRH/LDRSB/LDRSH) page 76
template<
    bool P, // 0=post, 1=pre
    bool U, // 0=down, 1=up
    bool W, // 0=non, 1=write-back addr to base reg
    bool L, // 0=store, 1=load
    bool S, // 0=unsigned, 1=signed
    bool H  // 0=byte, 1=halfword
>
auto halfword_data_transfer(Gba& gba, const u32 opcode, const u32 offset) -> void
{
    const auto Rd = bit::get_range<12, 15>(opcode);
    const auto Rn = bit::get_range<16, 19>(opcode);

    auto addr = get_reg(gba, Rn);
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

        // if set, 16-bit transfer, else, 8-bit
        if constexpr(H)
        {
            result = mem::read16(gba, addr);
            // the result is actually rotated if not word alligned!!!
            if constexpr(S)
            {
                // actually loads a byte instead
                if (addr & 1) [[unlikely]]
                {
                    result = mem::read8(gba, addr);
                    result = bit::sign_extend<7>(result);
                }
                else
                {
                    result = bit::sign_extend<15>(result);
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
            if constexpr(S)
            {
                result = bit::sign_extend<7>(result);
            }
        }

        set_reg(gba, Rd, result);
    }
    else
    {
        const auto value = get_reg(gba, Rd);

        // if set, 16-bit transfer, else, 8-bit
        if constexpr(H)
        {
            mem::write16(gba, addr, value);
        }
        else
        {
            mem::write8(gba, addr, value);
        }
    }

    // if set, write back the final_addr to rn
    // if p is unset (post) then
    if ((W || !P) && (!L || Rd != Rn))
    {
        set_reg(gba, Rn, final_addr);
    }
}

template<bool P, bool U, bool W, bool L, bool S, bool H>
auto halfword_data_transfer_register_offset(Gba& gba, const u32 opcode) -> void
{
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto offset = get_reg(gba, Rm);
    assert(Rm != PC_INDEX);
    halfword_data_transfer<P, U, W, L, S, H>(gba, opcode, offset);
}

template<bool P, bool U, bool W, bool L, bool S, bool H>
auto halfword_data_transfer_immediate_offset(Gba& gba, const u32 opcode) -> void
{
    const auto lo = bit::get_range<0, 3>(opcode);
    const auto hi = bit::get_range<8, 11>(opcode);
    const auto offset = (hi << 4) | lo;
    halfword_data_transfer<P, U, W, L, S, H>(gba, opcode, offset);
}

} // namespace
} // namespace gba::arm7tdmi::arm
