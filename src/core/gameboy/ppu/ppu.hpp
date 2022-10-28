#pragma once

#include "../types.hpp"
#include "fwd.hpp"
#include <span>

namespace gba::gb {

// defined in core/ppu/ppu.c
extern const u8 PIXEL_BIT_SHRINK[8];
extern const u8 PIXEL_BIT_GROW[8];

void on_ppu_event(void* user, s32 id, s32 late);
void write_scanline_to_frame(void* _pixels, u32 stride, u8 bpp, int x, int y, const u32 scanline[160]);
auto DMG_render_layer(Gba& gba, std::span<u16> pixels, u8 layer) -> u8;
auto GBC_render_layer(Gba& gba, std::span<u16> pixels, u8 layer) -> u8;

// data selects
auto get_bg_data_select(const Gba& gba) -> bool;
auto get_title_data_select(const Gba& gba) -> bool;
auto get_win_data_select(const Gba& gba) -> bool;
// map selects
auto get_bg_map_select(const Gba& gba) -> u16;
auto get_title_map_select(const Gba& gba) -> u16;
auto get_win_map_select(const Gba& gba) -> u16;
auto get_tile_offset(const Gba& gba, u8 tile_num, u8 sub_tile_y) -> u16;

auto vram_read(const Gba& gba, u16 addr, u8 bank) -> u8;
auto get_sprite_size(const Gba& gba) -> u8;

void on_bgp_write(Gba& gba, u8 value);
void on_obp0_write(Gba& gba, u8 value);
void on_obp1_write(Gba& gba, u8 value);

// GBC stuff
auto is_hdma_active(const Gba& gba) -> bool;
void perform_hdma(Gba& gba);

void DMG_render_scanline(Gba& gba);
void GBC_render_scanline(Gba& gba);

} // namespace gba::gb
