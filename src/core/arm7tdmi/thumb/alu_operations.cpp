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

enum class alu_operations_op
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

#if RELEASE_BUILD == 3
// page 146 (5.19)
thumb_instruction_template
auto alu_operations(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto Op = bit_decoded_get_range(6, 9);
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto oprand1 = get_reg(gba, Rd);
    const auto oprand2 = get_reg(gba, Rs);

    if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::AND)
    {
        const auto result = oprand1 & oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::EOR)
    {
        const auto result = oprand1 ^ oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::LSL)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::lsl>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::LSR)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::lsr>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::ASR)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::asr>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::ADC)
    {
        gba_log("adding: a: %u b: %u carry: %u\n", oprand1, oprand2, CPU.cpsr.C);
        const auto result = internal_add<true>(gba, oprand1, oprand2, CPU.cpsr.C);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::SBC)
    {
        const auto result = internal_sub<true>(gba, oprand1, oprand2, !CPU.cpsr.C);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::ROR)
    {
        const auto [result, carry] = barrel::shift_reg<barrel::type::ror>(oprand1, oprand2, CPU.cpsr.C);
        set_logical_flags<true>(gba, result, carry);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::TST)
    {
        const auto result = oprand1 & oprand2;
        set_logical_flags2<true>(gba, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::NEG)
    {
        const auto result = -oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::CMP)
    {
        gba_log("\tcmp r%u, r%u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, Rs, oprand1, oprand2);
        internal_sub<true>(gba, oprand1, oprand2);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::CMN)
    {
        internal_add<true>(gba, oprand1, oprand2);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::ORR)
    {
        const auto result = oprand1 | oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::MUL)
    {
        const auto result = oprand1 * oprand2;
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<31>(result);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::BIC)
    {
        const auto result = oprand1 & ~oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
    else if CONSTEXPR (static_cast<alu_operations_op>(Op) == alu_operations_op::MVN)
    {
        const auto result = ~oprand2;
        set_logical_flags2<true>(gba, result);
        set_reg_thumb(gba, Rd, result);
    }
}
#else
// page 146 (5.19)
thumb_instruction_template
auto alu_operations(Gba& gba, uint16_t opcode) -> void
{
    const auto Op = bit::get_range<6, 9>(opcode);
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto oprand1 = get_reg(gba, Rd);
    const auto oprand2 = get_reg(gba, Rs);

    switch (static_cast<alu_operations_op>(Op))
    {
        case alu_operations_op::AND:
        {
            const auto result = oprand1 & oprand2;
            set_logical_flags2<true>(gba, result);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::EOR:
        {
            const auto result = oprand1 ^ oprand2;
            set_logical_flags2<true>(gba, result);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::LSL:
        {
            const auto [result, carry] = barrel::shift_reg<barrel::type::lsl>(oprand1, oprand2, CPU.cpsr.C);
            set_logical_flags<true>(gba, result, carry);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::LSR:
        {
            const auto [result, carry] = barrel::shift_reg<barrel::type::lsr>(oprand1, oprand2, CPU.cpsr.C);
            set_logical_flags<true>(gba, result, carry);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::ASR:
        {
            const auto [result, carry] = barrel::shift_reg<barrel::type::asr>(oprand1, oprand2, CPU.cpsr.C);
            set_logical_flags<true>(gba, result, carry);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::ADC:
        {
            gba_log("adding: a: %u b: %u carry: %u\n", oprand1, oprand2, CPU.cpsr.C);
            const auto result = internal_add<true>(gba, oprand1, oprand2, CPU.cpsr.C);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::SBC:
        {
            const auto result = internal_sub<true>(gba, oprand1, oprand2, !CPU.cpsr.C);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::ROR:
        {
            const auto [result, carry] = barrel::shift_reg<barrel::type::ror>(oprand1, oprand2, CPU.cpsr.C);
            set_logical_flags<true>(gba, result, carry);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::TST:
        {
            const auto result = oprand1 & oprand2;
            set_logical_flags2<true>(gba, result);
        } break;

        case alu_operations_op::NEG:
        {
            const auto result = -oprand2;
            set_logical_flags2<true>(gba, result);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::CMP:
        {
            gba_log("\tcmp r%u, r%u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, Rs, oprand1, oprand2);
            internal_sub<true>(gba, oprand1, oprand2);
        } break;

        case alu_operations_op::CMN:
        {
            internal_add<true>(gba, oprand1, oprand2);
        } break;

        case alu_operations_op::ORR:
        {
            const auto result = oprand1 | oprand2;
            set_logical_flags2<true>(gba, result);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::MUL:
        {
            const auto result = oprand1 * oprand2;
            CPU.cpsr.Z = result == 0;
            CPU.cpsr.N = bit::is_set<31>(result);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::BIC:
        {
            const auto result = oprand1 & ~oprand2;
            set_logical_flags2<true>(gba, result);
            set_reg_thumb(gba, Rd, result);
        } break;

        case alu_operations_op::MVN:
        {
            const auto result = ~oprand2;
            set_logical_flags2<true>(gba, result);
            set_reg_thumb(gba, Rd, result);
        } break;
    }
}
#endif

} // namespace gba::arm7tdmi::thumb
