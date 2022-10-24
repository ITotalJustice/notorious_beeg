// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "log.hpp"

namespace gba::log {

auto get_level_str() -> std::span<const char*>
{
    static const char* str[] =
    {
        "FATAL",
        "ERROR",
        "WARN",
        "INFO",
        "DEBUG"
    };

    return str;
}

auto get_type_str() -> std::span<const char*>
{
    static const char* str[] =
    {
        "PPU",
        "SQUARE0",
        "SQUARE1",
        "WAVE",
        "NOISE",
        "FRAME_SEQUENCER",
        "TIMER0",
        "TIMER1",
        "TIMER2",
        "TIMER3",
        "DMA0",
        "DMA1",
        "DMA2",
        "DMA3",
        "INTERRUPT",
        "HALT",
        "ARM",
        "THUMB",
        "MEMORY",
        "EEPROM",
        "FLASH",
        "SRAM",
        "GPIO",
        "SIO",
        "EZFLASH",
        "M3CF",
        "M3SD",
        "MPCF",
        "SCCF",
        "SCSD",
        "GB_BUS",
        "GB_CPU",
        "GB_PPU",
        "GB_MBC0",
        "GB_MBC1",
        "GB_MBC2",
        "GB_MBC3",
        "GB_MBC5",
        "GB_TIMER",
        "GB_DIV",
        "GAME"
    };

    return str;
}

} // namespace gba::log
