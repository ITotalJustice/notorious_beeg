// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup/sram.hpp"
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

auto Sram::load_data(std::span<const std::uint8_t> new_data) -> bool
{
    if (new_data.size() <= std::size(this->data))
    {
        std::ranges::copy(new_data, this->data);
        return true;
    }
    else
    {
        std::printf("[SRAM] tried loading bad save data: %zu\n", new_data.size());
        return false;
    }
}

auto Sram::get_data() const -> std::span<const std::uint8_t>
{
    return this->data;
}

constexpr auto SRAM_MASK = sizeof(Sram::data)-1;

auto Sram::read(Gba& gba, std::uint32_t addr) -> std::uint8_t
{
    return mem::read_array<std::uint8_t>(this->data, SRAM_MASK, addr);
}

auto Sram::write(Gba& gba, std::uint32_t addr, std::uint8_t value) -> void
{
    mem::write_array<std::uint8_t>(this->data, SRAM_MASK, addr, value);
}

} // namespace gba::backup::eeprom
