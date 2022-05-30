// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::arm {
namespace {

// [4.4]
template<bool L> // 0=branch, 1=branch with link
auto branch(Gba& gba, u32 opcode) -> void
{
    s32 offset = bit::get_range<0, 23>(opcode) << 2;
    offset = bit::sign_extend<26>(offset);
    const auto pc = get_pc(gba);

    if constexpr(L) // with link
    {
        // save the PC (next instruction) to LR
        set_lr(gba, pc - 4);
    }

    set_pc(gba, pc + offset);
}

} // namespace
} // namespace gba::arm7tdmi::arm
