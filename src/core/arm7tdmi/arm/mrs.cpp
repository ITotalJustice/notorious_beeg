// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cassert>

namespace gba::arm7tdmi::arm {

// page 61
template<bool P> // 0=cpsr, 1=spsr
static auto mrs(Gba& gba, u32 opcode) -> void
{
    const auto Rd = bit::get_range<12, 15>(opcode);

    u32 value = 0;

    if constexpr(P)
    {
        value = get_u32_from_spsr(gba);
    }
    else
    {
        value = get_u32_from_cpsr(gba);
    }

    set_reg(gba, Rd, value);
}

} // namespace gba::arm7tdmi::arm
