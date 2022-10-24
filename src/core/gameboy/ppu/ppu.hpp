#pragma once

#include "../types.hpp"
#include "fwd.hpp"
#include <span>

namespace gba::gb {

// defined in core/ppu/ppu.c
extern const u8 PIXEL_BIT_SHRINK[8];
extern const u8 PIXEL_BIT_GROW[8];

STATIC void on_ppu_event(void* user, s32 id, s32 late);
STATIC void write_scanline_to_frame(void* _pixels, u32 stride, u8 bpp, int x, int y, const u32 scanline[160]);
STATIC auto DMG_render_layer(Gba& gba, std::span<u16> pixels, u8 layer) -> u8;
STATIC auto GBC_render_layer(Gba& gba, std::span<u16> pixels, u8 layer) -> u8;

// data selects
STATIC auto get_bg_data_select(const Gba& gba) -> bool;
STATIC auto get_title_data_select(const Gba& gba) -> bool;
STATIC auto get_win_data_select(const Gba& gba) -> bool;
// map selects
STATIC auto get_bg_map_select(const Gba& gba) -> u16;
STATIC auto get_title_map_select(const Gba& gba) -> u16;
STATIC auto get_win_map_select(const Gba& gba) -> u16;
STATIC auto get_tile_offset(const Gba& gba, u8 tile_num, u8 sub_tile_y) -> u16;

STATIC auto vram_read(const Gba& gba, u16 addr, u8 bank) -> u8;
STATIC auto get_sprite_size(const Gba& gba) -> u8;

STATIC void on_bgp_write(Gba& gba, u8 value);
STATIC void on_obp0_write(Gba& gba, u8 value);
STATIC void on_obp1_write(Gba& gba, u8 value);

// GBC stuff
STATIC auto is_hdma_active(const Gba& gba) -> bool;
STATIC void perform_hdma(Gba& gba);

STATIC void DMG_render_scanline(Gba& gba);
STATIC void GBC_render_scanline(Gba& gba);

} // namespace gba::gb
