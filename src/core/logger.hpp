// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// HOMEBREW DEVS: to log output to this emulator (and mgba) you can
// use this helper header that i wrote
// https://gist.github.com/ITotalJustice/7491efcd51f0c73cd0ee0bcf024ae0f1
#pragma once

#include "bit.hpp"
#include "gba.hpp"
#include <cstdio>
#include <utility>

#ifndef GBA_LOGGER
    #define GBA_LOGGER 0
#endif

namespace gba::log {

enum Level : u8
{
    FATAL = 0,
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    DEBUG = 4,
};

enum Type : u8
{
    PPU,

    SQUARE0,
    SQUARE1,
    WAVE,
    NOISE,
    FRAME_SEQUENCER,

    TIMER0,
    TIMER1,
    TIMER2,
    TIMER3,

    DMA0,
    DMA1,
    DMA2,
    DMA3,

    INTERRUPT,
    HALT,

    ARM,
    THUMB,

    MEMORY,

    EEPROM,
    FLASH,
    SRAM,

    GPIO,

    EZFLASH,
    M3CF,
    M3SD,
    MPCF,
    SCCF,
    SCSD,

    GB_BUS,
    GB_CPU,
    GB_PPU,
    GB_MBC0,
    GB_MBC1,
    GB_MBC2,
    GB_MBC3,
    GB_MBC5,
    GB_TIMER,
    GB_DIV,

    GAME,

    MAX,
};

enum LevelFlag : u64
{
    FLAG_FATAL = 1ULL << Level::FATAL,
    FLAG_ERROR = 1ULL << Level::ERROR,
    FLAG_WARN = 1ULL << Level::WARN,
    FLAG_INFO = 1ULL << Level::INFO,
    FLAG_DEBUG = 1ULL << Level::DEBUG,

    FLAG_LEVEL_ALL = FLAG_FATAL | FLAG_ERROR | FLAG_WARN | FLAG_INFO | FLAG_DEBUG,
};

enum TypeFlag : u64
{
    FLAG_PPU = 1ULL << Type::PPU,
    FLAG_SQUARE0 = 1ULL << Type::SQUARE0,
    FLAG_SQUARE1 = 1ULL << Type::SQUARE1,
    FLAG_WAVE = 1ULL << Type::WAVE,
    FLAG_NOISE = 1ULL << Type::NOISE,
    FLAG_FRAME_SEQUENCER = 1ULL << Type::FRAME_SEQUENCER,
    FLAG_TIMER0 = 1ULL << Type::TIMER0,
    FLAG_TIMER1 = 1ULL << Type::TIMER1,
    FLAG_TIMER2 = 1ULL << Type::TIMER2,
    FLAG_TIMER3 = 1ULL << Type::TIMER3,
    FLAG_DMA0 = 1ULL << Type::DMA0,
    FLAG_DMA1 = 1ULL << Type::DMA1,
    FLAG_DMA2 = 1ULL << Type::DMA2,
    FLAG_DMA3 = 1ULL << Type::DMA3,
    FLAG_INTERRUPT = 1ULL << Type::INTERRUPT,
    FLAG_HALT = 1ULL << Type::HALT,
    FLAG_ARM = 1ULL << Type::ARM,
    FLAG_THUMB = 1ULL << Type::THUMB,
    FLAG_MEMORY = 1ULL << Type::MEMORY,
    FLAG_EEPROM = 1ULL << Type::EEPROM,
    FLAG_FLASH = 1ULL << Type::FLASH,
    FLAG_SRAM = 1ULL << Type::SRAM,
    FLAG_GPIO = 1ULL << Type::GPIO,
    FLAG_EZFLASH = 1ULL << Type::EZFLASH,
    FLAG_M3CF = 1ULL << Type::M3CF,
    FLAG_M3SD = 1ULL << Type::M3SD,
    FLAG_MPCF = 1ULL << Type::MPCF,
    FLAG_SCCF = 1ULL << Type::SCCF,
    FLAG_SCSD = 1ULL << Type::SCSD,
    FLAG_GB_BUS = 1ULL << Type::GB_BUS,
    FLAG_GB_CPU = 1ULL << Type::GB_CPU,
    FLAG_GB_PPU = 1ULL << Type::GB_PPU,
    FLAG_GB_MBC0 = 1ULL << Type::GB_MBC0,
    FLAG_GB_MBC1 = 1ULL << Type::GB_MBC1,
    FLAG_GB_MBC2 = 1ULL << Type::GB_MBC2,
    FLAG_GB_MBC3 = 1ULL << Type::GB_MBC3,
    FLAG_GB_MBC5 = 1ULL << Type::GB_MBC5,
    FLAG_GB_TIMER = 1ULL << Type::GB_TIMER,
    FLAG_GB_DIV = 1ULL << Type::GB_DIV,
    FLAG_GAME = 1ULL << Type::GAME,
    FLAG_MAX = 1ULL << Type::MAX,

