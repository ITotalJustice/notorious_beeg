// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

// page 111 (5.1)
template<u8 Op>
auto move_shifted_register(Gba& gba, uint16_t opcode) -> void
{
    const auto Offset5 = bit::get_range<6, 10>(opcode);
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto [result, carry] = barrel::shift_imm<Op>(get_reg(gba, Rs), Offset5, CPU.cpsr.C);

    set_logical_flags<true>(gba, result, carry);
    set_reg_thumb(gba, Rd, result);
}

} // namespace gba::arm7tdmi::thumb
