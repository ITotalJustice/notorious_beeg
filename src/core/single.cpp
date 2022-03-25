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
#include "bios.cpp"
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

#endif
