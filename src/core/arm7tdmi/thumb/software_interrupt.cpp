// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <cstdint>

namespace gba::arm7tdmi::thumb {

auto software_interrupt(Gba& gba, uint16_t opcode) -> void
{
    const auto comment_field = bit::get_range<0, 7>(opcode);
    arm7tdmi::software_interrupt(gba, comment_field);
}

} // namespace gba::arm7tdmi::thumb
