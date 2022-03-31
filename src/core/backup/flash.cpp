// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup/flash.hpp"
#include "fwd.hpp"
#include "gba.hpp"
#include <utility>
#include <algorithm>
#include <ranges>

namespace gba::backup::flash
{

auto Flash::init([[maybe_unused]] Gba& gba, Type new_type) -> void
{
    this->type = new_type;
    this->mask = std::to_underlying(new_type) - 1;
    this->bank = 0;
    this->state = State::READY;
}

auto Flash::load_data(std::span<const u8> new_data) -> bool
{
    if (new_data.size() == std::to_underlying(Type::Flash64))
    {
        std::ranges::copy(new_data, this->data);
        this->type = Type::Flash64;
        return true;
    }
    else if (new_data.size() == std::to_underlying(Type::Flash128))
    {
        std::ranges::copy(new_data, this->data);
        this->type = Type::Flash128;
        return true;
    }
    else
    {
        std::printf("[FLASH] tried loading bad save data: %zu\n", new_data.size());
        return false;
    }
}

auto Flash::get_data() const -> std::span<const u8>
{
    return this->data;
}

auto Flash::get_manufacturer_id() const -> uint8_t
{
    switch (this->type)
    {
    case Type::Flash64: return 0x32;
    case Type::Flash128: return 0x62;
    }

    std::unreachable();
}

auto Flash::get_device_id() const -> uint8_t
{
    switch (this->type)
    {
    case Type::Flash64: return 0x1B;
    case Type::Flash128: return 0x13;
    }

    std::unreachable();
}

auto Flash::read([[maybe_unused]] Gba& gba, u32 addr) -> u8
{
    if (addr == 0x0E000000)
    {
        return this->get_manufacturer_id();
    }
    else if (addr == 0x0E000001)
    {
        return this->get_device_id();
    }

    return this->data[(this->bank + addr) & this->mask];
}

auto Flash::write([[maybe_unused]] Gba& gba, u32 addr, u8 value) -> void
{
    this->data[(this->bank + addr) & this->mask] = value;
}

} // namespace gba::backup::flash
