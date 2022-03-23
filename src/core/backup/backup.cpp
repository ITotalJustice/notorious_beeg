// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup.hpp"
#include <string_view>
#include <iostream>
#include <cassert>

namespace gba::backup
{

auto find_type(std::span<const std::uint8_t> rom) -> Type
{
    constexpr struct
    {
        std::string_view string;
        Type type;
    } entries[] =
    {
        { .string = "EEPROM_V", .type = Type::EEPROM },
        { .string = "SRAM_V", .type = Type::SRAM },
        { .string = "FLASH_V", .type = Type::FLASH },
        { .string = "FLASH512_V", .type = Type::FLASH512 },
        { .string = "FLASH1M_V", .type = Type::FLASH1M },
    };

    std::string_view rom_string{reinterpret_cast<const char*>(rom.data()), rom.size()};

    for (auto& entry : entries)
    {
        if (rom_string.find(entry.string) != rom_string.npos)
        {
            std::cout << "[backup] found: " << entry.string << '\n';
            return entry.type;
        }
    }

    std::cout << "failed to find backup, assuming the game doesn't have one\n";

    return Type::NONE;
}

} // namespace gba::backup
