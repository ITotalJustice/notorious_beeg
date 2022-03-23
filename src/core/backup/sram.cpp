// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "eeprom.hpp"
#include "gba.hpp"
#include <algorithm>
#include <iterator>
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

// auto Sram::read(Gba& gba, u32 addr) -> u8
// {
// }

// auto Sram::write(Gba& gba, u32 addr, u8 value) -> void
// {
// }

} // namespace gba::backup::eeprom
