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

// page 132 (5.11)
thumb_instruction_template
auto sp_relative_load_store(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto L = bit_decoded_is_set(11);
    CONSTEXPR const auto Rd = bit_decoded_get_range(8, 10);
    // shifted to 10-bit value (unsigned)
    const auto Word8 = bit::get_range<0, 7>(opcode) << 2;

    const auto addr = get_sp(gba) + Word8;

    if CONSTEXPR (L == 0) // STR Rd,[SP, #Imm]
    {
        const auto value = get_reg(gba, Rd);
        mem::write32(gba, addr & ~0x3, value);
    }
    else // LDR Rd,[SP, #Imm]
    {
        auto result = mem::read32(gba, addr & ~0x3);
        result = std::rotr(result, (addr & 0x3) * 8);
        set_reg(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
