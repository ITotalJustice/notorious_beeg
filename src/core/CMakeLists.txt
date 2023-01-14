cmake_minimum_required(VERSION 3.20.0)


project(notorious_beeg
    LANGUAGES CXX
)

# compiler basically inlines everything
option(SINGLE_FILE "compile everything as a single file" OFF)
# enable sanitizers
option(GBA_DEV "enable sanitizers" OFF)
# enable logging
option(GBA_LOGGER "enable logging" OFF)

if (SINGLE_FILE)
    add_library(GBA single.cpp)
else ()
    add_library(GBA
        gba.cpp
        ppu/ppu.cpp
        ppu/render.cpp
        mem.cpp
        dma.cpp
        timer.cpp
        apu/apu.cpp
        bios.cpp
        bios_hle.cpp
        gpio.cpp
        rtc.cpp
        key.cpp
        sio.cpp
        log.cpp
        waitloop.cpp

        backup/backup.cpp
        backup/eeprom.cpp
        backup/flash.cpp
        backup/sram.cpp

        arm7tdmi/arm7tdmi.cpp
        arm7tdmi/arm/arm_table.cpp
        arm7tdmi/thumb/thumb_table.cpp

        fat/fat.cpp
        fat/mpcf.cpp
        fat/m3cf.cpp
        fat/sccf.cpp
        fat/ezflash.cpp
        fat/ezflash/S71GL064A08.cpp
        fat/ezflash/S98WS512PE0.cpp

        gameboy/gb.cpp
        gameboy/cpu.cpp
        gameboy/bus.cpp
        gameboy/joypad.cpp
        gameboy/ppu/ppu.cpp
        gameboy/ppu/dmg_renderer.cpp
        gameboy/ppu/gbc_renderer.cpp
        gameboy/mbc.cpp
        gameboy/timers.cpp
        gameboy/palette_table.cpp
    )
endif()

target_include_directories(GBA PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_add_common_cflags(GBA PRIVATE)

# enable sanitizer_flags
if (GBA_DEV)
    list(APPEND sanitizer_flags
        # -fsanitize=address
        # -fsanitize=leak

        -fsanitize=undefined
        # i think all of these below are included in undefined
        -fsanitize=shift
        -fsanitize=shift-exponent
        -fsanitize=shift-base
        -fsanitize=integer-divide-by-zero
        -fsanitize=signed-integer-overflow
        -fsanitize=alignment
        -fsanitize=bool
        -fsanitize=enum
        -fsanitize=vptr
        -fsanitize=pointer-overflow

        # -fsanitize=thread
        -fstack-protector-all
    )

    target_add_compiler_flags(GBA sanitizer_flags PUBLIC)
    target_add_linker_flags(GBA sanitizer_flags PUBLIC)
endif()

target_compile_definitions(GBA PRIVATE
    GBA_DEBUG=$<BOOL:${GBA_DEBUG}>
    GBA_LOGGER=$<BOOL:${GBA_LOGGER}>
    SINGLE_FILE=$<BOOL:${SINGLE_FILE}>
    ENABLE_SCHEDULER=$<BOOL:${ENABLE_SCHEDULER}>
)

set_target_properties(GBA PROPERTIES CXX_STANDARD 23)
