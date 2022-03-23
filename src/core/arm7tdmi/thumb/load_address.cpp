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

// page 134 (5.12)
thumb_instruction_template
auto load_address(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto SP = bit_decoded_is_set(11); // 0=PC, 1=SP
    CONSTEXPR const auto Rd = bit_decoded_get_range(8, 10);
    const auto word8 = bit::get_range<0, 7>(opcode) << 2; // 10-bit
    CONSTEXPR const auto src_reg = SP ? SP_INDEX : PC_INDEX;
    auto src_value = get_reg(gba, src_reg);

    if CONSTEXPR (src_reg == PC_INDEX)
    {
        // fleroviux says it's 32-bit aligned.
        // this is the only way for this to pass: https://github.com/jsmolka/gba-tests/blob/a6447c5404c8fc2898ddc51f438271f832083b7e/thumb/arithmetic.asm#L126
        src_value &= ~0x2;

        gba_log("\t[load address] storing pc: %u word8: %u total: %u\n", src_value, word8, src_value+word8);
    }

    const auto result = src_value + word8;
    set_reg_thumb(gba, Rd, result);
}

} // namespace gba::arm7tdmi::thumb
