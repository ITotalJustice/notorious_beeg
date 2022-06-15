// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::thumb {
namespace {

// page 142 (5.16)
auto conditional_branch(Gba& gba, const u16 opcode) -> void
{
    const auto cond = bit::get_range<8, 11>(opcode);
    s32 soffest8 = bit::get_range<0, 7>(opcode) << 1;
    soffest8 = bit::sign_extend<8>(soffest8);

    if (check_cond(gba, cond))
    {
        set_pc(gba, get_pc(gba) + soffest8);
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
