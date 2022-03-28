// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "eeprom.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <algorithm>
#include <ranges>
#include <cstdint>

namespace gba::backup::sram
{

auto Sram::init(Gba& gba) -> void
{
    // this is just to satisfy valid union access
    // by first writing to the union before reading
    // without causing UB.
    this->dummy_union_write = true;
}

auto Sram::load_data(std::span<const std::uint8_t> new_data) -> void
{
    if (new_data.size() <= std::size(this->data))
    {
        std::ranges::copy(new_data, this->data);
    }
    else
    {
        std::printf("[SRAM] tried loading bad save data: %zu\n", new_data.size());
    }
}

constexpr auto SRAM_MASK = sizeof(Sram::data)-1;

auto Sram::read(Gba& gba, u32 addr) -> u8
{
    return mem::read_array<u8>(this->data, SRAM_MASK, addr);
}

auto Sram::write(Gba& gba, u32 addr, u8 value) -> void
{
    mem::write_array<u8>(this->data, SRAM_MASK, addr, value);
}

} // namespace gba::backup::eeprom
