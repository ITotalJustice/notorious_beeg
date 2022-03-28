// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>
#include <span>

namespace gba::backup::sram
{

struct Sram
{
    uint8_t data[0x8000];
    // used for writing to on init so that this is the most
    // recently written union'd data, meaning it's safe to then
    // read from said data.
    bool dummy_union_write;

    auto init(Gba& gba) -> void;
    auto load_data(std::span<const std::uint8_t> new_data) -> bool;
    auto get_data() const -> std::span<const std::uint8_t>;

    auto read(Gba& gba, std::uint32_t addr) -> std::uint8_t;
    auto write(Gba& gba, std::uint32_t addr, std::uint8_t value) -> void;
};

} // namespace gba::backup::sram
