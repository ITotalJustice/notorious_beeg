// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <span>

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
    u16 pixels[160][240];
};

// used for debugging
STATIC auto render_bg_mode(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> u8;

STATIC auto get_mode(Gba& gba) -> u8;
STATIC auto is_bitmap_mode(Gba & gba) -> bool;

// returns true if screen is forcefully blanked (off)
STATIC auto is_screen_blanked(Gba& gba) -> bool;
// returns true if in hdraw and screen isn't blanked
STATIC auto is_screen_visible(Gba& gba) -> bool;

STATIC auto on_event(Gba& gba) -> void;
STATIC auto reset(Gba& gba, bool skip_bios) -> void;

} // namespace gba::ppu
