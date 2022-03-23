// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

// page 146 (5.19)
// thumb_instruction_template
auto long_branch_with_link(Gba& gba, uint16_t opcode) -> void
{
    const auto H = bit::is_set<11>(opcode);
    // CONSTEXPR const auto H = bit_decoded_is_set(11);
    const auto pc = get_pc(gba);

    if (H)
    {
        const auto temp = pc - 2; // -2 because faking pipeline
        const auto OffsetLow = bit::get_range<0, 10>(opcode) << 1;
        gba_log("\tbl, pc: 0x%08X lr: 0x%08X\n", get_lr(gba) + OffsetLow, temp | 1);
        set_pc(gba, get_lr(gba) + OffsetLow);
        set_lr(gba, temp | 1);
    }
    else
    {
        auto OffsetHigh = bit::get_range<0, 10>(opcode) << 12;
        OffsetHigh = bit::sign_extend<23>(OffsetHigh);
        set_lr(gba, pc + OffsetHigh);
    }
}

} // namespace gba::arm7tdmi::thumb
