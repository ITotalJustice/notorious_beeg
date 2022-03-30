// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

enum class alu_operations_op : u8
{
    AND,
    EOR,
    LSL,
    LSR,
    ASR,
    ADC,
    SBC,
    ROR,
    TST,
    NEG,
    CMP,
    CMN,
    ORR,
    MUL,
    BIC,
    MVN,
};

// page 146 (5.19)
template<u8 Op2>
auto alu_operations(Gba& gba, uint16_t opcode) -> void
{
    constexpr auto Op = static_cast<alu_operations_op>(Op2);
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto oprand1 = get_reg(gba, Rd);
    const auto oprand2 = get_reg(gba, Rs);

    using enum alu_operations_op;

    if constexpr (Op == AND)
    {
        const auto result = oprand1 & oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == EOR)
    {
        const auto result = oprand1 ^ oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == LSL)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::lsl>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == LSR)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::lsr>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == ASR)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::asr>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == ADC)
    {
        const auto result = internal_adc<true>(gba, oprand1, oprand2, CPU.cpsr.C);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == SBC)
    {
        const auto result = internal_sbc<true>(gba, oprand1, oprand2, !CPU.cpsr.C);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == ROR)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::ror>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == TST)
    {
        const auto result = oprand1 & oprand2;
        set_logical_flags2<true>(gba, result);
    }
    else if constexpr (Op == NEG)
    {
        const auto result = internal_sub<true>(gba, 0, oprand2);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == CMP)
    {
        internal_sub<true>(gba, oprand1, oprand2);
    }
    else if constexpr (Op == CMN)
    {
        internal_add<true>(gba, oprand1, oprand2);
    }
    else if constexpr (Op == ORR)
    {
        const auto result = oprand1 | oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == MUL)
    {
        const auto result = oprand1 * oprand2;
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<31>(result);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == BIC)
    {
        const auto result = oprand1 & ~oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if constexpr (Op == MVN)
    {
        const auto result = ~oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
