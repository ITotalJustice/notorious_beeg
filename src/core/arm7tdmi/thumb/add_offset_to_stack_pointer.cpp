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

// static_assert(bit::sign_extend<9>(0b1000000 << 2) > 0b10000000, "ye");

// page 136 (5.13)
// thumb_instruction_template
auto add_offset_to_stack_pointer(Gba& gba, uint16_t opcode) -> void
{
    #if RELEASE_BUILD == 69
    CONSTEXPR const auto S = bit_decoded_is_set(7); // 0=unsigned, 1=signed
    #else
    const auto S = bit::is_set<7>(opcode); // 0=unsigned, 1=signed
    #endif
    auto SWord7 = bit::get_range<0, 6>(opcode) << 2;

    #if RELEASE_BUILD == 69
    if CONSTEXPR (S)
    #else
    if (S)
    #endif
    {
        SWord7 *= -1;
    }

    set_sp(gba, get_sp(gba) + SWord7);
}

} // namespace gba::arm7tdmi::thumb
