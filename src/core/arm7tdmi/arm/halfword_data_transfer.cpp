// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cassert>

namespace gba::arm7tdmi::arm {

template<
    bool P,
    bool U,
    bool W,
    bool L,
    bool S,
    bool H
>
auto halfword_data_transfer(Gba& gba, u32 opcode, u32 offset) -> void
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
            if constexpr(S)
            {
                result = bit::sign_extend<8>(result);
            }
        }

        set_reg(gba, Rd, result);
    }
    else
    {
        // if set, 16-bit transfer, else, 8-bit
        if constexpr(H)
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
template<bool P, bool U, bool W, bool L, bool S, bool H>
auto halfword_data_transfer_register_offset(Gba& gba, u32 opcode) -> void
{
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto offset = get_reg(gba, Rm);
    assert(Rm != PC_INDEX);
    halfword_data_transfer<P, U, W, L, S, H>(gba, opcode, offset);
}

template<bool P, bool U, bool W, bool L, bool S, bool H>
auto halfword_data_transfer_immediate_offset(Gba& gba, u32 opcode) -> void
{
    const auto lo = bit::get_range<0, 3>(opcode);
    const auto hi = bit::get_range<8, 11>(opcode);
    const auto offset = (hi << 4) | lo;
    halfword_data_transfer<P, U, W, L, S, H>(gba, opcode, offset);
}

} // namespace gba::arm7tdmi::arm
