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
thumb_instruction_template
auto move_shifted_register(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto Op = bit_decoded_get_range(11, 12);
    #if RELEASE_BUILD == 3
    CONSTEXPR const auto Offset5 = bit_decoded_get_range(6, 10);
    #else
    const auto Offset5 = bit::get_range<6, 10>(opcode);
    #endif
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    #if RELEASE_BUILD == 2 || RELEASE_BUILD == 3
    const auto [result, carry] = barrel::shift_imm<Op>(get_reg(gba, Rs), Offset5, CPU.cpsr.C);
    #else
    const auto [result, carry] = barrel::shift_imm(Op, get_reg(gba, Rs), Offset5, CPU.cpsr.C);
    #endif

    set_logical_flags<true>(gba, result, carry);
    set_reg_thumb(gba, Rd, result);
}

} // namespace gba::arm7tdmi::thumb
