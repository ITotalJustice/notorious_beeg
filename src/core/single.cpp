// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only


#ifndef SINGLE_FILE
    #define SINGLE_FILE 0
#endif

#if SINGLE_FILE == 1
    #include "gba.cpp"
    #include "ppu/ppu.cpp"
    #include "ppu/render.cpp"
    #include "mem.cpp"
    #include "dma.cpp"
    #include "timer.cpp"
    #include "apu/apu.cpp"
    #include "bios.cpp"
    #include "bios_hle.cpp"
    #include "scheduler.cpp"
    #include "gpio.cpp"
    #include "rtc.cpp"

    #include "backup/backup.cpp"
    #include "backup/eeprom.cpp"
    #include "backup/flash.cpp"
    #include "backup/sram.cpp"

    #include "arm7tdmi/arm7tdmi.cpp"

    #ifndef INTERPRETER_TABLE
        #define INTERPRETER_TABLE 0
    #endif // INTERPRETER_TABLE
    #ifndef INTERPRETER_SWITCH
        #define INTERPRETER_SWITCH 1
    #endif // INTERPRETER_SWITCH
    #ifndef INTERPRETER_GOTO
        #define INTERPRETER_GOTO 2
    #endif // INTERPRETER_GOTO

    #ifndef INTERPRETER
        #define INTERPRETER INTERPRETER_TABLE
    #endif

    #if INTERPRETER == INTERPRETER_TABLE
        #include "arm7tdmi/arm/arm_table.cpp"
        #include "arm7tdmi/thumb/thumb_table.cpp"
    #elif INTERPRETER == INTERPRETER_SWITCH
        #include "arm7tdmi/arm/arm_switch.cpp"
        #include "arm7tdmi/thumb/thumb_switch.cpp"
    #elif INTERPRETER == INTERPRETER_GOTO
        #include "arm7tdmi/arm/arm_goto.cpp"
        #include "arm7tdmi/thumb/thumb_goto.cpp"
    #endif
#endif