    FLAG_TYPE_ALL_APU = FLAG_SQUARE0 | FLAG_SQUARE1 | FLAG_WAVE | FLAG_NOISE | FLAG_FRAME_SEQUENCER,
    FLAG_TYPE_ALL_TIMER = FLAG_TIMER0 | FLAG_TIMER1 | FLAG_TIMER2 | FLAG_TIMER3,
    FLAG_TYPE_ALL_DMA = FLAG_DMA0 | FLAG_DMA1 | FLAG_DMA2 | FLAG_DMA3,
    FLAG_TYPE_ALL_ARM = FLAG_ARM | FLAG_THUMB,
    FLAG_TYPE_ALL_BACKUP = FLAG_EEPROM | FLAG_FLASH | FLAG_SRAM,
    FLAG_TYPE_ALL_FAT = FLAG_EZFLASH | FLAG_M3CF | FLAG_M3SD | FLAG_MPCF | FLAG_SCCF | FLAG_SCSD,
    FLAG_TYPE_ALL_GB = FLAG_GB_BUS | FLAG_GB_CPU | FLAG_GB_PPU | FLAG_GB_MBC0 | FLAG_GB_MBC1 | FLAG_GB_MBC2 | FLAG_GB_MBC3 | FLAG_GB_MBC5 | FLAG_GB_TIMER | FLAG_GB_DIV,

    FLAG_TYPE_ALL = FLAG_PPU | FLAG_SQUARE0 | FLAG_SQUARE1 | FLAG_WAVE | FLAG_NOISE | FLAG_FRAME_SEQUENCER | FLAG_TIMER0 | FLAG_TIMER1 | FLAG_TIMER2 | FLAG_TIMER3 | FLAG_DMA0 | FLAG_DMA1 | FLAG_DMA2 | FLAG_DMA3 | FLAG_INTERRUPT | FLAG_HALT | FLAG_ARM | FLAG_THUMB | FLAG_MEMORY | FLAG_EEPROM | FLAG_FLASH | FLAG_SRAM | FLAG_GPIO | FLAG_EZFLASH | FLAG_M3CF | FLAG_M3SD | FLAG_MPCF | FLAG_SCCF | FLAG_SCSD | FLAG_GB_BUS | FLAG_GB_CPU | FLAG_GB_PPU | FLAG_GB_MBC0 | FLAG_GB_MBC1 | FLAG_GB_MBC2 | FLAG_GB_MBC3 | FLAG_GB_MBC5 | FLAG_GB_TIMER | FLAG_GB_DIV | FLAG_GAME,
};

#if GBA_LOGGER

inline void print(Gba& gba, u8 type, u8 level, const char* str)
{
    if (gba.log_callback && bit::is_set(gba.log_type, type) && bit::is_set(gba.log_level, level)) [[unlikely]]
    {
        gba.log_callback(gba.userdata, static_cast<u8>(type), static_cast<u8>(level), str);
    }
}

template <typename ...Args>
inline void print(Gba& gba, u8 type, u8 level, const char* fmt, Args... args)
{
    if (gba.log_callback && bit::is_set(gba.log_type, type) && bit::is_set(gba.log_level, level)) [[unlikely]]
    {
        char buf[0x101];
        std::snprintf(buf, sizeof(buf)-1, fmt, args...);
        buf[0x100] = '\0';
        gba.log_callback(gba.userdata, static_cast<u8>(type), static_cast<u8>(level), buf);
    }
}

template <typename ...Args> inline void print_fatal(Gba& gba, Type type, const char* fmt, Args... args) { print(gba, type, Level::FATAL, fmt, args...); }
template <typename ...Args> inline void print_error(Gba& gba, Type type, const char* fmt, Args... args) { print(gba, type, Level::ERROR, fmt, args...); }
template <typename ...Args> inline void print_warn(Gba& gba, Type type, const char* fmt, Args... args) { print(gba, type, Level::WARN, fmt, args...); }
template <typename ...Args> inline void print_info(Gba& gba, Type type, const char* fmt, Args... args) { print(gba, type, Level::INFO, fmt, args...); }
template <typename ...Args> inline void print_debug(Gba& gba, Type type, const char* fmt, Args... args) { print(gba, type, Level::DEBUG, fmt, args...); }

#else

inline void print([[maybe_unused]] Gba& gba, [[maybe_unused]] u8 type, u8 level, [[maybe_unused]] const char* str) { }
template <typename ...Args> inline void print([[maybe_unused]] Gba& gba, [[maybe_unused]] u8 type, u8 level, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) { }
template <typename ...Args> inline void print_fatal([[maybe_unused]] Gba& gba, [[maybe_unused]] Type type, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) { }
template <typename ...Args> inline void print_error([[maybe_unused]] Gba& gba, [[maybe_unused]] Type type, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) { }
template <typename ...Args> inline void print_warn([[maybe_unused]] Gba& gba, [[maybe_unused]] Type type, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) { }
template <typename ...Args> inline void print_info([[maybe_unused]] Gba& gba, [[maybe_unused]] Type type, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) { }
template <typename ...Args> inline void print_debug([[maybe_unused]] Gba& gba, [[maybe_unused]] Type type, [[maybe_unused]] const char* fmt, [[maybe_unused]] Args... args) { }

#endif

} // namespace gba::log
