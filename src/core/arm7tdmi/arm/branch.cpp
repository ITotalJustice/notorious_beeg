// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cstdio>

namespace gba::arm7tdmi::arm {

// [4.4]
auto branch(Gba& gba, uint32_t opcode) -> void
{
    #if 1
    const auto link = bit::is_set<24>(opcode);
    int32_t offset = bit::get_range<0, 23>(opcode) << 2;
    offset = bit::sign_extend<26>(offset);
    const auto pc = get_pc(gba);
    #else
    const auto link = bit::is_set<24>(opcode);
    // int32_t offset = bit::get_range<0, 23>(opcode) << 2;
    const int offset = bit::sign_extend<26>(opcode << 2);
    const auto pc = get_pc(gba);
    #endif

    if (link)
    {
        // save the PC (next instruction) to LR
        gba_log("did branch with link: 0x%08X offset: %d\n", pc - 4, offset);
        set_lr(gba, pc - 4);
    }
    else
    {
        gba_log("did branch offset: %d\n", offset);
    }

    set_pc(gba, pc + offset);
}

} // namespace gba::arm7tdmi::arm
