// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "eeprom.hpp"
#include "gba.hpp"
#include "log.hpp"
#include <cassert>
#include <cstring>
#include <utility>

namespace gba::backup::eeprom {
namespace {

constexpr auto READY_BIT = 0x1;
constexpr auto READ_COUNTER_RESET = 68;

} // namespace

auto Eeprom::init([[maybe_unused]] Gba& gba) -> void
{
    this->state = State::Command;
    this->request = Request::invalid0;
    this->width = Width::unknown;
    this->bits = 0;
    this->bit_write_counter = 0;
    this->write_address = 0;
    this->read_address = 0;
    this->bit_read_counter = READ_COUNTER_RESET;
    std::memset(this->data, 0xFF, sizeof(this->data));
}

auto Eeprom::load_data(Gba& gba, std::span<const u8> new_data) -> bool
{
    if (new_data.size() == 512)
    {
        std::memcpy(this->data, new_data.data(), new_data.size());
        this->set_width(gba, Width::small);
        return true;
    }
    else if (new_data.size() == 8 * 1024)
    {
        std::memcpy(this->data, new_data.data(), new_data.size());
        this->set_width(gba, Width::beeg);
        return true;
    }
    else
    {
        log::print_error(gba, log::Type::EEPROM, "tried loading bad save data: %zu\n", new_data.size());
        return false;
    }
}

auto Eeprom::get_data() const -> SaveData
{
    SaveData save{};

    switch (this->width)
    {
        case Width::unknown:
            break;

        case Width::small:
            save.write_entry({ this->data, 512 });
            break;

        case Width::beeg:
            save.write_entry(this->data);
            break;
    }

    return save;
}

auto Eeprom::on_state_change(State new_state) -> void
{
    this->state = new_state;
    this->bits = 0;
    this->bit_write_counter = 0;
}

auto Eeprom::set_width(Gba& gba, Width new_width) -> void
{
    if (this->width == Width::unknown)
    {
        log::print_info(gba, log::Type::EEPROM, "updating width to %u\n", std::to_underlying(new_width));
        this->width = new_width;
    }
    else
    {
        if (this->width != new_width)
        {
            log::print_warn(gba, log::Type::EEPROM, "width size changed. ignoring for now... old: %u new: %u\n", std::to_underlying(this->width), std::to_underlying(new_width));
            // assert(this->width == new_width && "[EEPROM] width changed somehow");
        }
    }
}

auto Eeprom::read([[maybe_unused]] Gba& gba, [[maybe_unused]] u32 addr) -> u8
{
    // usually a game will do this to check if its in ready state
    if (this->request != Request::read)
    {
        return READY_BIT;
    }

    this->bit_read_counter--;

    // apparent the first 4 bits are ignored for some reason
    if (this->bit_read_counter >= 64) [[unlikely]]
    {
        return READY_BIT;
    }

    const auto bit = 1 << (this->bit_read_counter % 8);
    const auto value = !!(this->data[this->read_address] & bit);

    // every 8bits, increase the address
    if ((this->bit_read_counter % 8) == 0)
    {
        this->read_address++;
    }

    // once 64bits (8 bytes) has been transfered, we reset
    if (this->bit_read_counter == 0)
    {
        this->bit_read_counter = READ_COUNTER_RESET;
    }

    return value;
}

auto Eeprom::write(Gba& gba, [[maybe_unused]] u32 addr, u8 value) -> void
{
    this->bits <<= 1;
    this->bits |= value & 1; // shift in only 1 bit at a time
    this->bit_write_counter++;

    switch (this->state)
    {
        case State::Command:
            if (this->bit_write_counter == 2)
            {
                this->request = static_cast<Request>(this->bits & 0x3);
                assert(this->bits == 2 || this->bits == 3);
                this->on_state_change(State::Address);
            }
            break;

        // todo: detect address. for now assume small
        case State::Address:
            assert(this->width != Width::unknown && "unknown width with addr write. add game to database");

            if (this->bit_write_counter == std::to_underlying(this->width))
            {
                if (this->request == Request::read)
                {
                    this->read_address = this->bits * 8;
                }
                else if (this->request == Request::write)
                {
                    this->write_address = this->bits * 8;
                }
                this->on_state_change(State::Data);
            }
            break;

        case State::Data:
            if (this->request == Request::read)
            {
                assert(this->bit_write_counter == 1);
                // assert(this->bits == 0 && "invalid bit0, probably wrong eeprom size");
                this->on_state_change(State::Command);
            }
            else
            {
                if (this->bit_write_counter == 65)
                {
                    // assert(this->bits == 0 && "invalid bit0, probably wrong eeprom size");
                    this->on_state_change(State::Command);
                }
                else
                {
                    // write an entire byte at a time
                    if ((this->bit_write_counter % 8) == 0)
                    {
                        this->data[this->write_address++] = this->bits;
                        this->bits = 0;
                    }
                }
                dirty = true;
            }
            break;
    }
}

} // namespace gba::backup::eeprom
