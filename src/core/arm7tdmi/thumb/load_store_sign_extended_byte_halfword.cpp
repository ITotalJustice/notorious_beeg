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
auto load_store_sign_extended_byte_halfword(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto H = bit_decoded_is_set(11); // 0=STR, 1=LDR
    CONSTEXPR const auto S = bit_decoded_is_set(10); // 0=normal, 1=sign-extended
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

    if CONSTEXPR (S == 0)
    {
        if CONSTEXPR (H == 0) // STRH Rd,[Rb, Ro]
        {
            const auto value = get_reg(gba, Rd);
            mem::write16(gba, addr & ~0x1, value);
        }
        else // LDRH Rd,[Rb, Ro]
        {
            std::uint32_t result = mem::read16(gba, addr & ~0x1);
            result = std::rotr(result, (addr & 0x1) * 8);
            set_reg_thumb(gba, Rd, result);
        }
    }
    else
    {
        std::uint32_t result = 0;

        if CONSTEXPR (H == 0) // LDSB Rd,[Rb, Ro]
        {
            result = mem::read8(gba, addr);
            result = bit::sign_extend<8>(result);
        }
        else // LDSH Rd,[Rb, Ro]
        {
            // if LDSH addr is not aligned, it basically does LDSB instead
            if (addr & 1) [[unlikely]]
            {
                result = mem::read8(gba, addr);
                result = bit::sign_extend<8>(result);
            }
            else
            {
                result = mem::read16(gba, addr & ~0x1);
                result = bit::sign_extend<16>(result);
            }
        }

        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
