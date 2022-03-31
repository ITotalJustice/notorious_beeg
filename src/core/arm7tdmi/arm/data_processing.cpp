// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/barrel_shifter.hpp"
#include "arm7tdmi/helper.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cassert>
#include <cstdint>

namespace gba::arm7tdmi::arm {

enum class data_processing_op
{
    AND = 0b0000,
    EOR = 0b0001,
    SUB = 0b0010,
    RSB = 0b0011,
    ADD = 0b0100,
    ADC = 0b0101,
    SBC = 0b0110,
    RSC = 0b0111,
    TST = 0b1000,
    TEQ = 0b1001,
    CMP = 0b1010,
    CMN = 0b1011,
    ORR = 0b1100,
    MOV = 0b1101,
    BIC = 0b1110,
    MVN = 0b1111,
};

// page 52 [4.5]
template<
    bool S, // 0=no flags, 1=set flags
    u8 Op2
>
auto data_processing(Gba& gba, u32 opcode, u32 oprand1, u32 oprand2, bool new_carry) -> void
{
    constexpr auto Op = static_cast<data_processing_op>(Op2);
    const auto Rd = bit::get_range<12, 15>(opcode);

    using enum data_processing_op;

    if constexpr(Op == AND)
    {
        const auto result = oprand1 & oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags<S>(gba, result, new_carry);
    }
    else if constexpr(Op == EOR)
    {
        const auto result = oprand1 ^ oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags<S>(gba, result, new_carry);
    }
    else if constexpr(Op == SUB)
    {
        const auto result = internal_sub<S>(gba, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, result);
    }
    else if constexpr(Op == RSB)
    {
        const auto result = internal_sub<S>(gba, oprand2, oprand1);
        set_reg_data_processing(gba, Rd, result);
    }
    else if constexpr(Op == ADD)
    {
        const auto result = internal_add<S>(gba, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, result);
    }
    else if constexpr(Op == ADC)
    {
        const auto result = internal_adc<S>(gba, oprand1, oprand2, CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    }
    else if constexpr(Op == SBC)
    {
        const auto result = internal_sbc<S>(gba, oprand1, oprand2, !CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    }
    else if constexpr(Op == RSC)
    {
        const auto result = internal_sbc<S>(gba, oprand2, oprand1, !CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    }
    else if constexpr(Op == TST)
    {
        assert(S && "S bit not set in TST");
        const auto result = oprand1 & oprand2;
        set_logical_flags<S>(gba, result, new_carry);
    }
    else if constexpr(Op == TEQ)
    {
        assert(S && "S bit not set in TEQ");
        const auto result = oprand1 ^ oprand2;
        set_logical_flags<S>(gba, result, new_carry);
    }
    else if constexpr(Op == CMP)
    {
        assert(S && "S bit not set in CMP");
        internal_sub<S>(gba, oprand1, oprand2);
    }
    else if constexpr(Op == CMN)
    {
        assert(S && "S bit not set in CMN");
        internal_add<S>(gba, oprand1, oprand2);
    }
    else if constexpr(Op == ORR)
    {
        const auto result = oprand1 | oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags<S>(gba, result, new_carry);
    }
    else if constexpr(Op == MOV)
    {
        set_reg_data_processing(gba, Rd, oprand2);
        set_logical_flags<S>(gba, oprand2, new_carry);
    }
    else if constexpr(Op == BIC)
    {
        const auto result = oprand1 & ~oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags<S>(gba, result, new_carry);
    }
    else if constexpr(Op == MVN)
    {
        set_reg_data_processing(gba, Rd, ~oprand2);
        set_logical_flags<S>(gba, ~oprand2, new_carry);
    }

    if (Rd == PC_INDEX)
    {
        // todo: dont update flags if rd=15 and s is set
        if constexpr(S)
        {
            load_spsr_into_cpsr(gba);
        }

        // do not flush pipeline if r15 was not actually written to
        if constexpr(Op2 < 8 || Op2 > 11)
        {
            refill_pipeline(gba);
        }
    }
}

template<
    bool S, // 0=no flags, 1=set flags
    u8 Op
>
auto data_processing_imm(Gba& gba, u32 opcode) -> void
{
    const auto Rn = bit::get_range<16, 19>(opcode);
    const auto oprand1 = get_reg(gba, Rn);
    const auto imm = bit::get_range<0, 7>(opcode);
    const auto rotate = bit::get_range<8, 11>(opcode) * 2;
    const auto [oprand2, new_carry] = barrel::shift<barrel::ror>(imm, rotate, CPU.cpsr.C);

    data_processing<S, Op>(gba, opcode, oprand1, oprand2, new_carry);
}

template<
    bool S, // 0=no flags, 1=set flags
    u8 Op,
    u8 shift_type, // see barrel_shifter.hpp
    bool reg_shift // 0=shift reg by imm, 1=shift reg by reg
>
auto data_processing_reg(Gba& gba, u32 opcode) -> void
{
    const auto Rn = bit::get_range<16, 19>(opcode);
    auto oprand1 = get_reg(gba, Rn);
    const auto [oprand2, new_carry] = shift_thing2<shift_type, reg_shift>(gba, opcode, oprand1, Rn);

    data_processing<S, Op>(gba, opcode, oprand1, oprand2, new_carry);
}

} // namespace gba::arm7tdmi::arm
