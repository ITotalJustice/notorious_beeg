// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm/instructions.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <cassert>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace gba::arm7tdmi::arm {

auto fetch(Gba& gba)
{
    const auto opcode = CPU.pipeline[0];
    CPU.pipeline[0] = CPU.pipeline[1];
    gba.cpu.registers[PC_INDEX] += 4;
    CPU.pipeline[1] = mem::read32(gba, get_pc(gba));
    CPU.opcode = opcode;

    return opcode;
}

enum class Instruction
{
    data_processing,
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

constexpr auto decode_template(auto opcode)
{
    #if RELEASE_BUILD_ARM == 1
    return (bit::get_range<20, 27>(opcode) << 4) | (bit::get_range<4, 7>(opcode));
    #else
    return opcode;
    #endif
}

constexpr auto decode(uint32_t opcode) -> Instruction
{
    constexpr auto data_processing_mask_a = decode_template(0b0000'110'0000'0'0000'0000'000000000000);
    constexpr auto data_processing_mask_b = decode_template(0b0000'000'0000'0'0000'0000'000000000000);

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
    else if ((opcode & data_processing_mask_a) == data_processing_mask_b)
    {
        return Instruction::data_processing;
    }

    assert(0 && "how");
    std::printf("OH NO\n");
    return Instruction::undefined;
}

#if RELEASE_BUILD_ARM == 1

auto undefined(Gba& gba, u32 opcode) -> void
{
    (void)gba;
    printf("[arm] undefined %08X\n", opcode);
}

template <u16 i, u16 end>
consteval auto fill_table(auto& table) -> void
{
    constexpr auto instruction = decode(i);

    switch (instruction)
    {
        case Instruction::data_processing: table[i] = data_processing<i>; break;
        case Instruction::multiply: table[i] = multiply; break;
        case Instruction::multiply_long: table[i] = multiply_long<i>; break;
        case Instruction::single_data_swap: table[i] = single_data_swap; break;
        case Instruction::branch_and_exchange: table[i] = branch_and_exchange; break;
        case Instruction::halfword_data_transfer_register_offset: table[i] = halfword_data_transfer_register_offset<i>; break;
        case Instruction::halfword_data_transfer_immediate_offset: table[i] = halfword_data_transfer_immediate_offset<i>; break;
        case Instruction::single_data_transfer: table[i] = single_data_transfer<i>; break;
        case Instruction::undefined: table[i] = undefined; break;
        case Instruction::block_data_transfer: table[i] = block_data_transfer<i>; break;
        case Instruction::branch: table[i] = branch; break;
        case Instruction::software_interrupt: table[i] = software_interrupt; break;
    }

    if constexpr (i == end)
    {
        return;
    }
    else
    {
        fill_table<i + 1, end>(table);
    };
}

using func_type = void (*)(Gba&, uint32_t);

consteval auto generate_function_table()
{
    std::array<func_type, 4096> table{};

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
};

constexpr auto func_table = generate_function_table();

auto execute(Gba& gba) -> void
{
    const auto opcode = fetch(gba);

    if (check_cond(gba, opcode >> 28))
    {
        func_table[decode_arm_fancy(opcode)](gba, opcode);
    }
}
#else
auto decode_str(Instruction i) -> const char*
{
    switch (i)
    {
        case Instruction::data_processing: return "data_processing";
        case Instruction::multiply: return "multiply";
        case Instruction::multiply_long: return "multiply_long";
        case Instruction::single_data_swap: return "single_data_swap";
        case Instruction::branch_and_exchange: return "branch_and_exchange";
        case Instruction::halfword_data_transfer_register_offset: return "halfword_data_transfer_register_offset";
        case Instruction::halfword_data_transfer_immediate_offset: return "halfword_data_transfer_immediate_offset";
        case Instruction::single_data_transfer: return "single_data_transfer";
        case Instruction::undefined: return "undefined";
        case Instruction::block_data_transfer: return "block_data_transfer";
        case Instruction::branch: return "branch";
        case Instruction::software_interrupt: return "software_interrupt";
    }

    return "NULL";
}

auto execute(Gba& gba) -> void
{
    const auto opcode = fetch(gba);
    const auto instruction = decode(opcode);

    if (instruction == Instruction::undefined)
    {
        std::printf("[ARM] PC: 0x%08X opcode: 0x%08X decoded: %s\n", get_pc(gba) - (CPU.cpsr.T ? 2 * 2 : 4 * 2), opcode, decode_str(instruction));
        print_bits<32>(opcode);
        std::printf("cpsr: 0x%08X\n", get_u32_from_cpsr(gba));
        std::printf("spsr: 0x%08X\n", get_u32_from_spsr(gba));
        assert(instruction != Instruction::undefined);
    }

    if (CPU.breakpoint)
    {
        std::printf("[ARM] PC: 0x%08X opcode: 0x%08X decoded: %s cpsr: 0x%08X spsr: 0x%08X mode: %u\n", get_pc(gba) - (CPU.cpsr.T ? 2 * 2 : 4 * 2), opcode, decode_str(instruction), get_u32_from_cpsr(gba), get_u32_from_spsr(gba), get_mode(gba));
    }

    if (check_cond(gba, opcode >> 28))
    {
        switch (instruction)
        {
            case Instruction::data_processing: data_processing(gba, opcode); break;
            case Instruction::multiply: multiply(gba, opcode); break;
            case Instruction::multiply_long: multiply_long(gba, opcode); break;
            case Instruction::single_data_swap: single_data_swap(gba, opcode); break;
            case Instruction::branch_and_exchange: branch_and_exchange(gba, opcode); break;
            case Instruction::halfword_data_transfer_register_offset: halfword_data_transfer_register_offset(gba, opcode); break;
            case Instruction::halfword_data_transfer_immediate_offset: halfword_data_transfer_immediate_offset(gba, opcode); break;
            case Instruction::single_data_transfer: single_data_transfer(gba, opcode); break;
            case Instruction::undefined: assert(!"unimpl undefined"); break;
            case Instruction::block_data_transfer: block_data_transfer(gba, opcode); break;
            case Instruction::branch: branch(gba, opcode); break;
            case Instruction::software_interrupt: software_interrupt(gba, opcode); break;
        }
    }
    else
    {
        gba_log("skipping instruction! cond: %X\n", opcode >> 28);
    }
}
#endif

} // namespace gba::arm7tdmi::arm
