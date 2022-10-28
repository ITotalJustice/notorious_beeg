// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <span>
#include "fwd.hpp"
#include "eeprom.hpp"
#include "sram.hpp"
#include "flash.hpp"

namespace gba::backup {

enum class Type : u8
{
    NONE, // no backup chip
    EEPROM, // unknown size
    EEPROM512, // 512bytes
    EEPROM8K, // 8k
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

    [[nodiscard]] auto is_eeprom() const -> bool
    {
        return type == Type::EEPROM || type == Type::EEPROM512 || type == Type::EEPROM8K;
    }

    [[nodiscard]] auto is_sram() const -> bool
    {
        return type == Type::SRAM;
    }

    [[nodiscard]] auto is_flash() const -> bool
    {
        return type == Type::FLASH || type == Type::FLASH512 || type == Type::FLASH1M;
    }

    Type type;
    bool dirty_ram;
};

auto find_type(std::span<const u8> rom) -> Type;

} // namespace gba::backup
