// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup.hpp"
#include <string_view>
#include <cstdio>
#include <cassert>

namespace gba::backup {

auto find_type(std::span<const u8> rom) -> Type
{
    constexpr struct
    {
        std::string_view string;
        Type type;
    } entries[] =
    {
        { .string = "EEPROM", .type = Type::EEPROM },
        { .string = "SRAM", .type = Type::SRAM },
        { .string = "FLASH_", .type = Type::FLASH },
        { .string = "FLASH512", .type = Type::FLASH512 },
        { .string = "FLASH1M", .type = Type::FLASH1M },
    };

    const auto rom_string = std::string_view{reinterpret_cast<const char*>(rom.data()), rom.size()};

    for (auto& [string, type] : entries)
    {
        if (rom_string.find(string) != std::string_view::npos)
        {
            std::printf("[backup] found: %.*s\n", static_cast<int>(string.size()), string.data());
            return type;
        }
    }

    std::printf("failed to find backup, assuming the game doesn't have one\n");

    return Type::NONE;
}

} // namespace gba::backup
