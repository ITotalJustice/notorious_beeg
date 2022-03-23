// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>
#include <array>
#include <span>

namespace gba::backup::eeprom
{

enum class State
{
    Command, // 2 bits
    Address, // 6-14 bits
    Data, // data bit (how many bits are written here?)
};

enum class Request : std::uint8_t
{
    invalid0 = 0b00,
    invalid1 = 0b01,
    write = 0b10,
    read = 0b11,
};

enum class Width : std::uint8_t
{
    unknown = 1, // assume small
    small = 6,
    beeg = 14,
};

struct Eeprom
{
public:
    // std::array<std::uint8_t, 8 * 1024> data;
    std::uint8_t data[8 * 1024];
    std::uint16_t read_address; // address of read | write mode
    std::uint16_t write_address; // address of read | write mode

    std::uint16_t bits; // bits shifted in / out
    std::uint8_t bit_write_counter; // bit position
    std::uint8_t bit_read_counter; // bit position

    State state; // what state are we in
    Request request; // request type
    Width width;

    auto init(Gba& gba) -> void;
    auto set_width(Width new_width) -> void;

    auto load_data(std::span<const std::uint8_t> new_data) -> void;
    auto get_data() const -> std::span<const std::uint8_t>;

    auto read(Gba& gba, u32 addr) -> u8;
    auto write(Gba& gba, u32 addr, u8 value) -> void;

private:
    auto on_state_change(State new_state) -> void;
};

} // namespace gba::backup::eeprom
