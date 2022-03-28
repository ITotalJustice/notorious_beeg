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
    std::uint8_t data[1024 * 64 * 2];
    std::uint32_t mask;
    std::uint32_t bank; // 0 or 1024*64

    Command command;
    State state;
    Type type;

    auto init(Gba& gba, Type new_type) -> void;
    auto load_data(std::span<const std::uint8_t> new_data) -> bool;
    auto get_data() const -> std::span<const std::uint8_t>;

    auto read(Gba& gba, std::uint32_t addr) -> std::uint8_t;
    auto write(Gba& gba, std::uint32_t addr, std::uint8_t value) -> void;

private:
    auto get_manufacturer_id() const -> uint8_t;
    auto get_device_id() const -> uint8_t;
};

} // namespace gba::backup::flash
