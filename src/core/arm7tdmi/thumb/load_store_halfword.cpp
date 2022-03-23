// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

// page 126 (5.8)
thumb_instruction_template
auto load_store_halfword(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto L = bit_decoded_is_set(11); // 0=STR, 1=LDR

    const auto Rb = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    #if RELEASE_BUILD == 3
    CONSTEXPR const auto offset = bit_decoded_get_range(6, 10) << 1; // 6bit
    #else
    const auto offset = bit::get_range<6, 10>(opcode) << 1; // 6bit
    #endif

    const auto base = get_reg(gba, Rb);
    const auto addr = base + offset;

    if CONSTEXPR (L == 0) // STRH Rd,[Rb, #Imm]
    {
        const auto value = get_reg(gba, Rd);
        mem::write16(gba, addr & ~0x1, value);
    }
    else // LDRH Rd,[Rb, #Imm]
    {
        std::uint32_t result = mem::read16(gba, addr & ~0x1);
        result = std::rotr(result, (addr & 0x1) * 8);
        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
