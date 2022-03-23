// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <array>
#include <cstdint>

// https://dillonbeliveau.com/2020/06/05/GBA-FLASH.html
namespace gba::backup::flash
{

enum class Type : std::uint32_t
{
    Flash64 = 1024 * 64,
    Flash128 = 1024 * 128,
};

enum class Command
{
    ChipID_Start = 0x90,
    ChipID_Exit = 0xF0,

    EreasePrepare = 0x80,
    EraseAll = 0x10,
    EraseSector = 0x30,

    SingleData = 0xA0,
    SetMemoryBank = 0xB0,
};

enum class State
{
    READY,
    CMD1,
    CMD2,
};

struct Flash
{
public:
    // 2 banks of 64K
    u8 data[2][1024 * 64];
    std::uint32_t mask;
    bool bank;

    Command command;
    State state;
    Type type;

    auto init(Gba& gba, Type new_type) -> void;
    auto read(Gba& gba, u32 addr) -> u8;
    auto write(Gba& gba, u32 addr, u8 value) -> void;

private:
    auto get_manufacturer_id() const -> uint8_t;
    auto get_device_id() const -> uint8_t;
};

} // namespace gba::backup::flash
