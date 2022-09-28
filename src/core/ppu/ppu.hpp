// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <span>

namespace gba::ppu {

enum class Period : u8
{
    hdraw,
    hblank,
    vdraw,
    vblank,
};

struct Ppu
{
    Period period;

    // these are incremented every hblank
    // and reloaded on vblank
    s32 bg2x;
    s32 bg2y;
    s32 bg3x;
    s32 bg3y;
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

STATIC auto write_BG2X(Gba& gba, u32 addr, u16 value) -> void;
STATIC auto write_BG2Y(Gba& gba, u32 addr, u16 value) -> void;
STATIC auto write_BG3X(Gba& gba, u32 addr, u16 value) -> void;
STATIC auto write_BG3Y(Gba& gba, u32 addr, u16 value) -> void;

} // namespace gba::ppu
