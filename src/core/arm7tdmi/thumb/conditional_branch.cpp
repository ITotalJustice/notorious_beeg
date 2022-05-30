// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "fwd.hpp"
#include "gba.hpp"
#include "mem.hpp"

namespace gba::arm7tdmi::thumb {
namespace {

// page 142 (5.16)
auto conditional_branch(Gba& gba, u16 opcode) -> void
{
    // CONSTEXPR const auto cond = bit_decoded_get_range(8, 11);
    const auto cond = bit::get_range<8, 11>(opcode);
    s32 soffest8 = bit::get_range<0, 7>(opcode) << 1;
    soffest8 = bit::sign_extend<9>(soffest8);

    if (check_cond(gba, cond))
    {
        gba_log("took branch r0: 0x%08X r1: 0x%08X cond: %u\n", get_reg(gba, 0), get_reg(gba, 1), cond);
        set_pc(gba, get_pc(gba) + soffest8);
    }
    else
    {
        gba_log("didn't take the branch r0: 0x%08X r1: 0x%08X\n", get_reg(gba, 0), get_reg(gba, 1));
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
