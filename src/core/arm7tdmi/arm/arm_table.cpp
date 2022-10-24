// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm/branch.cpp"
#include "arm7tdmi/arm/data_processing.cpp"
#include "arm7tdmi/arm/halfword_data_transfer.cpp"
#include "arm7tdmi/arm/single_data_transfer.cpp"
#include "arm7tdmi/arm/block_data_transfer.cpp"
#include "arm7tdmi/arm/multiply.cpp"
#include "arm7tdmi/arm/software_interrupt.cpp"
#include "arm7tdmi/arm/branch_and_exchange.cpp"
#include "arm7tdmi/arm/multiply_long.cpp"
#include "arm7tdmi/arm/single_data_swap.cpp"
#include "arm7tdmi/arm/msr.cpp"
#include "arm7tdmi/arm/mrs.cpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include "logger.hpp"
#include <cassert>
#include <array>

namespace gba::arm7tdmi::arm {
namespace {

enum class Instruction
{
    data_processing,
    msr,
    mrs,
    multiply,
    multiply_long,
    single_data_swap,
    branch_and_exchange,
    halfword_data_transfer_register_offset,
    halfword_data_transfer_immediate_offset,
    single_data_transfer,
    undefined,
    block_data_transfer,
    branch,
    software_interrupt,
};

[[nodiscard]]
constexpr auto decode_template(const u32 opcode)
{
    return (bit::get_range<20, 27>(opcode) << 4) | (bit::get_range<4, 7>(opcode));
}

// page 44
[[nodiscard]]
consteval auto decode(const u32 opcode) -> Instruction
{
    constexpr auto data_processing_mask_a = decode_template(0b0000'110'0000'0'0000'0000'000000000000);
    constexpr auto data_processing_mask_b = decode_template(0b0000'000'0000'0'0000'0000'000000000000);

    constexpr auto mrs_mask_a = decode_template(0b0000'11111'0'111111'0000'111111111111);
    constexpr auto mrs_mask_b = decode_template(0b0000'00010'0'001111'0000'000000000000);

    constexpr auto msr_mask_a = decode_template(0b0000'11'0'11'0'1'1'0'0'0'0'1111'000000000000);
    constexpr auto msr_mask_b = decode_template(0b0000'00'0'10'0'1'0'0'0'0'0'1111'000000000000);

    constexpr auto multiply_mask_a = decode_template(0b0000'111111'0'0'0000'0000'0000'1'11'1'0000);
    constexpr auto multiply_mask_b = decode_template(0b0000'000000'0'0'0000'0000'0000'1'00'1'0000);

    constexpr auto multiply_long_mask_a = decode_template(0b0000'1111'1'0'0'0'0000'0000'0000'1'11'1'0000);
    constexpr auto multiply_long_mask_b = decode_template(0b0000'0000'1'0'0'0'0000'0000'0000'1'00'1'0000);

    constexpr auto single_data_swap_mask_a = decode_template(0b0000'111'1'1'0'11'0000'0000'1111'1'11'1'0000);
    constexpr auto single_data_swap_mask_b = decode_template(0b0000'000'1'0'0'00'0000'0000'0000'1'00'1'0000);

    constexpr auto branch_and_exchange_mask_a = decode_template(0b0000'1111'1111'1111'1111'1111'1111'0000);
    constexpr auto branch_and_exchange_mask_b = decode_template(0b0000'0001'0010'1111'1111'1111'0001'0000);

    constexpr auto halfword_data_transfer_register_offset_mask_a = decode_template(0b0000'111'0'0'1'0'0'0000'0000'1111'1'0'0'1'0000);
    constexpr auto halfword_data_transfer_register_offset_mask_b = decode_template(0b0000'000'0'0'0'0'0'0000'0000'0000'1'0'0'1'0000);

    constexpr auto halfword_data_transfer_immediate_offset_mask_a = decode_template(0b0000'111'0'0'1'0'0'0000'0000'0000'1'0'0'1'0000);
    constexpr auto halfword_data_transfer_immediate_offset_mask_b = decode_template(0b0000'000'0'0'1'0'0'0000'0000'0000'1'0'0'1'0000);

    constexpr auto single_data_transfer_mask_a = decode_template(0b0000'11'0'0'0'0'0'0'0000'0000'000000000000);
    constexpr auto single_data_transfer_mask_b = decode_template(0b0000'01'0'0'0'0'0'0'0000'0000'000000000000);

    constexpr auto block_data_transfer_mask_a = decode_template(0b0000'1'11'0'0'0'0'0'0000'0000000000000000);
    constexpr auto block_data_transfer_mask_b = decode_template(0b0000'1'00'0'0'0'0'0'0000'0000000000000000);

    constexpr auto branch_mask_a = decode_template(0b0000'1110'000000000000000000000000);
    constexpr auto branch_mask_b = decode_template(0b0000'1010'000000000000000000000000);

    constexpr auto software_interrupt_mask_a = decode_template(0b0000'1111'000000000000000000000000);
    constexpr auto software_interrupt_mask_b = decode_template(0b0000'1111'000000000000000000000000);

    // note: the order of the [if's] is VERY important DO NOT CHANGE
    // generally, reverse order works apart from multiply conflicting with halfword

    if ((opcode & software_interrupt_mask_a) == software_interrupt_mask_b)
    {
        return Instruction::software_interrupt;
    }
    else if ((opcode & branch_mask_a) == branch_mask_b)
    {
        return Instruction::branch;
    }
    else if ((opcode & block_data_transfer_mask_a) == block_data_transfer_mask_b)
    {
        return Instruction::block_data_transfer;
    }
    else if ((opcode & single_data_transfer_mask_a) == single_data_transfer_mask_b)
    {
        return Instruction::single_data_transfer;
    }
    else if ((opcode & branch_and_exchange_mask_a) == branch_and_exchange_mask_b)
    {
        return Instruction::branch_and_exchange;
    }
    else if ((opcode & single_data_swap_mask_a) == single_data_swap_mask_b)
    {
        return Instruction::single_data_swap;
    }
    else if ((opcode & multiply_long_mask_a) == multiply_long_mask_b)
    {
        return Instruction::multiply_long;
    }
    else if ((opcode & multiply_mask_a) == multiply_mask_b)
    {
        return Instruction::multiply;
    }
    else if ((opcode & halfword_data_transfer_immediate_offset_mask_a) == halfword_data_transfer_immediate_offset_mask_b)
    {
        return Instruction::halfword_data_transfer_immediate_offset;
    }
    else if ((opcode & halfword_data_transfer_register_offset_mask_a) == halfword_data_transfer_register_offset_mask_b)
    {
        return Instruction::halfword_data_transfer_register_offset;
    }
    else if ((opcode & msr_mask_a) == msr_mask_b)
    {
        return Instruction::msr;
    }
    else if ((opcode & mrs_mask_a) == mrs_mask_b)
    {
        return Instruction::mrs;
    }
    else if ((opcode & data_processing_mask_a) == data_processing_mask_b)
    {
        return Instruction::data_processing;
    }

    return Instruction::undefined;
}

auto undefined([[maybe_unused]] Gba& gba, u32 opcode) -> void
{
    log::print_fatal(gba, log::Type::ARM, "undefined 0x%08X\n", opcode);
    assert(!"[arm] undefined");
}

template<auto b> [[nodiscard]]
consteval auto decoded_is_set(auto v)
{
    // 27-20 and 7-4
    static_assert((b <= 27 && b >= 20) || (b <= 7 && b >= 4), "invalid");
    if constexpr(b <= 27 && b >= 20)
    {
        constexpr auto new_bit = (b - 20) + 4;
        return bit::is_set<new_bit>(v);
    }
    else
    {
        constexpr auto new_bit = b - 4;
        return bit::is_set<new_bit>(v);
    }
}

template<u8 start, u8 end> [[nodiscard]]
consteval auto decoded_get_range(auto v)
{
    static_assert((start <= 27 && start >= 20) || (start <= 7 && start >= 4), "invalid");
    static_assert((end <= 27 && end >= 20) || (end <= 7 && end >= 4), "invalid");

    if constexpr(start <= 27 && start >= 20)
    {
        constexpr u8 new_start = (start - 20) + 4;
        constexpr u8 new_end = (end - 20) + 4;
        return bit::get_range<new_start, new_end>(v);
    }
    else
    {
        constexpr u8 new_start = start - 4;
        constexpr u8 new_end = end - 4;
        return bit::get_range<new_start, new_end>(v);
    }
}

template <int i, int end>
consteval auto fill_table(auto& table) -> void
{
    constexpr auto instruction = decode(i);

    switch (instruction)
    {
        case Instruction::data_processing: {
            constexpr auto I = decoded_is_set<25>(i);
            constexpr auto S = decoded_is_set<20>(i);
            constexpr auto Op = decoded_get_range<21, 24>(i);

            if constexpr(I == 0) // reg
            {
                constexpr auto shift_type = static_cast<barrel::type>(decoded_get_range<5, 6>(i));
                constexpr auto reg_shift = decoded_is_set<4>(i);
                table[i] = data_processing_reg<S, Op, shift_type, reg_shift>;
            }
            else // imm
            {
                table[i] = data_processing_imm<S, Op>;
            }
        } break;

        case Instruction::msr: {
            constexpr auto I = decoded_is_set<25>(i); // 0=reg, 1=imm
            constexpr auto P = decoded_is_set<22>(i); // 0=cpsr, 1=spsr
            table[i] = msr<I, P>;
        } break;

        case Instruction::mrs: {
            constexpr auto P = decoded_is_set<22>(i); // 0=cpsr, 1=spsr
            table[i] = mrs<P>;
        } break;

        case Instruction::multiply: {
            constexpr auto A = decoded_is_set<21>(i); // 0=mul, 1=mul and accumulate
            constexpr auto S = decoded_is_set<20>(i); // 0=no flags, 1=mod flags
            table[i] = multiply<A, S>;
        } break;

        case Instruction::multiply_long: {
            constexpr auto U = decoded_is_set<22>(i); // 0=unsigned, 1=signed
            constexpr auto A = decoded_is_set<21>(i); // 0=mull, 1=mlal and accumulate
            constexpr auto S = decoded_is_set<20>(i); // 0=no flags, 1=mod flags
            table[i] = multiply_long<U, A, S>;
        } break;

        case Instruction::single_data_swap: {
            constexpr auto B = decoded_is_set<22>(i); // 0=word, 1=byte
            table[i] = single_data_swap<B>;
        } break;

        case Instruction::branch_and_exchange: {
            table[i] = branch_and_exchange;
        } break;

        case Instruction::halfword_data_transfer_register_offset: {
            constexpr auto P = decoded_is_set<24>(i);
            constexpr auto U = decoded_is_set<23>(i);
            constexpr auto W = decoded_is_set<21>(i);
            constexpr auto L = decoded_is_set<20>(i);
            constexpr auto S = decoded_is_set<6>(i);
            constexpr auto H = decoded_is_set<5>(i);
            table[i] = halfword_data_transfer_register_offset<P, U, W, L, S, H>;
        } break;

        case Instruction::halfword_data_transfer_immediate_offset: {
            constexpr auto P = decoded_is_set<24>(i);
            constexpr auto U = decoded_is_set<23>(i);
            constexpr auto W = decoded_is_set<21>(i);
            constexpr auto L = decoded_is_set<20>(i);
            constexpr auto S = decoded_is_set<6>(i);
            constexpr auto H = decoded_is_set<5>(i);
            table[i] = halfword_data_transfer_immediate_offset<P, U, W, L, S, H>;
        } break;

        case Instruction::single_data_transfer: {
            constexpr auto I = decoded_is_set<25>(i); // 0=imm,1=reg
            constexpr auto P = decoded_is_set<24>(i); // 0=post,1=pre
            constexpr auto U = decoded_is_set<23>(i); // 0=sub,1=add
            constexpr auto L = decoded_is_set<20>(i); // 0=str,1=ldr
            constexpr auto B = decoded_is_set<22>(i); // 0=byte,1=word
            constexpr auto W = decoded_is_set<21>(i); // 0=none,1=write

            if constexpr(I == 0) // imm
            {
                table[i] = single_data_transfer_imm<P, U, L, B, W>;
            }
            else
            {
                constexpr auto shift_type = static_cast<barrel::type>(decoded_get_range<5, 6>(i));
                constexpr auto reg_shift = decoded_is_set<4>(i);
                table[i] = single_data_transfer_reg<P, U, L, B, W, shift_type, reg_shift>;
            }
        } break;

        case Instruction::undefined: {
            table[i] = undefined;
        } break;

        case Instruction::block_data_transfer: {
            constexpr auto P = decoded_is_set<24>(i);
            constexpr auto U = decoded_is_set<23>(i);
            constexpr auto S = decoded_is_set<22>(i);
            constexpr auto W = decoded_is_set<21>(i);
            constexpr auto L = decoded_is_set<20>(i); // 0=STM, 1=LDM
            table[i] = block_data_transfer<P, U, S, W, L>;
        } break;

        case Instruction::branch: {
            constexpr const auto L = decoded_is_set<24>(i);
            table[i] = branch<L>;
        } break;

        case Instruction::software_interrupt: {
            table[i] = software_interrupt;
        } break;
    }

    if constexpr(i < end)
    {
        fill_table<i + 1, end>(table);
    }
}

[[nodiscard]]
consteval auto generate_function_table()
{
    using func_type = void (*)(Gba&, u32);
    std::array<func_type, 4096> table{};
    table.fill(undefined);

    fill_table<0x0000, 0x00FF>(table);
    fill_table<0x0100, 0x01FF>(table);
    fill_table<0x0200, 0x02FF>(table);
    fill_table<0x0300, 0x03FF>(table);
    fill_table<0x0400, 0x04FF>(table);
    fill_table<0x0500, 0x05FF>(table);
    fill_table<0x0600, 0x06FF>(table);
    fill_table<0x0700, 0x07FF>(table);
    fill_table<0x0800, 0x08FF>(table);
    fill_table<0x0900, 0x09FF>(table);
    fill_table<0x0A00, 0x0AFF>(table);
    fill_table<0x0B00, 0x0BFF>(table);
    fill_table<0x0C00, 0x0CFF>(table);
    fill_table<0x0D00, 0x0DFF>(table);
    fill_table<0x0E00, 0x0EFF>(table);
    fill_table<0x0F00, 0x0FFF>(table);

    return table;
}

[[nodiscard]]
inline auto fetch(Gba& gba)
{
    const auto opcode = CPU.pipeline[0];
    CPU.pipeline[0] = CPU.pipeline[1];
    gba.cpu.registers[PC_INDEX] += 4;
    CPU.pipeline[1] = mem::read32(gba, get_pc(gba));

    return opcode;
}

} // namespace

auto execute(Gba& gba) -> void
{
    static constexpr auto func_table = generate_function_table();

    const auto opcode = fetch(gba);
    const auto cond = bit::get_range<28, 31>(opcode);

    // it's highly likely that cond == 0xE, so we optimise for that
    // before hitting the switch (slower).
    if (cond == COND_AL || check_cond(gba, cond)) [[likely]]
    {
        func_table[decode_template(opcode)](gba, opcode);
    }
}

} // namespace gba::arm7tdmi::arm
