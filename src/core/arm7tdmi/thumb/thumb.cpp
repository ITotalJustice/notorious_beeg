// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "move_compare_add_subtract_immediate.cpp"
#include "hi_register_operations.cpp"
#include "load_address.cpp"
#include "conditional_branch.cpp"
#include "unconditional_branch.cpp"
#include "move_shifted_register.cpp"
#include "add_subtract.cpp"
#include "long_branch_with_link.cpp"
#include "alu_operations.cpp"
#include "add_offset_to_stack_pointer.cpp"
#include "pc_relative_load.cpp"
#include "load_store_with_register_offset.cpp"
#include "load_store_sign_extended_byte_halfword.cpp"
#include "load_store_with_immediate_offset.cpp"
#include "load_store_halfword.cpp"
#include "sp_relative_load_store.cpp"
#include "push_pop_registers.cpp"
#include "multiple_load_store.cpp"
#include "software_interrupt.cpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "gba.hpp"
#include <cassert>
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

// page 108
static constexpr auto decode(uint16_t opcode) -> Instruction
{
    // only need bits 5-15
    constexpr auto shift_down = 6;

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

    // not sure if order matters ngl, probably best to not mess with it
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

static auto undefined([[maybe_unused]] gba::Gba &gba, uint16_t opcode) -> void
{
    std::printf("[THUMB] undefined %04X\n", opcode);
    assert(!"[THUMB] undefined instruction hit");
}

template<auto b>
consteval auto decoded_is_set(auto v)
{
    static_assert(b >= 6, "invalid");

    constexpr auto new_bit = b - 6;
    return bit::is_set<new_bit>(v);
}

template<u8 start, u8 end>
consteval auto decoded_get_range(auto v)
{
    static_assert(start >= 6, "invalid");
    static_assert(end >= 6, "invalid");

    constexpr u8 new_start = start - 6;
    constexpr u8 new_end = end - 6;

    return bit::get_range<new_start, new_end>(v);
}

template <int i, int end>
consteval auto fill_table(auto& table) -> void
{
    constexpr auto instruction = decode(i);

    switch (instruction)
    {
        case Instruction::move_shifted_register: {
            constexpr auto Op = decoded_get_range<11, 12>(i);
            table[i] = move_shifted_register<Op>;
        } break;

        case Instruction::add_subtract: {
            constexpr auto I = decoded_is_set<10>(i); // 0=reg, 1=imm
            constexpr auto Op = decoded_is_set<9>(i); // 0=ADD, 1=SUB
            table[i] = add_subtract<I, Op>;
        } break;

        case Instruction::move_compare_add_subtract_immediate: {
            constexpr auto Op = decoded_get_range<11, 12>(i);
            table[i] = move_compare_add_subtract_immediate<Op>;
        } break;

        case Instruction::alu_operations: {
            constexpr auto Op = decoded_get_range<6, 9>(i);
            table[i] = alu_operations<Op>;
        } break;

        case Instruction::hi_register_operations: {
            constexpr auto Op = decoded_get_range<8, 9>(i);
            constexpr auto H1 = decoded_is_set<7>(i) ? 8 : 0;
            constexpr auto H2 = decoded_is_set<6>(i) ? 8 : 0;
            table[i] = hi_register_operations<Op, H1, H2>;
        } break;

        case Instruction::pc_relative_load: {
            table[i] = pc_relative_load;
        } break;

        case Instruction::load_store_with_register_offset: {
            constexpr auto L = decoded_is_set<11>(i); // 0=STR, 1=LDR
            constexpr auto B = decoded_is_set<10>(i); // 0=word, 1=byte
            table[i] = load_store_with_register_offset<L, B>;
        } break;

        case Instruction::load_store_sign_extended_byte_halfword: {
            constexpr auto H = decoded_is_set<11>(i); // 0=STR, 1=LDR
            constexpr auto S = decoded_is_set<10>(i); // 0=normal, 1=sign-extended
            table[i] = load_store_sign_extended_byte_halfword<H, S>;
        } break;

        case Instruction::load_store_with_immediate_offset: {
            constexpr auto B = decoded_is_set<12>(i); // 0=word, 1=byte
            constexpr auto L = decoded_is_set<11>(i); // 0=STR, 1=LDR
            table[i] = load_store_with_immediate_offset<B, L>;
        } break;

        case Instruction::load_store_halfword: {
            constexpr auto L = decoded_is_set<11>(i); // 0=STR, 1=LDR
            table[i] = load_store_halfword<L>;
        } break;

        case Instruction::sp_relative_load_store: {
            constexpr auto L = decoded_is_set<11>(i); // 0=STR, 1=LDR
            table[i] = sp_relative_load_store<L>;
        } break;

        case Instruction::load_address: {
            constexpr auto SP = decoded_is_set<11>(i); // 0=PC, 1=SP
            table[i] = load_address<SP>;
        } break;

        case Instruction::add_offset_to_stack_pointer: {
            constexpr auto S = decoded_is_set<7>(i); // 0=unsigned, 1=signed
            table[i] = add_offset_to_stack_pointer<S>;
        } break;

        case Instruction::push_pop_registers: {
            constexpr auto L = decoded_is_set<11>(i); // 0=push, 1=pop
            constexpr auto R = decoded_is_set<8>(i); // 0=non, 1=store lr/load pc
            table[i] = push_pop_registers<L, R>;
        } break;

        case Instruction::multiple_load_store: {
            constexpr auto L = decoded_is_set<11>(i); // 0=store, 1=load
            table[i] = multiple_load_store<L>;
        } break;

        case Instruction::conditional_branch: {
            table[i] = conditional_branch;
        } break;

        case Instruction::software_interrupt: {
            table[i] = software_interrupt;
        } break;

        case Instruction::unconditional_branch: {
            table[i] = unconditional_branch;
        } break;

        case Instruction::long_branch_with_link: {
            constexpr auto H = decoded_is_set<11>(i);
            table[i] = long_branch_with_link<H>;
        } break;

        case Instruction::undefined: {
            table[i] = undefined;
        } break;
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

consteval auto generate_function_table()
{
    using func_type = void (*)(gba::Gba&, uint16_t);
    std::array<func_type, 1024> table{};
    table.fill(undefined); // also handled in fill_table.

    fill_table<0x0000, 0x00FF>(table);
    fill_table<0x0100, 0x01FF>(table);
    fill_table<0x0200, 0x02FF>(table);
    fill_table<0x0300, 0x03FF>(table);

    return table;
};

static auto fetch(Gba& gba)
{
    const std::uint16_t opcode = CPU.pipeline[0];
    CPU.pipeline[0] = CPU.pipeline[1];
    gba.cpu.registers[PC_INDEX] += 2;
    CPU.pipeline[1] = mem::read16(gba, get_pc(gba));

    return opcode;
}

auto execute(Gba& gba) -> void
{
    static constexpr auto func_table = generate_function_table();

    const auto opcode = fetch(gba);
    func_table[opcode>>6](gba, opcode);
}

} // namespace gba::arm7tdmi::thumb
