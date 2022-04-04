// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>

namespace gba::arm7tdmi::thumb {

// page 126 (5.8)
template<
    bool L // 0=STR, 1=LDR
>
static auto load_store_halfword(Gba& gba, u16 opcode) -> void
{
    const auto Rb = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);
    const auto offset = bit::get_range<6, 10>(opcode) << 1; // 6bit

    const auto base = get_reg(gba, Rb);
    const auto addr = base + offset;

    if constexpr(L == 0) // STRH Rd,[Rb, #Imm]
    {
        const auto value = get_reg(gba, Rd);
        mem::write16(gba, addr, value);
    }
    else // LDRH Rd,[Rb, #Imm]
    {
        u32 result = mem::read16(gba, addr);
        result = std::rotr(result, (addr & 0x1) * 8);
        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
