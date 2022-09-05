// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <tuple>

namespace gba::arm7tdmi::thumb {
namespace {

enum class move_compare_add_subtract_immediate_op
{
    MOV,
    CMP,
    ADD,
    SUB,
};

// page 115 (5.3)
template<u8 Op2>
auto move_compare_add_subtract_immediate(Gba& gba, const u16 opcode) -> void
{
    constexpr auto Op = static_cast<move_compare_add_subtract_immediate_op>(Op2);
    const auto Rd = bit::get_range<8, 10>(opcode);
    const auto Offset8 = bit::get_range<0, 7>(opcode);
    using enum move_compare_add_subtract_immediate_op;

    if constexpr(Op == MOV)
    {
        set_reg_thumb(gba, Rd, Offset8);
        set_logical_flags_without_carry<true>(gba, Offset8);
    }
    else if constexpr(Op == CMP)
    {
        std::ignore = internal_sub<true>(gba, get_reg(gba, Rd), Offset8);
    }
    else if constexpr(Op == ADD)
    {
        const auto result = internal_add<true>(gba, get_reg(gba, Rd), Offset8);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr(Op == SUB)
    {
        const auto result = internal_sub<true>(gba, get_reg(gba, Rd), Offset8);
        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
