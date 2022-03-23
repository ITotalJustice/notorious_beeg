// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

enum class move_compare_add_subtract_immediate_op
{
    MOV,
    CMP,
    ADD,
    SUB,
};

// page 115 (5.3)
thumb_instruction_template
auto move_compare_add_subtract_immediate(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto Op = bit_decoded_get_range(11, 12);
    CONSTEXPR const auto Rd = bit_decoded_get_range(8, 10);
    const auto Offset8 = bit::get_range<0, 7>(opcode);

    if CONSTEXPR (static_cast<move_compare_add_subtract_immediate_op>(Op) == move_compare_add_subtract_immediate_op::MOV)
    {
        set_reg_thumb(gba, Rd, Offset8);
        set_logical_flags2<true>(gba, Offset8);
    }

    else if CONSTEXPR(static_cast<move_compare_add_subtract_immediate_op>(Op) == move_compare_add_subtract_immediate_op::CMP)
    {
        internal_add<true>(gba, get_reg(gba, Rd), ~Offset8 + 1);
    }

    else if CONSTEXPR(static_cast<move_compare_add_subtract_immediate_op>(Op) == move_compare_add_subtract_immediate_op::ADD)
    {
        const auto result = internal_add<true>(gba, get_reg(gba, Rd), Offset8);
        set_reg_thumb(gba, Rd, result);
    }

    else if CONSTEXPR(static_cast<move_compare_add_subtract_immediate_op>(Op) == move_compare_add_subtract_immediate_op::SUB)
    {
        const auto result = internal_sub<true>(gba, get_reg(gba, Rd), Offset8);
        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
