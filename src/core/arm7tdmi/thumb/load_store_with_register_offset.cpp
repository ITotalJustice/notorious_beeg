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

// page 124 (5.7)
thumb_instruction_template
auto load_store_with_register_offset(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto L = bit_decoded_is_set(11); // 0=STR, 1=LDR
    CONSTEXPR const auto B = bit_decoded_is_set(10); // 0=word, 1=byte
    #if RELEASE_BUILD == 3
    CONSTEXPR const auto Ro = bit_decoded_get_range(6, 8);
    #else
    const auto Ro = bit::get_range<6, 8>(opcode);
    #endif
    const auto Rb = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto base = get_reg(gba, Rb);
    const auto offset = get_reg(gba, Ro);
    const auto addr = base + offset;

    if CONSTEXPR (L) // LDR
    {
        std::uint32_t result = 0;

        if CONSTEXPR (B) // byte
        {
            result = mem::read8(gba, addr);
        }
        else // word
        {
            result = mem::read32(gba, addr & ~0x3);
            result = std::rotr(result, (addr & 0x3) * 8);
        }

        set_reg_thumb(gba, Rd, result);
    }
    else // STR
    {
        const auto value = get_reg(gba, Rd);

        if CONSTEXPR (B) // byte
        {
            mem::write8(gba, addr, value);
        }
        else // word
        {
            mem::write32(gba, addr & ~0x3, value);
        }
    }
}

} // namespace gba::arm7tdmi::thumb
