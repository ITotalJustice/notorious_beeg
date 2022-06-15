// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::thumb {
namespace {

// page 113 (5.2)
template<
    bool I, // 0=reg, 1=imm
    bool Op // 0=ADD, 1=SUB
>
auto add_subtract(Gba& gba, const u16 opcode) -> void
{
    const auto Rn = bit::get_range<6, 8>(opcode); // either reg or imm value
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto oprand1 = get_reg(gba, Rs);
    const auto oprand2 = I ? Rn : get_reg(gba, Rn);

    u32 result = 0;

    if constexpr(Op) // SUB
    {
        result = internal_sub<true>(gba, oprand1, oprand2);
    }
    else // ADD
    {
        result = internal_add<true>(gba, oprand1, oprand2);
    }

    set_reg_thumb(gba, Rd, result);
}

} // namespace
} // namespace gba::arm7tdmi::thumb
