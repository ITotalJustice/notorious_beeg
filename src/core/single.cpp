// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only


#ifndef SINGLE_FILE
#define SINGLE_FILE 0
#endif

#if SINGLE_FILE == 1
#include "gba.cpp"
#include "ppu.cpp"
#include "mem.cpp"
#include "dma.cpp"
#include "timer.cpp"
#include "apu/apu.cpp"
#include "bios_hle.cpp"
#include "scheduler.cpp"

#include "backup/backup.cpp"
#include "backup/eeprom.cpp"
#include "backup/flash.cpp"
#include "backup/sram.cpp"

#include "arm7tdmi/arm7tdmi.cpp"
#include "arm7tdmi/arm/arm.cpp"
#include "arm7tdmi/thumb/thumb.cpp"

#if RELEASE_BUILD_ARM != 1
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
#endif

#if RELEASE_BUILD != 1 && RELEASE_BUILD != 2 && RELEASE_BUILD != 3
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
#endif
#endif
