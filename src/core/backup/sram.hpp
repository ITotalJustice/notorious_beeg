// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <span>

namespace gba::backup::sram {

struct Sram
{
    u8 data[0x8000];
    bool dirty; // set when ram is modified

    auto init(Gba& gba) -> void;
    auto load_data(Gba& gba, std::span<const u8> new_data) -> bool;
    [[nodiscard]] auto get_data() const -> SaveData;

    auto read(Gba& gba, u32 addr) const -> u8;
    auto write(Gba& gba, u32 addr, u8 value) -> void;

    [[nodiscard]] auto is_dirty() const -> bool { return dirty; }
    void clear_dirty_flag() { dirty = false; }
};

} // namespace gba::backup::sram
