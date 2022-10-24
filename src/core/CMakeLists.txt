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

set(INTERPRETER_TABLE 0)
set(INTERPRETER_SWITCH 1)
set(INTERPRETER_GOTO 2)

# if an interpreter backend hasn't been set, default to INTERPRETER_SWITCH
if (NOT DEFINED INTERPRETER)
    set(INTERPRETER ${INTERPRETER_SWITCH})
endif()

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

        backup/backup.cpp
        backup/eeprom.cpp
        backup/flash.cpp
        backup/sram.cpp

        arm7tdmi/arm7tdmi.cpp

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

    if (${INTERPRETER} EQUAL ${INTERPRETER_TABLE})
        target_sources(GBA PRIVATE arm7tdmi/arm/arm_table.cpp)
        target_sources(GBA PRIVATE arm7tdmi/thumb/thumb_table.cpp)
    elseif(${INTERPRETER} EQUAL ${INTERPRETER_SWITCH})
        target_sources(GBA PRIVATE arm7tdmi/arm/arm_switch.cpp)
        target_sources(GBA PRIVATE arm7tdmi/thumb/thumb_switch.cpp)
    elseif(${INTERPRETER} EQUAL ${INTERPRETER_GOTO})
        target_sources(GBA PRIVATE arm7tdmi/arm/arm_goto.cpp)
        target_sources(GBA PRIVATE arm7tdmi/thumb/thumb_goto.cpp)
    endif()
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
    INTERPRETER=${INTERPRETER}
    INTERPRETER_TABLE=${INTERPRETER_TABLE}
    INTERPRETER_SWITCH=${INTERPRETER_SWITCH}
    INTERPRETER_GOTO=${INTERPRETER_GOTO}
)

set_target_properties(GBA PROPERTIES CXX_STANDARD 23)
