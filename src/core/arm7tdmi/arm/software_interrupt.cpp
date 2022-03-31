// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::arm {

// page 91 (4.13)
auto software_interrupt(Gba& gba, u32 opcode) -> void
{
    const auto comment_field = bit::get_range<0, 23>(opcode) >> 16;
    arm7tdmi::software_interrupt(gba, comment_field);
}

} // namespace gba::arm7tdmi::arm
