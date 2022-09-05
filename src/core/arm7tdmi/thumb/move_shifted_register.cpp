// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"

namespace gba::arm7tdmi::thumb {
namespace {

// page 111 (5.1)
template<barrel::type Op>
auto move_shifted_register(Gba& gba, const u16 opcode) -> void
{
    const auto Offset5 = bit::get_range<6, 10>(opcode);
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);
    const auto value_to_be_shifted = get_reg(gba, Rs);

    const auto [result, carry] = barrel::shift_imm<Op>(value_to_be_shifted, Offset5, CPU.cpsr.C);

    set_logical_flags<true>(gba, result, carry);
    set_reg_thumb(gba, Rd, result);
}

} // namespace
} // namespace gba::arm7tdmi::thumb
