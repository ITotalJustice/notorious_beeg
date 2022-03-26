cmake_minimum_required(VERSION 3.20.0)

# # huge speedup, disable if encounter bug
# option(ENABLE_SCHEDULER "huge speedup, but can cause timing bugs" ON)
# # compiler basically inlines everything
# option(SINGLE_FILE "compile everything as a single file" OFF)
# # actually slower for doom (~30fps slower), MUCH faster for everything else
# option(RELEASE_BUILD_THUMB "template thumb" OFF)
# # very big speedup
# option(RELEASE_BUILD_ARM "template arm" ON)
# # enable sanitizers
# option(GBA_DEV "enable sanitizers" OFF)

# huge speedup, disable if encounter bug
set(ENABLE_SCHEDULER ON)
# compiler basically inlines everything
set(SINGLE_FILE OFF)
# enable sanitizers
set(GBA_DEV OFF)


if (SINGLE_FILE)
    add_library(GBA single.cpp)
else ()
    add_library(GBA
        gba.cpp
        ppu.cpp
        mem.cpp
        dma.cpp
        timer.cpp
        apu/apu.cpp
        bios.cpp
        bios_hle.cpp
        scheduler.cpp

        backup/backup.cpp
        backup/eeprom.cpp
        backup/flash.cpp
        backup/sram.cpp

        arm7tdmi/arm7tdmi.cpp
        arm7tdmi/arm/arm.cpp
        arm7tdmi/thumb/thumb.cpp
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
    SINGLE_FILE=$<BOOL:${SINGLE_FILE}>
    ENABLE_SCHEDULER=$<BOOL:${ENABLE_SCHEDULER}>
)

# enable LTO (only in release builds)
# this greatly reduces template bloat as well
if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    if (ipo_supported)
        message(STATUS "IPO / LTO enabled")
        set_property(TARGET GBA PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO / LTO not supported: <${ipo_error}>")
    endif()
else()
    message(STATUS "IPO / LTO not enabled in debug build")
endif()
