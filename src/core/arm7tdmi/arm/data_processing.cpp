// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/barrel_shifter.hpp"
#include "arm7tdmi/helper.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::arm {

#if RELEASE_BUILD_ARM == 1
arm_instruction_template
auto shift_thing(Gba& gba, const uint32_t opcode, std::uint32_t& oprand1, std::uint8_t Rn)
{
    CONSTEXPR_ARM const auto reg_shift = bit_decoded_is_set_arm(4);
    CONSTEXPR_ARM const auto shift_type = bit_decoded_get_range_arm(5, 6);
    // const auto shift_value = bit::get_range<4, 11>(opcode);
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto old_carry = CPU.cpsr.C;
    auto reg_to_shift = get_reg(gba, Rm);

    if CONSTEXPR_ARM (reg_shift)
    {
        if (Rm == PC_INDEX) [[unlikely]]
        {
            reg_to_shift += 4;
        }
        if (Rn == PC_INDEX) [[unlikely]]
        {
            oprand1 += 4;
        }
        const auto Rs = bit::get_range<8, 11>(opcode);
        assert(Rs != PC_INDEX);
        const auto shift_amount = get_reg(gba, Rs) & 0xFF;
        return barrel::shift_reg<shift_type>(reg_to_shift, shift_amount, old_carry);
    }
    else
    {
        const auto shift_amount = bit::get_range<7, 11>(opcode);
        return barrel::shift_imm<shift_type>(reg_to_shift, shift_amount, old_carry);
    }
}
#else
auto shift_thing(Gba& gba, const uint32_t opcode, std::uint32_t& oprand1, std::uint8_t Rn)
{
    const auto reg_shift = bit::is_set<4>(opcode);
    const auto shift_type = bit::get_range<5, 6>(opcode);
    // const auto shift_value = bit::get_range<4, 11>(opcode);
    const auto Rm = bit::get_range<0, 3>(opcode);
    const auto old_carry = CPU.cpsr.C;
    auto reg_to_shift = get_reg(gba, Rm);

    if (reg_shift)
    {
        if (Rm == PC_INDEX)
        {
            reg_to_shift += 4;
        }
        if (Rn == PC_INDEX)
        {
            oprand1 += 4;
        }
        const auto Rs = bit::get_range<8, 11>(opcode);
        assert(Rs != PC_INDEX);
        const auto shift_amount = get_reg(gba, Rs) & 0xFF;
        return barrel::shift_reg(shift_type, reg_to_shift, shift_amount, old_carry);
    }
    else
    {
        const auto shift_amount = bit::get_range<7, 11>(opcode);
        return barrel::shift_imm(shift_type, reg_to_shift, shift_amount, old_carry);
    }
}
#endif

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

enum
{
    REGISTER,
    IMMEDIATE,
};

constexpr auto instruction_str(auto i) -> const char*
{
    switch (i)
    {
    case data_processing_op::AND: return "AND";
    case data_processing_op::EOR: return "EOR";
    case data_processing_op::SUB: return "SUB";
    case data_processing_op::RSB: return "RSB";
    case data_processing_op::ADD: return "ADD";
    case data_processing_op::ADC: return "ADC";
    case data_processing_op::SBC: return "SBC";
    case data_processing_op::RSC: return "RSC";
    case data_processing_op::TST: return "TST";
    case data_processing_op::TEQ: return "TEQ";
    case data_processing_op::CMP: return "CMP";
    case data_processing_op::CMN: return "CMN";
    case data_processing_op::ORR: return "ORR";
    case data_processing_op::MOV: return "MOV";
    case data_processing_op::BIC: return "BIC";
    case data_processing_op::MVN: return "MVN";
    }

    return "NULL";
}

