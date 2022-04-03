// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <array>
#include <cstdint>
#include <span>

// https://dillonbeliveau.com/2020/06/05/GBA-FLASH.html
namespace gba::backup::flash
{

constexpr auto BANK_SIZE = 1024 * 64;

enum class Type : u32
{
    Flash64 = BANK_SIZE,
    Flash128 = BANK_SIZE * 2,
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
    u8 data[BANK_SIZE * 2];
    u32 mask;
    u32 bank; // 0 or 1024*64

    Command command;
    State state;
    Type type;

    auto init(Gba& gba, Type new_type) -> void;
    auto load_data(std::span<const u8> new_data) -> bool;
    auto get_data() const -> std::span<const u8>;

    auto read(Gba& gba, u32 addr) -> u8;
    auto write(Gba& gba, u32 addr, u8 value) -> void;

private:
    auto get_manufacturer_id() const -> u8;
    auto get_device_id() const -> u8;
};

} // namespace gba::backup::flash
