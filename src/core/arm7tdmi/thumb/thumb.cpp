// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/thumb/instructions.hpp"
#include "gba.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <array>

namespace gba::arm7tdmi::thumb {

enum class Instruction
{
    move_shifted_register,
    add_subtract,
    move_compare_add_subtract_immediate,
    alu_operations,
    hi_register_operations,
    pc_relative_load,
    load_store_with_register_offset,
    load_store_sign_extended_byte_halfword,
    load_store_with_immediate_offset,
    load_store_halfword,
    sp_relative_load_store,
    load_address,
    add_offset_to_stack_pointer,
    push_pop_registers,
    multiple_load_store,
    conditional_branch,
    software_interrupt,
    unconditional_branch,
    long_branch_with_link,
    undefined,
};

auto fetch(Gba& gba)
{
    const std::uint16_t opcode = CPU.pipeline[0];
    CPU.pipeline[0] = CPU.pipeline[1];
    gba.cpu.registers[PC_INDEX] += 2;
    CPU.pipeline[1] = mem::read16(gba, get_pc(gba));
    CPU.opcode = opcode;

    return opcode;
}

// page 108
constexpr auto decode(uint16_t opcode) -> Instruction
{
    #if RELEASE_BUILD == 2
    // 250-260
    constexpr auto shift_down = 8;
    #elif RELEASE_BUILD == 3
    // 250-260
    constexpr auto shift_down = 6;
    #else
    // 280-290
    constexpr auto shift_down = 0;
    #endif

    // 1110101000000000
    constexpr auto multiple_load_store_mask_a = 0b1111'0'000'00000000 >> shift_down;
    constexpr auto multiple_load_store_mask_b = 0b1100'0'000'00000000 >> shift_down;

    constexpr auto push_pop_registers_mask_a = 0b1111'0'11'0'00000000 >> shift_down;
    constexpr auto push_pop_registers_mask_b = 0b1011'0'10'0'00000000 >> shift_down;

    constexpr auto sp_relative_load_store_mask_a = 0b1111'0'000'00000000 >> shift_down;
    constexpr auto sp_relative_load_store_mask_b = 0b1001'0'000'00000000 >> shift_down;

    constexpr auto load_store_halfword_mask_a = 0b1111'0'00000'000'000 >> shift_down;
    constexpr auto load_store_halfword_mask_b = 0b1000'0'00000'000'000 >> shift_down;

    constexpr auto load_store_with_immediate_offset_mask_a = 0b111'0'0'00000'000'000 >> shift_down;
    constexpr auto load_store_with_immediate_offset_mask_b = 0b011'0'0'00000'000'000 >> shift_down;

    constexpr auto load_store_with_register_offset_mask_a = 0b1111'0'0'1'000'000'000 >> shift_down;
    constexpr auto load_store_with_register_offset_mask_b = 0b0101'0'0'0'000'000'000 >> shift_down;

    constexpr auto load_store_sign_extended_byte_halfword_mask_a = 0b1111'0'0'1'000'000'000 >> shift_down;
    constexpr auto load_store_sign_extended_byte_halfword_mask_b = 0b0101'0'0'1'000'000'000 >> shift_down;

    constexpr auto add_offset_to_stack_pointer_mask_a = 0b11111111'0'0000000 >> shift_down;
    constexpr auto add_offset_to_stack_pointer_mask_b = 0b10110000'0'0000000 >> shift_down;

    constexpr auto move_shifted_register_mask_a = 0b111'00'00000'000'000 >> shift_down;
    constexpr auto move_shifted_register_mask_b = 0b000'00'00000'000'000 >> shift_down;

    constexpr auto software_interrupt_mask_a = 0b1111'1111'00000000 >> shift_down;
    constexpr auto software_interrupt_mask_b = 0b1101'1111'00000000 >> shift_down;

    constexpr auto conditional_branch_mask_a = 0b1111'0000'00000000 >> shift_down;
    constexpr auto conditional_branch_mask_b = 0b1101'0000'00000000 >> shift_down;

    constexpr auto unconditional_branch_mask_a = 0b11111'00000000000 >> shift_down;
    constexpr auto unconditional_branch_mask_b = 0b11100'00000000000 >> shift_down;

    constexpr auto long_branch_with_link_mask_a = 0b1111000000000000 >> shift_down;
    constexpr auto long_branch_with_link_mask_b = 0b1111000000000000 >> shift_down;

    constexpr auto hi_register_operations_mask_a = 0b111111'00'0'0'000'000 >> shift_down;
    constexpr auto hi_register_operations_mask_b = 0b010001'00'0'0'000'000 >> shift_down;

    constexpr auto move_compare_add_subtract_immediate_mask_a = 0b111'00'000'00000000 >> shift_down;
    constexpr auto move_compare_add_subtract_immediate_mask_b = 0b001'00'000'00000000 >> shift_down;

    constexpr auto add_subtract_mask_a = 0b11111'0'0'000'000'000 >> shift_down;
    constexpr auto add_subtract_mask_b = 0b00011'0'0'000'000'000 >> shift_down;

    constexpr auto alu_operations_mask_a = 0b111111'0000'000'000 >> shift_down;
    constexpr auto alu_operations_mask_b = 0b010000'0000'000'000 >> shift_down;

    constexpr auto pc_relative_load_mask_a = 0b11111'000'00000000 >> shift_down;
    constexpr auto pc_relative_load_mask_b = 0b01001'000'00000000 >> shift_down;

    constexpr auto load_address_mask_a = 0b1111'0'000'00000000 >> shift_down;
    constexpr auto load_address_mask_b = 0b1010'0'000'00000000 >> shift_down;

    if ((opcode & add_offset_to_stack_pointer_mask_a) == add_offset_to_stack_pointer_mask_b)
    {
        return Instruction::add_offset_to_stack_pointer;
    }

    else if ((opcode & multiple_load_store_mask_a) == multiple_load_store_mask_b)
    {
        return Instruction::multiple_load_store;
    }

    else if ((opcode & push_pop_registers_mask_a) == push_pop_registers_mask_b)
    {
        return Instruction::push_pop_registers;
    }

    else if ((opcode & sp_relative_load_store_mask_a) == sp_relative_load_store_mask_b)
    {
        return Instruction::sp_relative_load_store;
    }

    else if ((opcode & load_store_halfword_mask_a) == load_store_halfword_mask_b)
    {
        return Instruction::load_store_halfword;
    }

    else if ((opcode & load_store_with_immediate_offset_mask_a) == load_store_with_immediate_offset_mask_b)
    {
        return Instruction::load_store_with_immediate_offset;
    }

    else if ((opcode & load_store_with_register_offset_mask_a) == load_store_with_register_offset_mask_b)
    {
        return Instruction::load_store_with_register_offset;
    }

    else if ((opcode & load_store_sign_extended_byte_halfword_mask_a) == load_store_sign_extended_byte_halfword_mask_b)
    {
        return Instruction::load_store_sign_extended_byte_halfword;
    }

    else if ((opcode & software_interrupt_mask_a) == software_interrupt_mask_b)
    {
        return Instruction::software_interrupt;
    }

    else if ((opcode & conditional_branch_mask_a) == conditional_branch_mask_b)
    {
        return Instruction::conditional_branch;
    }

    else if ((opcode & unconditional_branch_mask_a) == unconditional_branch_mask_b)
    {
        return Instruction::unconditional_branch;
    }

    else if ((opcode & long_branch_with_link_mask_a) == long_branch_with_link_mask_b)
    {
        return Instruction::long_branch_with_link;
    }

    else if ((opcode & hi_register_operations_mask_a) == hi_register_operations_mask_b)
    {
        return Instruction::hi_register_operations;
    }

    else if ((opcode & move_compare_add_subtract_immediate_mask_a) == move_compare_add_subtract_immediate_mask_b)
    {
        return Instruction::move_compare_add_subtract_immediate;
    }

    else if ((opcode & add_subtract_mask_a) == add_subtract_mask_b)
    {
        return Instruction::add_subtract;
    }

    else if ((opcode & move_shifted_register_mask_a) == move_shifted_register_mask_b)
    {
        return Instruction::move_shifted_register;
    }

    else if ((opcode & alu_operations_mask_a) == alu_operations_mask_b)
    {
        return Instruction::alu_operations;
    }

    else if ((opcode & pc_relative_load_mask_a) == pc_relative_load_mask_b)
    {
        return Instruction::pc_relative_load;
    }

    else if ((opcode & load_address_mask_a) == load_address_mask_b)
    {
        return Instruction::load_address;
    }

    return Instruction::undefined;
}

constexpr auto decode_str(Instruction i) -> const char*
{
    switch (i)
    {
        case Instruction::move_shifted_register: return "move_shifted_register";
        case Instruction::add_subtract: return "add_subtract";
        case Instruction::move_compare_add_subtract_immediate: return "move_compare_add_subtract_immediate";
        case Instruction::alu_operations: return "alu_operations";
        case Instruction::hi_register_operations: return "hi_register_operations";
        case Instruction::pc_relative_load: return "pc_relative_load";
        case Instruction::load_store_with_register_offset: return "load_store_with_register_offset";
        case Instruction::load_store_sign_extended_byte_halfword: return "load_store_sign_extended_byte_halfword";
        case Instruction::load_store_with_immediate_offset: return "load_store_with_immediate_offset";
        case Instruction::load_store_halfword: return "load_store_halfword";
        case Instruction::sp_relative_load_store: return "sp_relative_load_store";
        case Instruction::load_address: return "load_address";
        case Instruction::add_offset_to_stack_pointer: return "add_offset_to_stack_pointer";
        case Instruction::push_pop_registers: return "push_pop_registers";
        case Instruction::multiple_load_store: return "multiple_load_store";
        case Instruction::conditional_branch: return "conditional_branch";
        case Instruction::software_interrupt: return "software_interrupt";
        case Instruction::unconditional_branch: return "unconditional_branch";
        case Instruction::long_branch_with_link: return "long_branch_with_link";
        case Instruction::undefined: return "undefined";
    }

    return "NULL";
}

#if RELEASE_BUILD == 3
auto undefined(gba::Gba &gba, uint16_t opcode) -> void
{
    printf("undefined %04X\n", opcode);
}

template <int i, int end>
consteval auto fill_table(auto& table) -> void
{
    constexpr auto instruction = decode(i);

    switch (instruction)
    {
        case Instruction::move_shifted_register: table[i] = move_shifted_register<i>; break;
        case Instruction::add_subtract: table[i] = add_subtract<i>; break;
        case Instruction::move_compare_add_subtract_immediate: table[i] = move_compare_add_subtract_immediate<i>; break;
        case Instruction::alu_operations: table[i] = alu_operations<i>; break;
        case Instruction::hi_register_operations: table[i] = hi_register_operations<i>; break;
        case Instruction::pc_relative_load: table[i] = pc_relative_load<i>; break;
        case Instruction::load_store_with_register_offset: table[i] = load_store_with_register_offset<i>; break;
        case Instruction::load_store_sign_extended_byte_halfword: table[i] = load_store_sign_extended_byte_halfword<i>; break;
        case Instruction::load_store_with_immediate_offset: table[i] = load_store_with_immediate_offset<i>; break;
        case Instruction::load_store_halfword: table[i] = load_store_halfword<i>; break;
        case Instruction::sp_relative_load_store: table[i] = sp_relative_load_store<i>; break;
        case Instruction::load_address: table[i] = load_address<i>; break;
        case Instruction::add_offset_to_stack_pointer: table[i] = add_offset_to_stack_pointer; break;
        case Instruction::push_pop_registers: table[i] = push_pop_registers<i>; break;
        case Instruction::multiple_load_store: table[i] = multiple_load_store<i>; break;
        case Instruction::conditional_branch: table[i] = conditional_branch; break;
        case Instruction::software_interrupt: table[i] = software_interrupt; break;
        case Instruction::unconditional_branch: table[i] = unconditional_branch; break;
        case Instruction::long_branch_with_link: table[i] = long_branch_with_link; break;
        case Instruction::undefined: table[i] = undefined; break;
    }

    if constexpr (i == end)
    {
        return;
    }
    else
    {
        fill_table<i + 1, end>(table);
    }
}

using func_type = void (*)(gba::Gba&, uint16_t);

consteval auto generate_function_table()
{
    std::array<func_type, 1024> table{};

    fill_table<0x0000, 0x00FF>(table);
    fill_table<0x0100, 0x01FF>(table);
    fill_table<0x0200, 0x02FF>(table);
    fill_table<0x0300, 0x03FF>(table);

    return table;
};

constexpr auto func_table = generate_function_table();

auto execute(Gba& gba) -> void
{
    const auto opcode = fetch(gba);
    func_table[opcode>>6](gba, opcode);
}
#elif RELEASE_BUILD == 2
template <int i, int end>
consteval auto fill_table(auto& table) -> void
{
    constexpr auto instruction = decode(i);

    switch (instruction)
    {
        case Instruction::move_shifted_register: table[i] = move_shifted_register<i>; break;
        case Instruction::add_subtract: table[i] = add_subtract<i>; break;
        case Instruction::move_compare_add_subtract_immediate: table[i] = move_compare_add_subtract_immediate<i>; break;
        case Instruction::alu_operations: table[i] = alu_operations<i>; break;
        case Instruction::hi_register_operations: table[i] = hi_register_operations<i>; break;
        case Instruction::pc_relative_load: table[i] = pc_relative_load<i>; break;
        case Instruction::load_store_with_register_offset: table[i] = load_store_with_register_offset<i>; break;
        case Instruction::load_store_sign_extended_byte_halfword: table[i] = load_store_sign_extended_byte_halfword<i>; break;
        case Instruction::load_store_with_immediate_offset: table[i] = load_store_with_immediate_offset<i>; break;
        case Instruction::load_store_halfword: table[i] = load_store_halfword<i>; break;
        case Instruction::sp_relative_load_store: table[i] = sp_relative_load_store<i>; break;
        case Instruction::load_address: table[i] = load_address<i>; break;
        case Instruction::add_offset_to_stack_pointer: table[i] = add_offset_to_stack_pointer<i>; break;
        case Instruction::push_pop_registers: table[i] = push_pop_registers<i>; break;
        case Instruction::multiple_load_store: table[i] = multiple_load_store<i>; break;
        case Instruction::conditional_branch: table[i] = conditional_branch<i>; break;
        case Instruction::software_interrupt: table[i] = software_interrupt<i>; break;
        case Instruction::unconditional_branch: table[i] = unconditional_branch<i>; break;
        case Instruction::long_branch_with_link: table[i] = long_branch_with_link<i>; break;
        case Instruction::undefined: break;
    }

    if constexpr (i == end)
    {
        return;
    }
    else
    {
        fill_table<i + 1, end>(table);
    }
}

using func_type = void (*)(gba::Gba&, uint16_t);

consteval auto generate_function_table()
{
    std::array<func_type, 0x100> table{};

    fill_table<0x0000, 0x00FF>(table);

    return table;
};

constexpr auto func_table = generate_function_table();

auto execute(Gba& gba) -> void
{
    const auto opcode = fetch(gba);
    func_table[opcode>>8](gba, opcode);
}
#else

// page 108
auto execute(Gba& gba) -> void
{
    const auto opcode = fetch(gba);
    const auto instruction = decode(opcode);

    // if (CPU.breakpoint)
    // {
    //     std::printf("[THUMB] PC: 0x%08X opcode: 0x%04X decoded: %s\n", get_pc(gba) - (CPU.cpsr.T ? 2 * 2 : 4 * 2), opcode, decode_str(instruction));
    //     // std::printf("[ARM] PC: 0x%08X opcode: 0x%08X decoded: %s", get_pc(gba), opcode, decode_str(instruction));
    // }

    switch (instruction)
    {
        case Instruction::move_shifted_register: move_shifted_register(gba, opcode); break;
        case Instruction::add_subtract: add_subtract(gba, opcode); break;
        case Instruction::move_compare_add_subtract_immediate: move_compare_add_subtract_immediate(gba, opcode); break;
        case Instruction::alu_operations: alu_operations(gba, opcode); break;
        case Instruction::hi_register_operations: hi_register_operations(gba, opcode); break;
        case Instruction::pc_relative_load: pc_relative_load(gba, opcode); break;
        case Instruction::load_store_with_register_offset: load_store_with_register_offset(gba, opcode); break;
        case Instruction::load_store_sign_extended_byte_halfword: load_store_sign_extended_byte_halfword(gba, opcode); break;
        case Instruction::load_store_with_immediate_offset: load_store_with_immediate_offset(gba, opcode); break;
        case Instruction::load_store_halfword: load_store_halfword(gba, opcode); break;
        case Instruction::sp_relative_load_store: sp_relative_load_store(gba, opcode); break;
        case Instruction::load_address: load_address(gba, opcode); break;
        case Instruction::add_offset_to_stack_pointer: add_offset_to_stack_pointer(gba, opcode); break;
        case Instruction::push_pop_registers: push_pop_registers(gba, opcode); break;
        case Instruction::multiple_load_store: multiple_load_store(gba, opcode); break;
        case Instruction::conditional_branch: conditional_branch(gba, opcode); break;
        case Instruction::software_interrupt: software_interrupt(gba, opcode); break;
        case Instruction::unconditional_branch: unconditional_branch(gba, opcode); break;
        case Instruction::long_branch_with_link: long_branch_with_link(gba, opcode); break;
        case Instruction::undefined: assert(!"[THUMB] unimpl undefined"); break;
    }
}
#endif

} // namespace gba::arm7tdmi::thumb
