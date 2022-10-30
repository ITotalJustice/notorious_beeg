// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "S98WS512PE0.hpp"
#include "gba.hpp"
#include <type_traits>
#include <cstdio>
#include <cstring>

namespace gba::flash::s98 {
namespace {

constexpr auto ADDR_5555 = 0x555 * 2;
constexpr auto ADDR_2AAA = 0x2AA * 2;

constexpr auto BANK_SIZE = 1024 * 64;
constexpr auto MANUFACTURER_ID = 0x22;
constexpr auto DEVICE_ID = 0x3D;
constexpr auto SECTOR_SIZE = 0x8000;

enum Command : u8
{
    ChipID_Start = 0x90,
    ChipID_Exit = 0xF0,

    EreasePrepare = 0x80,
    EraseAll = 0x10,
    EraseSector = 0x30,

    SingleData = 0xA0,
    SetMemoryBank = 0xB0,

    // ezflash
    WriteBufferLoadCommand = 0x25,
    WriteConfirmCommand = 0x29,
    UnknownC0 = 0xC0,
};

enum State : u8
{
    READY,
    CMD1,
    CMD2,

    BUFFER,
};

inline auto read8(const u8* data) -> u8
{
    return data[0];
}

inline auto read16(const u8* data) -> u16
{
    u16 result{};

    result |= data[0] << 0;
    result |= data[1] << 8;

    return result;
}

inline auto read32(const u8* data) -> u32
{
    u32 result{};

    result |= data[0] << 0;
    result |= data[1] << 8;
    result |= data[2] << 16;
    result |= data[3] << 24;

    return result;
}

inline auto write8(u8* data, u8 value) -> void
{
    data[0] = value;
}

inline auto write16(u8* data, u16 value) -> void
{
    data[0] = value >> 0;
    data[1] = value >> 8;
}

inline auto write32(u8* data, u32 value) -> void
{
    data[0] = value >> 0;
    data[1] = value >> 8;
    data[2] = value >> 16;
    data[3] = value >> 24;
}

} // namespace

auto S98WS512PE0::init() -> void
{
    this->bank = 0;
    this->state = State::READY;

    // unit memory is set to 0xFF
    std::memset(this->flash, 0xFF, sizeof(this->flash));
    std::memset(this->ram, 0xFF, sizeof(this->ram));
}

auto S98WS512PE0::load_data(std::span<const u8> new_data) -> bool
{
    if (new_data.size() == sizeof(this->flash))
    {
        std::memcpy(this->flash, new_data.data(), new_data.size());
        return true;
    }
    else
    {
        std::printf("[S98WS512PE0] tried loading bad save data: %zu\n", new_data.size());
        return false;
    }
}

auto S98WS512PE0::get_data() const -> std::span<const u8>
{
    return this->flash;
}

template<typename T>
auto S98WS512PE0::read_flash_internal(u32 addr) const -> T
{
    if (this->command == Command::ChipID_Start)
    {
        if constexpr(std::is_same<T, u8>())
        {
            assert(0);
            return 0x0;
        }
        if constexpr(std::is_same<T, u16>() || std::is_same<T, u32>())
        {
            return (MANUFACTURER_ID << 8) | DEVICE_ID;
        }
    }

    if constexpr(std::is_same<T, u8>())
    {
        return read8(this->flash + this->bank + addr);
    }
    if constexpr(std::is_same<T, u16>())
    {
        return read16(this->flash + this->bank + addr);
    }
    if constexpr(std::is_same<T, u32>())
    {
        return read32(this->flash + this->bank + addr);
    }
}

template<typename T>
void S98WS512PE0::write_flash_internal(u32 addr, T value)
{
    switch (this->state)
    {
        case State::READY:
            if (value == 0xAA && (addr & 0xFFFF) == ADDR_5555)
            {
                this->state = State::CMD1;
            }
            // there's 2 exit sequences for chipID used in different chips
            // games don't bother to detect which chip is what.
            // what they do instead is try both exit sequences.
            // so basically, it'll try the first exit seq like normal.
            // the second exit sequence is just writing 0xF0.
            else if ((addr & 0xFFFF) == 0x0 && value == Command::ChipID_Exit)
            {
                // std::printf("[S98WS512PE0] invalid ready addr: 0x%04X value: 0x%04X\n", addr, value);
                // assert(0);
            }
            // set bank
            else if (this->command == Command::SetMemoryBank)
            {
                constexpr auto max_bank = sizeof(this->flash) / BANK_SIZE;

                if (value > max_bank)
                {
                    std::printf("[S98WS512PE0] invalid bank set in flash64\n");
                    assert(0);
                    this-> bank = 0;
                }
                else
                {
                    this->bank = (BANK_SIZE * value);
                }
            }
            else if (this->command == Command::SingleData)
            {
                if constexpr(std::is_same<T, u8>())
                {
                    write8(this->flash + this->bank + addr, value);
                }
                if constexpr(std::is_same<T, u16>())
                {
                    write16(this->flash + this->bank + addr, value);
                }
                if constexpr(std::is_same<T, u32>())
                {
                    write32(this->flash + this->bank + addr, value);
                }
            }
            else if (this->command == Command::UnknownC0)
            {
                // do nothing for now
                if (value != 0x80 && value != 0x30 && value != 0x90 && value != 0x00)
                {
                    std::printf("[S98WS512PE0] bad value: 0x%08X v: 0x%04X\n", addr, value);
                }

                assert(value == 0x80 || value == 0x30 || value == 0x90 || value == 0x00);
            }
            else
            {
                std::printf("[S98WS512PE0] invalid ready addr: 0x%04X value: 0x%04X\n", addr, value);
                assert(0);
            }
            break;

        case State::CMD1:
            if (value == 0x55 && (addr & 0xFFFF) == ADDR_2AAA)
            {
                this->state = State::CMD2;
            }
            else
            {
                std::printf("[S98WS512PE0] invalid cmd1 write to 0x%08X value: 0x%02X\n", addr, value);
                this->state = State::READY;
                assert(0);
            }
            break;

        case State::CMD2:
            if ((addr & 0xFFFF) == ADDR_5555)
            {
                this->command = value;

                switch (this->command)
                {
                    // these are handled elsewhere
                    case Command::ChipID_Start: break;
                    case Command::ChipID_Exit: break;
                    case Command::EreasePrepare: break;
                    case Command::SingleData: break;
                    case Command::SetMemoryBank: break;
                    case Command::UnknownC0: break;

                    case Command::EraseAll:
                        std::printf("[S98WS512PE0] eraseAll\n");
                        std::memset(this->flash, 0xFF, sizeof(this->flash));
                        break;

                    default:
                        std::printf("[S98WS512PE0] unknown command value: 0x%02X\n", value);
                        break;
                }
            }
            // sector erase
            else if (value == Command::EraseSector && this->command == Command::EreasePrepare)
            {
                std::printf("[S98WS512PE0] sector erase addr: 0x%08X\n", addr);
                const auto page = addr & ~(SECTOR_SIZE - 1);
                std::memset(this->flash + page, 0xFF, SECTOR_SIZE);
            }
            else if (value == Command::WriteBufferLoadCommand)
            {
                this->command = Command::WriteBufferLoadCommand;
                return;
            }
            else if (this->command == Command::WriteBufferLoadCommand)
            {
                this->state = State::BUFFER;
                this->buffer_count = value + 1;
                return;
            }
            else
            {
                assert(!"[S98WS512PE0] invalid 3 write command");
            }
            this->state = State::READY;
            break;

        case State::BUFFER:
            if (this->buffer_count)
            {
                this->buffer_count--;

                if constexpr(std::is_same<T, u8>())
                {
                    write8(this->flash + this->bank + addr, value);
                }
                if constexpr(std::is_same<T, u16>())
                {
                    write16(this->flash + this->bank + addr, value);
                }
                if constexpr(std::is_same<T, u32>())
                {
                    write32(this->flash + this->bank + addr, value);
                }
            }
            else
            {
                assert(value == 0x29);
                this->state = State::READY;
            }
            break;
    }
}

void S98WS512PE0::write_flash_data(u32 addr, const void* data, u32 size)
{
    const auto u8_data = static_cast<const u8*>(data);

    this->write_flash<u16>(0x0, ChipID_Exit);
    this->write_flash<u16>(ADDR_5555, 0xAA);
    this->write_flash<u16>(ADDR_2AAA, 0x55);
    this->write_flash<u16>(ADDR_5555, SingleData);

    for (u32 i = 0; i < size; i++)
    {
        this->write_flash<u8>(addr + i, u8_data[i]);
    }

    this->write_flash<u16>(0x0, ChipID_Exit);
}

void S98WS512PE0::read_flash_data(u32 addr, void* data, u32 size) const
{
    auto u8_data = static_cast<u8*>(data);

    for (u32 i = 0; i < size; i++)
    {
        u8_data[i] = this->read_flash<u8>(addr + i);
    }
}

template<typename T>
auto S98WS512PE0::read_ram_internal(u32 addr) const -> T
{
    if constexpr(std::is_same<T, u8>())
    {
        return read8(this->ram + addr);
    }
    if constexpr(std::is_same<T, u16>())
    {
        return read16(this->ram + addr);
    }
    if constexpr(std::is_same<T, u32>())
    {
        return read32(this->ram + addr);
    }
}

template<typename T>
void S98WS512PE0::write_ram_internal(u32 addr, T value)
{
    if constexpr(std::is_same<T, u8>())
    {
        write8(this->ram + addr, value);
    }
    if constexpr(std::is_same<T, u16>())
    {
        write16(this->ram + addr, value);
    }
    if constexpr(std::is_same<T, u32>())
    {
        write32(this->ram + addr, value);
    }
}

template<> auto S98WS512PE0::read_flash<u8>(u32 addr) const -> u8
{
    return read_flash_internal<u8>(addr);
}
template<> auto S98WS512PE0::read_flash<u16>(u32 addr) const -> u16
{
    return read_flash_internal<u16>(addr);
}
template<> auto S98WS512PE0::read_flash<u32>(u32 addr) const -> u32
{
    return read_flash_internal<u32>(addr);
}

template<> void S98WS512PE0::write_flash<u8>(u32 addr, u8 value)
{
    return write_flash_internal<u8>(addr, value);
}
template<> void S98WS512PE0::write_flash<u16>(u32 addr, u16 value)
{
    return write_flash_internal<u16>(addr, value);
}
template<> void S98WS512PE0::write_flash<u32>(u32 addr, u32 value)
{
    return write_flash_internal<u32>(addr, value);
}

template<> auto S98WS512PE0::read_ram<u8>(u32 addr) const -> u8
{
    return read_ram_internal<u8>(addr);
}
template<> auto S98WS512PE0::read_ram<u16>(u32 addr) const -> u16
{
    return read_ram_internal<u16>(addr);
}
template<> auto S98WS512PE0::read_ram<u32>(u32 addr) const -> u32
{
    return read_ram_internal<u32>(addr);
}

template<> void S98WS512PE0::write_ram<u8>(u32 addr, u8 value)
{
    return write_ram_internal<u8>(addr, value);
}
template<> void S98WS512PE0::write_ram<u16>(u32 addr, u16 value)
{
    return write_ram_internal<u16>(addr, value);
}
template<> void S98WS512PE0::write_ram<u32>(u32 addr, u32 value)
{
    return write_ram_internal<u32>(addr, value);
}

} // namespace gba::flash::s98
