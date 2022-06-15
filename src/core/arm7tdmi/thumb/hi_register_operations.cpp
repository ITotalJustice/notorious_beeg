// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <tuple>
#include <utility>

namespace gba::arm7tdmi::thumb {
namespace {

enum class hi_register_operations_op : u8
{
    ADD,
    CMP,
    MOV,
    BX,
};

// this can be further templated is desired so that it's
// known when regs are in range 0-7.
// regs in range 0-7 can use the faster set_reg_thumb()
// however, it's likely not worth it

// page 119 (5.5)
template<u8 Op2, u8 H1, u8 H2>
auto hi_register_operations(Gba& gba, const u16 opcode) -> void
{
    static_assert(H1 == 0 || H1 == 8, "bad");
    static_assert(H2 == 0 || H2 == 8, "bad");

    constexpr auto Op = static_cast<hi_register_operations_op>(Op2);
    const auto Rs = bit::get_range<3, 5>(opcode) | H2;
    const auto Rd = bit::get_range<0, 2>(opcode) | H1;

    const auto oprand1 = get_reg(gba, Rd);
    const auto oprand2 = get_reg(gba, Rs);

    using enum hi_register_operations_op;

    // note: only CMP sets flags
    if constexpr(Op == ADD)
    {
        const auto result = internal_add<false>(gba, oprand1, oprand2);
        set_reg(gba, Rd, result);
    }
    else if constexpr(Op == CMP)
    {
        std::ignore = internal_sub<true>(gba, oprand1, oprand2);
    }
    else if constexpr(Op == MOV)
    {
        set_reg(gba, Rd, oprand2);
    }
    else if constexpr(Op == BX)
    {
        const auto new_state = static_cast<State>(oprand2 & 1);
        change_state(gba, new_state, oprand2);
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
