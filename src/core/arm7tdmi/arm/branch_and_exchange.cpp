// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::arm {
namespace {

// page 48 (4.3)
auto branch_and_exchange(Gba& gba, const u32 opcode) -> void
{
    const auto Rn = bit::get_range<0, 3>(opcode);
    const auto addr = get_reg(gba, Rn);

    const auto new_state = static_cast<State>(addr & 1);
    change_state(gba, new_state, addr);
}

} // namespace
} // namespace gba::arm7tdmi::arm