// set register to the value from C/Spsr
arm_instruction_template
auto mrs(Gba& gba, uint32_t opcode) -> void
{
    CONSTEXPR_ARM const auto P = bit_decoded_is_set_arm(22);
    const auto Rd = bit::get_range<12, 15>(opcode);

    std::uint32_t value = 0;

    if CONSTEXPR_ARM (P)
    {
        value = get_u32_from_spsr(gba);
        gba_log("\tmrs r%u,spsr value: 0x%08X mode: %u\n", Rd, value, get_mode(gba));
    }
    else
    {
        value = get_u32_from_cpsr(gba);
        gba_log("\tmrs r%u,cpsr value: 0x%08X mode: %u\n", Rd, value, get_mode(gba));
    }

    set_reg(gba, Rd, value);
}

// page 61
// load register / immediate value into C/Spsr
// https://problemkaputt.de/gbatek.htm#armopcodespsrtransfermrsmsr
// gbatek documents a few extra options, such as only setting
// control flags (cpsr_c).
arm_instruction_template
auto msr(Gba& gba, uint32_t opcode) -> void
{
    CONSTEXPR_ARM const auto I = bit_decoded_is_set_arm(25); // 0=reg, 1=imm
    CONSTEXPR_ARM const auto P = bit_decoded_is_set_arm(22); // 0=cpsr, 1=spsr
    const auto F = bit::is_set<19>(opcode); // write to flags
    const auto C = bit::is_set<16>(opcode); // write to control

    const auto oprand = [&]()
    {
        if CONSTEXPR_ARM (I == REGISTER)
        {
            const auto Rm = bit::get_range<0, 3>(opcode);
            return get_reg(gba, Rm);
        }
        else
        {
            const auto imm = bit::get_range<0, 7>(opcode);
            // rotated by twice the value
            const auto rotate = bit::get_range<8, 11>(opcode) * 2;
            return std::rotr(imm, rotate);
        }
    }();

    // std::printf("%s I: %u P: %u F: %u C: %u oprand: 0x%08X\n", P ? "SPSR" : "CPSR", I, P, F, C, oprand);

    if CONSTEXPR_ARM (P)
    {
        set_spsr_from_u32(gba, oprand, F, C);
    }
    else
    {
        set_cpsr_from_u32(gba, oprand, F, C);
    }
}

constexpr auto decode_arm_fancy1(auto opcode)
{
    return (bit::get_range<20, 27>(opcode) << 4) | (bit::get_range<4, 7>(opcode));
}

