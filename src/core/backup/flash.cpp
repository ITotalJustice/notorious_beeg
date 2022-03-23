// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup.hpp"
#include "backup/flash.hpp"
#include "fwd.hpp"
#include "gba.hpp"
#include <cstdint>
#include <utility>

namespace gba::backup::flash
{

auto Flash::init(Gba& gba, Type new_type) -> void
{
    this->type = new_type;
    this->mask = std::to_underlying(new_type) - 1;
    this->bank = 0;
    this->state = State::READY;
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

auto Flash::read(Gba& gba, u32 addr) -> u8
{
    if (addr == 0x0E000000)
    {
        return this->get_manufacturer_id();
    }
    else if (addr == 0x0E000001)
    {
        return this->get_device_id();
    }

    return this->data[this->bank][addr & this->mask];
}

auto Flash::write(Gba& gba, u32 addr, u8 value) -> void
{
    this->data[this->bank][addr & this->mask] = value;
}

} // namespace gba::backup::flash
