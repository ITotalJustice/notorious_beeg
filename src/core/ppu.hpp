// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>

namespace gba::ppu {

enum class Period
{
    hdraw,
    hblank,
    vdraw,
    vblank,
};

struct Ppu
{
    u32 period_cycles;
    u32 cycles;
    Period period;

    // bgr555
    std::uint16_t pixels[160][240];
};

auto get_mode(Gba& gba) -> u8;
auto is_bitmap_mode(Gba & gba) -> bool;

auto on_event(Gba& gba) -> void;
auto reset(Gba& gba) -> void;
auto run(Gba& gba, u8 cycles) -> void;

} // namespace gba::ppu
