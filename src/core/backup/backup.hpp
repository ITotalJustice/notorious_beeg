// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <span>
#include "eeprom.hpp"
#include "sram.hpp"
#include "flash.hpp"

namespace gba::backup {

enum class Type : u8
{
    NONE, // no backup chip
    EEPROM, // 512bytes
    SRAM, // 32k
    FLASH, // 64k
    FLASH512, // 64k
    FLASH1M, // 128k
};

struct Backup
{
    union
    {
        eeprom::Eeprom eeprom;
        sram::Sram sram;
        flash::Flash flash;
    };

    Type type;
    bool dirty_ram;
};

STATIC auto find_type(std::span<const u8> rom) -> Type;

} // namespace gba::backup
