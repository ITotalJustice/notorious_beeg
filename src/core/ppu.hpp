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
    std::uint32_t period_cycles;
    std::uint32_t cycles;
    Period period;

    // bgr555
    std::uint16_t pixels[160][240];
};

auto get_mode(Gba& gba) -> std::uint8_t;
auto is_bitmap_mode(Gba & gba) -> bool;

auto on_event(Gba& gba) -> void;
auto reset(Gba& gba) -> void;
auto run(Gba& gba, uint8_t cycles) -> void;

} // namespace gba::ppu
