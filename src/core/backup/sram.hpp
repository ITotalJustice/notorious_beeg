// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>
#include <span>

namespace gba::backup::sram {

struct Sram
{
    u8 data[0x8000];

    auto init(Gba& gba) -> void;
    auto load_data(std::span<const u8> new_data) -> bool;
    [[nodiscard]] auto get_data() const -> std::span<const u8>;

    auto read(Gba& gba, u32 addr) const -> u8;
    auto write(Gba& gba, u32 addr, u8 value) -> void;
};

} // namespace gba::backup::sram
