// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <span>
#include "fwd.hpp"
#include "eeprom.hpp"
#include "sram.hpp"
#include "flash.hpp"

namespace gba::backup {

#if 0
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
#else
enum Type : u16
{
    NONE = 0, // no backup chip
    EEPROM = 1 << 1, // 512bytes
    EEPROM512 = 1 << 2, // 512bytes
    EEPROM8K = 1 << 3, // 8k
    SRAM = 1 << 4, // 32k
    FLASH512 = 1 << 5, // 64k
    FLASH1M = 1 << 6, // 128k

    EZFLASH_NONE = (1 << 7) | NONE,
    EZFLASH_EEPROM512 = (1 << 7) | EEPROM512,
    EZFLASH_EEPROM8K = (1 << 7) | EEPROM8K,
    EZFLASH_SRAM = (1 << 7) | SRAM,
    EZFLASH_FLASH512 = (1 << 7) | FLASH512,
    EZFLASH_FLASH1M = (1 << 7) | FLASH1M,
};
#endif

struct Backup
{
    union
    {
        eeprom::Eeprom eeprom;
        sram::Sram sram;
        flash::Flash flash;
    };

    Type type;

    auto init(Gba& gba, Type new_type) -> bool;

    auto load_data(Gba& gba, std::span<const u8> new_data) -> bool;
    [[nodiscard]] auto get_data(const Gba& gba) const -> SaveData;

    [[nodiscard]] auto is_eeprom() const -> bool
    {
        return type & Type::EEPROM512 || type & Type::EEPROM8K;
    }

    [[nodiscard]] auto is_sram() const -> bool
    {
        return type & Type::SRAM;
    }

    [[nodiscard]] auto is_flash() const -> bool
    {
        return type & Type::FLASH512 || type & Type::FLASH1M;
    }

    [[nodiscard]] auto is_dirty(Gba& gba) const -> bool;
    void clear_dirty_flag(Gba& gba);
};

[[nodiscard]] auto find_type(std::span<const u8> rom) -> Type;

} // namespace gba::backup
