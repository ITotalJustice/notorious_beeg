// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"

namespace gba::arm7tdmi::thumb {

// page 146 (5.19)
template<bool H>
static auto long_branch_with_link(Gba& gba, u16 opcode) -> void
{
    const auto pc = get_pc(gba);

    if constexpr(H)
    {
        const auto temp = pc - 2; // -2 because faking pipeline
        const auto OffsetLow = bit::get_range<0, 10>(opcode) << 1;
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
