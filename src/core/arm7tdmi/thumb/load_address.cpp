// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"

namespace gba::arm7tdmi::thumb {

// page 134 (5.12)
template<
    bool SP // 0=PC, 1=SP
>
static auto load_address(Gba& gba, u16 opcode) -> void
{
    constexpr const auto src_reg = SP ? SP_INDEX : PC_INDEX;
    const auto Rd = bit::get_range<8, 10>(opcode);
    const auto word8 = bit::get_range<0, 7>(opcode) << 2; // 10-bit
    auto src_value = get_reg(gba, src_reg);

    if constexpr(src_reg == PC_INDEX)
    {
        // fleroviux says it's 32-bit aligned.
        // this is the only way for this to pass: https://github.com/jsmolka/gba-tests/blob/a6447c5404c8fc2898ddc51f438271f832083b7e/thumb/arithmetic.asm#L126
        src_value &= ~0x2;
    }

    const auto result = src_value + word8;
    set_reg_thumb(gba, Rd, result);
}

} // namespace gba::arm7tdmi::thumb
