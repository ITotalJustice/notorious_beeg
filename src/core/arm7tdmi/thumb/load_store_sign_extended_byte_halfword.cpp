// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>

namespace gba::arm7tdmi::thumb {
namespace {

// page 126 (5.8)
template<
    bool H, // 0=STR, 1=LDR
    bool S  // 0=normal, 1=sign-extended
>
auto load_store_sign_extended_byte_halfword(Gba& gba, const u16 opcode) -> void
{
    const auto Ro = bit::get_range<6, 8>(opcode);
    const auto Rb = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto base = get_reg(gba, Rb);
    const auto offset = get_reg(gba, Ro);
    const auto addr = base + offset;

    if constexpr(S == 0)
    {
        if constexpr(H == 0) // STRH Rd,[Rb, Ro]
        {
            const auto value = get_reg(gba, Rd);
            mem::write16(gba, addr, value);
        }
        else // LDRH Rd,[Rb, Ro]
        {
            u32 result = mem::read16(gba, addr);
            result = std::rotr(result, (addr & 0x1) * 8);
            set_reg_thumb(gba, Rd, result);
        }
    }
    else
    {
        u32 result = 0;

        if constexpr(H == 0) // LDSB Rd,[Rb, Ro]
        {
            result = mem::read8(gba, addr);
            result = bit::sign_extend<7>(result);
        }
        else // LDSH Rd,[Rb, Ro]
        {
            // if LDSH addr is not aligned, it does LDSB instead
            if (addr & 1) [[unlikely]]
            {
                result = mem::read8(gba, addr);
                result = bit::sign_extend<7>(result);
            }
            else
            {
                result = mem::read16(gba, addr);
                result = bit::sign_extend<15>(result);
            }
        }

        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
