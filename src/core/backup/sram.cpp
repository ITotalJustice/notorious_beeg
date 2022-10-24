// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "sram.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <cstdio>
#include <cstring>

namespace gba::backup::sram {

auto Sram::init([[maybe_unused]] Gba& gba) -> void
{
    std::memset(this->data, 0xFF, sizeof(this->data));
}

auto Sram::load_data(std::span<const u8> new_data) -> bool
{
    if (new_data.size() <= std::size(this->data))
    {
        std::memcpy(this->data, new_data.data(), new_data.size());
        return true;
    }
    else
    {
        std::printf("[SRAM] tried loading bad save data: %zu\n", new_data.size());
        return false;
    }
}

auto Sram::get_data() const -> std::span<const u8>
{
    return this->data;
}

constexpr auto SRAM_MASK = sizeof(Sram::data)-1;

auto Sram::read([[maybe_unused]] Gba& gba, u32 addr) const -> u8
{
    return this->data[addr & SRAM_MASK];
}

auto Sram::write([[maybe_unused]] Gba& gba, u32 addr, u8 value) -> void
{
    this->data[addr & SRAM_MASK] = value;
    gba.backup.dirty_ram = true;
}

} // namespace gba::backup::eeprom
