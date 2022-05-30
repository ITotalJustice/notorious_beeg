// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup/flash.hpp"
#include "fwd.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <utility>
#include <algorithm>
#include <ranges>
#include <cassert>

namespace gba::backup::flash {

using enum Command;

auto Flash::init([[maybe_unused]] Gba& gba, Type new_type) -> void
{
    this->type = new_type;
    this->mask = std::to_underlying(new_type) - 1;
    this->bank = 0;
    this->state = State::READY;

    // unit memory is set to 0xFF
    std::ranges::fill(this->data, 0xFF);
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

auto Flash::read([[maybe_unused]] Gba& gba, u32 addr) const -> u8
{
    addr &= 0xFFFF;

    if (this->command == ChipID_Start)
    {
        if (addr == 0x00000)
        {
            return this->get_manufacturer_id();
        }
        else if (addr == 0x00001)
        {
            return this->get_device_id();
        }
    }

    return mem::read_array<u8>(this->data, this->bank + addr, 0x1FFFF);
}

auto Flash::write([[maybe_unused]] Gba& gba, u32 addr, u8 value) -> void
{
    addr &= 0xFFFF;

    switch (this->state)
    {
        case State::READY:
            if (value == 0xAA && addr == 0x5555)
            {
                this->state = State::CMD1;
            }
            // set bank
            else if (this->command == SetMemoryBank)
            {
                if (this->type == Type::Flash128)
                {
                    assert(value == 0 || value == 1);
                    value &= 0x1;
                    this->bank = BANK_SIZE * value;
                }
                else
                {
                    std::printf("[FLASH] invalid bank set in flash64\n");
                    assert(0);
                }
            }
            else if (this->command == SingleData)
            {
                mem::write_array<u8>(this->data, this->bank + addr, 0x1FFFF, value);
            }
            // there's 2 exit sequences for chipID used in different chips
            // games don't bother to detect which chip is what.
            // what they do instead is try both exit sequences.
            // so basically, it'll try the first exit seq like normal.
            // the second exit sequence is just writing 0xF0.
            else if (value != std::to_underlying(ChipID_Exit))
            {
                std::printf("[FLASH] invalid ready\n");
                assert(0);
            }
            break;

        case State::CMD1:
            if (value == 0x55 && addr == 0x2AAA)
            {
                this->state = State::CMD2;
            }
            else
            {
                std::printf("[FLASH] invalid cmd1 write to 0x%08X value: 0x%02X\n", addr, value);
                this->state = State::READY;
                assert(0);
            }
            break;

        case State::CMD2:
            if (addr == 0x5555)
            {
                this->command = static_cast<Command>(value);

                switch (this->command)
                {
                    // these are handled elsewhere
                    case ChipID_Start: break;
                    case ChipID_Exit: break;
                    case EreasePrepare: break;
                    case SingleData: break;
                    case SetMemoryBank: break;

                    case EraseAll:
                        std::ranges::fill(this->data, 0xFF);
                        break;

                    default:
                        std::printf("[FLASH] unknown command value: 0x%02X\n", value);
                        break;
                }
            }
            // 4KiB sector erase
            else if (value == std::to_underlying(EraseSector) && this->command == EreasePrepare)
            {
                const auto page = addr & 0xF000;
                for (auto i = 0; i < 0x1000; i++)
                {
                    this->data[this->bank + page + i] = 0xFF;
                }
            }
            else
            {
                assert(!"invalid 3 write command");
            }
            this->state = State::READY;
            break;
    }
}

} // namespace gba::backup::flash
