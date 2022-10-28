// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <span>

namespace gba::ppu {

enum class Period : u8
{
    draw,
    blank,
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
auto render_bg_mode(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> u8;

auto get_mode(Gba& gba) -> u8;
auto is_bitmap_mode(Gba & gba) -> bool;

// returns true if screen is forcefully blanked (off)
auto is_screen_blanked(Gba& gba) -> bool;
// returns true if in hdraw and screen isn't blanked
auto is_screen_visible(Gba& gba) -> bool;

auto on_event(void* user, s32 id, s32 late) -> void;
auto reset(Gba& gba, bool skip_bios) -> void;

auto write_BG2X(Gba& gba, u32 addr, u16 value) -> void;
auto write_BG2Y(Gba& gba, u32 addr, u16 value) -> void;
auto write_BG3X(Gba& gba, u32 addr, u16 value) -> void;
auto write_BG3Y(Gba& gba, u32 addr, u16 value) -> void;

} // namespace gba::ppu
