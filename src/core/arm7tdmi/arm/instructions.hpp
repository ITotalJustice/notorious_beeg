// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "arm7tdmi/arm7tdmi.hpp"

#if RELEASE_BUILD_ARM == 1

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
#else

namespace gba::arm7tdmi::arm {

auto branch(Gba& gba, uint32_t opcode) -> void;
auto branch_link(Gba& gba, uint32_t opcode) -> void;
auto data_processing(Gba& gba, uint32_t opcode) -> void;
auto single_data_transfer(Gba& gba, uint32_t opcode) -> void;
auto halfword_data_transfer_register_offset(Gba& gba, uint32_t opcode) -> void;
auto halfword_data_transfer_immediate_offset(Gba& gba, uint32_t opcode) -> void;
auto block_data_transfer(Gba& gba, uint32_t opcode) -> void;
auto multiply(Gba& gba, uint32_t opcode) -> void;
auto software_interrupt(Gba& gba, uint32_t opcode) -> void;
auto branch_and_exchange(Gba& gba, uint32_t opcode) -> void;
auto multiply_long(Gba& gba, uint32_t opcode) -> void;
auto single_data_swap(Gba& gba, uint32_t opcode) -> void;

} // namespace gba::arm7tdmi::arm
#endif
