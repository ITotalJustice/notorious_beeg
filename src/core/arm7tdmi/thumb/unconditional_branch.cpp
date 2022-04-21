// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::thumb {

// page 145 (5.18)
static auto unconditional_branch(Gba& gba, u16 opcode) -> void
{
    auto offset11 = bit::get_range<0, 10>(opcode) << 1;
    offset11 = bit::sign_extend<12>(offset11);

    set_pc(gba, get_pc(gba) + offset11);
}

} // namespace gba::arm7tdmi::thumb
