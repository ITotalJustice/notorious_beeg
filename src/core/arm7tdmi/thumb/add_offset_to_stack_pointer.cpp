// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

// page 136 (5.13)
template<
    bool S // 0=unsigned, 1=signed
>
auto add_offset_to_stack_pointer(Gba& gba, uint16_t opcode) -> void
{
    auto SWord7 = bit::get_range<0, 6>(opcode) << 2;

    if constexpr (S)
    {
        SWord7 *= -1;
    }

    set_sp(gba, get_sp(gba) + SWord7);
}

} // namespace gba::arm7tdmi::thumb
