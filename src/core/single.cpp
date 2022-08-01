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
    #include "rtc.cpp"

    #include "backup/backup.cpp"
    #include "backup/eeprom.cpp"
    #include "backup/flash.cpp"
    #include "backup/sram.cpp"

    #include "arm7tdmi/arm7tdmi.cpp"
    #include "arm7tdmi/arm/arm.cpp"
    #include "arm7tdmi/thumb/thumb.cpp"
#endif
