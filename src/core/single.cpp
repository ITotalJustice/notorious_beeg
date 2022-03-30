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
    #if EXECUTER == EXECUTER_TABLE
        #include "arm7tdmi/arm/arm_table.cpp"
        #include "arm7tdmi/thumb/thumb_table.cpp"
    #elif EXECUTER == EXECUTER_SWITCH
        #include "arm7tdmi/arm/arm_switch.cpp"
        #include "arm7tdmi/thumb/thumb_switch.cpp"
    #elif EXECUTER == EXECUTER_GOTO
        #include "arm7tdmi/arm/arm_goto.cpp"
        #include "arm7tdmi/thumb/thumb_goto.cpp"
    #endif
#endif
