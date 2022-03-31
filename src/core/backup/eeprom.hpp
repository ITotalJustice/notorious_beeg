// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>
#include <span>

namespace gba::backup::eeprom
{

enum class State
{
    Command, // 2 bits
    Address, // 6-14 bits
    Data, // data bit (how many bits are written here?)
};

enum class Request : u8
{
    invalid0 = 0b00,
    invalid1 = 0b01,
    write = 0b10,
    read = 0b11,
};

enum class Width : u8
{
    unknown = 1, // assume small
    small = 6,
    beeg = 14,
};

struct Eeprom
{
public:
    u8 data[8 * 1024];
    u16 read_address; // address of read | write mode
    u16 write_address; // address of read | write mode

    u16 bits; // bits shifted in / out
    u8 bit_write_counter; // bit position
    u8 bit_read_counter; // bit position

    State state; // what state are we in
    Request request; // request type
    Width width;

    auto init(Gba& gba) -> void;
    auto set_width(Width new_width) -> void;

    auto load_data(std::span<const u8> new_data) -> bool;
    auto get_data() const -> std::span<const u8>;

    auto read(Gba& gba, std::uint32_t addr) -> u8;
    auto write(Gba& gba, std::uint32_t addr, u8 value) -> void;

private:
    auto on_state_change(State new_state) -> void;
};

} // namespace gba::backup::eeprom