// page 52 [4.5]
arm_instruction_template
auto data_processing(Gba& gba, uint32_t opcode) -> void
{
    constexpr auto mrs_mask_a = 0b0000'11111'0'111111'0000'111111111111;
    constexpr auto mrs_mask_b = 0b0000'00010'0'001111'0000'000000000000;

    constexpr auto msr_mask_a = 0b0000'11'0'11'0'1'1'0'0'0'0'1111'000000000000;
    constexpr auto msr_mask_b = 0b0000'00'0'10'0'1'0'0'0'0'0'1111'000000000000;

    #if RELEASE_BUILD_ARM == 1
    constexpr auto dec_mrs_mask_a = decode_arm_fancy1(mrs_mask_a);
    constexpr auto dec_mrs_mask_b = decode_arm_fancy1(mrs_mask_b);

    constexpr auto dec_msr_mask_a = decode_arm_fancy1(msr_mask_a);
    constexpr auto dec_msr_mask_b = decode_arm_fancy1(msr_mask_b);
    if CONSTEXPR_ARM((decoded_opcode & dec_mrs_mask_a) == dec_mrs_mask_b)
    {
        mrs<decoded_opcode>(gba, opcode);
    }
    else if CONSTEXPR_ARM((decoded_opcode & dec_msr_mask_a) == dec_msr_mask_b)
    {
        msr<decoded_opcode>(gba, opcode);
    }
    else
    {
    #else
    // have to handle mrs / psr
    if ((opcode & mrs_mask_a) == mrs_mask_b) {
        mrs(gba, opcode);
        return;
    }
    else if ((opcode & msr_mask_a) == msr_mask_b)
    {
        msr(gba, opcode);
        return;
    }
    #endif

    CONSTEXPR_ARM const auto I = bit_decoded_is_set_arm(25);
    CONSTEXPR_ARM const auto S = bit_decoded_is_set_arm(20);
    CONSTEXPR_ARM const auto Op = bit_decoded_get_range_arm(21, 24);
    const auto Rn = bit::get_range<16, 19>(opcode);
    const auto Rd = bit::get_range<12, 15>(opcode);

    auto oprand1 = get_reg(gba, Rn);

    const auto [oprand2, new_carry] = [&]()
    {
        if CONSTEXPR_ARM (I == REGISTER)
        {
            #if RELEASE_BUILD_ARM == 1
            return shift_thing<decoded_opcode>(gba, opcode, oprand1, Rn);
            #else
            return shift_thing(gba, opcode, oprand1, Rn);
            #endif
        }
        else
        {
            const auto imm = bit::get_range<0, 7>(opcode);
            // rotated by twice the value
            const auto rotate = bit::get_range<8, 11>(opcode) * 2;
            gba_log("\toprand is imm, value: 0x%08X rotate: %u\n", imm, rotate);
            // finally rotate
            return barrel::shift<barrel::ror>(imm, rotate, CPU.cpsr.C);
        }
    }();

    gba_log("[data_processing] instruction: %s Rd: %u Rn: %u oprand1: 0x%08X oprand2: 0x%08X\n", instruction_str(Op), Rd, Rn, oprand1, oprand2);

    #if RELEASE_BUILD_ARM == 1
    if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::AND)
    {
        gba_log("\t[AND] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 & oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::EOR)
    {
        gba_log("\t[EOR] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 ^ oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::SUB)
    {
        gba_log("\t[SUB] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_sub_arm(gba, S, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, result);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::RSB)
    {
        gba_log("\t[RSB] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_sub_arm(gba, S, oprand2, oprand1);
        set_reg_data_processing(gba, Rd, result);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::ADD)
    {
        gba_log("\t[ADD] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_add_arm(gba, S, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, result);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::ADC)
    {
        gba_log("\t[ADC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_addc_arm(gba, S, oprand1, oprand2, CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::SBC)
    {
        gba_log("\t[SBC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_subc_arm(gba, S, oprand1, oprand2, !CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::RSC)
    {
        gba_log("\t[RSC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_subc_arm(gba, S, oprand2, oprand1, !CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::TST)
    {
        assert(S && "S bit not set in TST");
        gba_log("\t[TST] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 & oprand2;
        set_logical_flags_arm(gba, S, result, new_carry);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::TEQ)
    {
        assert(S && "S bit not set in TEQ");
        gba_log("\t[TEQ] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 ^ oprand2;
        set_logical_flags_arm(gba, S, result, new_carry);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::CMP)
    {
        gba_log("\t[CMP] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        assert(S && "S bit not set in CMP");
        internal_sub_arm(gba, S, oprand1, oprand2);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::CMN)
    {
        assert(S && "S bit not set in CMN");
        gba_log("\t[CMN] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        internal_add_arm(gba, S, oprand1, oprand2);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::ORR)
    {
        gba_log("\t[ORR] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 | oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::MOV)
    {
        gba_log("\t[MOV] Rd: %u oprand1: 0x%08X oprand2: 0x%u\n", Rd, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, oprand2);
        set_logical_flags_arm(gba, S, oprand2, new_carry);
        gba_log("MOV result: 0x%08X\n", oprand2);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::BIC)
    {
        gba_log("\t[BIC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 & ~oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    }
    else if CONSTEXPR_ARM (static_cast<data_processing_op>(Op) == data_processing_op::MVN)
    {
        gba_log("\t[MVN] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, ~oprand2);
        set_logical_flags_arm(gba, S, ~oprand2, new_carry);
    }
    #else
    switch (static_cast<data_processing_op>(Op))
    {
    case data_processing_op::AND: {
        gba_log("\t[AND] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 & oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    } break;

    case data_processing_op::EOR: {
        gba_log("\t[EOR] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 ^ oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    } break;

    case data_processing_op::SUB: {
        gba_log("\t[SUB] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_sub_arm(gba, S, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, result);
    } break;

    case data_processing_op::RSB: {
        gba_log("\t[RSB] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_sub_arm(gba, S, oprand2, oprand1);
        set_reg_data_processing(gba, Rd, result);
    } break;

    case data_processing_op::ADD: {
        gba_log("\t[ADD] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_add_arm(gba, S, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, result);
    } break;

    case data_processing_op::ADC: {
        gba_log("\t[ADC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_addc_arm(gba, S, oprand1, oprand2, CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    } break;

    case data_processing_op::SBC: {
        gba_log("\t[SBC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_subc_arm(gba, S, oprand1, oprand2, !CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    } break;

    case data_processing_op::RSC: {
        gba_log("\t[RSC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = internal_subc_arm(gba, S, oprand2, oprand1, !CPU.cpsr.C);
        set_reg_data_processing(gba, Rd, result);
    } break;

    case data_processing_op::TST: {
        assert(S && "S bit not set in TST");
        gba_log("\t[TST] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 & oprand2;
        set_logical_flags_arm(gba, S, result, new_carry);
    } break;

    case data_processing_op::TEQ: {
        assert(S && "S bit not set in TEQ");
        gba_log("\t[TEQ] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 ^ oprand2;
        set_logical_flags_arm(gba, S, result, new_carry);
    } break;

    case data_processing_op::CMP: {
        gba_log("\t[CMP] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        assert(S && "S bit not set in CMP");
        internal_sub_arm(gba, S, oprand1, oprand2);
    } break;

    case data_processing_op::CMN: {
        assert(S && "S bit not set in CMN");
        gba_log("\t[CMN] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        internal_add_arm(gba, S, oprand1, oprand2);
    } break;

    case data_processing_op::ORR: {
        gba_log("\t[ORR] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 | oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    } break;

    case data_processing_op::MOV: {
        gba_log("\t[MOV] Rd: %u oprand1: 0x%08X oprand2: 0x%u\n", Rd, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, oprand2);
        set_logical_flags_arm(gba, S, oprand2, new_carry);
        gba_log("MOV result: 0x%08X\n", oprand2);
    } break;

    case data_processing_op::BIC: {
        gba_log("\t[BIC] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        const auto result = oprand1 & ~oprand2;
        set_reg_data_processing(gba, Rd, result);
        set_logical_flags_arm(gba, S, result, new_carry);
    } break;

    case data_processing_op::MVN: {
        gba_log("\t[MVN] Rd: %u oprand1: 0x%08X oprand2: 0x%08X\n", Rd, oprand1, oprand2);
        set_reg_data_processing(gba, Rd, ~oprand2);
        set_logical_flags_arm(gba, S, ~oprand2, new_carry);
    } break;
    }
    #endif

    if (Rd == PC_INDEX)
    {
        // todo: dont update flags if rd=15 and s is set
        if CONSTEXPR_ARM (S)
        {
            // std::printf("[data_processing] instruction: %s Rd: %u Rn: %u oprand1: 0x%08X oprand2: 0x%08X mode: %u\n", instruction_str(instruction), Rd, Rn, oprand1, oprand2, CPU.cpsr.M);
            // std::printf("cpsr: 0x%08X\n", get_u32_from_cpsr(gba));
            // std::printf("spsr: 0x%08X\n", get_u32_from_spsr(gba));
            gba_log("data processing handling mode swap\n");
            load_spsr_into_cpsr(gba);
        }

        // do not flush pipeline if r15 was not actually written to
        if CONSTEXPR_ARM (Op < 8 || Op > 11)
        {
            refill_pipeline(gba);
        }
    }

    #if RELEASE_BUILD_ARM == 1
    } // see the mrs msr constexpr, the else is to magic all this away
    #endif
}

} // namespace gba::arm7tdmi::arm
