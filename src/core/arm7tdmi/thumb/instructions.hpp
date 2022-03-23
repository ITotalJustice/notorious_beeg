// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "arm7tdmi/arm7tdmi.hpp"

#if RELEASE_BUILD == 1 || RELEASE_BUILD == 2 || RELEASE_BUILD == 3

#include "arm7tdmi/thumb/move_compare_add_subtract_immediate.cpp"
#include "arm7tdmi/thumb/hi_register_operations.cpp"
#include "arm7tdmi/thumb/load_address.cpp"
#include "arm7tdmi/thumb/conditional_branch.cpp"
#include "arm7tdmi/thumb/unconditional_branch.cpp"
#include "arm7tdmi/thumb/move_shifted_register.cpp"
#include "arm7tdmi/thumb/add_subtract.cpp"
#include "arm7tdmi/thumb/long_branch_with_link.cpp"
#include "arm7tdmi/thumb/alu_operations.cpp"
#include "arm7tdmi/thumb/add_offset_to_stack_pointer.cpp"
#include "arm7tdmi/thumb/pc_relative_load.cpp"
#include "arm7tdmi/thumb/load_store_with_register_offset.cpp"
#include "arm7tdmi/thumb/load_store_sign_extended_byte_halfword.cpp"
#include "arm7tdmi/thumb/load_store_with_immediate_offset.cpp"
#include "arm7tdmi/thumb/load_store_halfword.cpp"
#include "arm7tdmi/thumb/sp_relative_load_store.cpp"
#include "arm7tdmi/thumb/push_pop_registers.cpp"
#include "arm7tdmi/thumb/multiple_load_store.cpp"
#include "arm7tdmi/thumb/software_interrupt.cpp"

#else
namespace gba::arm7tdmi::thumb {

auto move_compare_add_subtract_immediate(Gba& gba, uint16_t opcode) -> void;
auto hi_register_operations(Gba& gba, uint16_t opcode) -> void;
auto load_address(Gba& gba, uint16_t opcode) -> void;
auto conditional_branch(Gba& gba, uint16_t opcode) -> void;
auto unconditional_branch(Gba& gba, uint16_t opcode) -> void;
auto move_shifted_register(Gba& gba, uint16_t opcode) -> void;
auto add_subtract(Gba& gba, uint16_t opcode) -> void;
auto long_branch_with_link(Gba& gba, uint16_t opcode) -> void;
auto alu_operations(Gba& gba, uint16_t opcode) -> void;
auto add_offset_to_stack_pointer(Gba& gba, uint16_t opcode) -> void;
auto pc_relative_load(Gba& gba, uint16_t opcode) -> void;
auto load_store_with_register_offset(Gba& gba, uint16_t opcode) -> void;
auto load_store_sign_extended_byte_halfword(Gba& gba, uint16_t opcode) -> void;
auto load_store_with_immediate_offset(Gba& gba, uint16_t opcode) -> void;
auto load_store_halfword(Gba& gba, uint16_t opcode) -> void;
auto sp_relative_load_store(Gba& gba, uint16_t opcode) -> void;
auto push_pop_registers(Gba& gba, uint16_t opcode) -> void;
auto multiple_load_store(Gba& gba, uint16_t opcode) -> void;
auto software_interrupt(Gba& gba, uint16_t opcode) -> void;

} // namespace gba::arm7tdmi::thumb
#endif
