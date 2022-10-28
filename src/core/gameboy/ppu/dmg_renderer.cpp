#include "bit.hpp"
#include "gba.hpp"
#include "ppu.hpp"
#include "../internal.hpp"
#include "../gb.hpp"
#include "scheduler.hpp"

#include <cstring>
#include <cassert>
#include <utility>


namespace gba::gb {
namespace {

#define DMG_PPU gba.gameboy.ppu.system.dmg


struct DmgPrioBuf
{
    // 0-3
    u8 colour_id[SCREEN_WIDTH];
};

inline auto calculate_col_from_palette(const u8 palette, const u8 colour) -> u16
{
    return ((palette >> (colour << 1)) & 3);
}

void dmg_update_colours(struct PalCache cache[20], u32 colours[20][4], bool* dirty, const u32 pal_colours[4], const u8 palette_reg)
{
    if (!*dirty)
    {
        // reset cache so old entries don't persist
        std::memset(cache, 0, sizeof(struct PalCache) * 20);
        return;
    }

    *dirty = false;
    auto palette = palette_reg;

    for (auto i = 0; i < 20; i++)
    {
        if (cache[i].used)
        {
            // this is needed for now, see prehistorik man text intro
            *dirty = true;

            cache[i].used = false;
            palette = cache[i].pal;
        }

        for (auto j = 0; j < 4; ++j)
        {
            colours[i][j] = pal_colours[calculate_col_from_palette(palette, j)];
        }
    }
}

inline void on_dmg_palette_write(const Gba& gba, PalCache cache[20], bool* dirty, u8 palette, u8 value)
{
    *dirty |= palette != value;

    #if USE_SCHED
    if (!gba.scheduler.has_event(scheduler::ID::PPU))
    {
        return;
    }

    const auto cycles = gba.scheduler.get_event_cycles(scheduler::ID::PPU);

    assert(cycles >= 0);
    #else
    const auto cycles = gba.gameboy.ppu.next_cycles;
    #endif

    // can writes to happen even if disabled?
    // im assuming that is the case for now!
    if (get_status_mode(gba) == STATUS_MODE_TRANSFER && cycles > 12 && cycles <= 172)
    {
        const auto index = (172 - cycles) / 8;

        cache[index].used = true;
        cache[index].pal = value;
    }
}

struct DMG_SpriteAttribute
{
    u8 pal : 1; // only 0,1. not set as bool as its not a flag
    bool xflip : 1;
    bool yflip : 1;
    bool prio : 1;
};

struct DMG_Sprite
{
    s16 y;
    s16 x;
    u8 i;
    DMG_SpriteAttribute a;
};

struct DMG_Sprites
{
    DMG_Sprite sprite[10];
    u8 count;
};

inline auto dmg_get_sprite_attr(const u8 v) -> DMG_SpriteAttribute
{
    return {
        .pal    = bit::is_set<4>(v),
        .xflip  = bit::is_set<5>(v),
        .yflip  = bit::is_set<6>(v),
        .prio   = bit::is_set<7>(v),
    };
}

inline auto dmg_sprite_fetch(const Gba& gba) -> DMG_Sprites
{
    DMG_Sprites sprites{};

    const auto sprite_size = get_sprite_size(gba);
    const auto ly = IO_LY;

    for (auto i = 0; i < 0xA0; i += 4)
    {
        const auto sprite_y = gba.gameboy.oam[i + 0] - 16;

        // check if the y is in bounds!
        if (ly >= sprite_y && ly < (sprite_y + sprite_size))
        {
            auto& sprite = sprites.sprite[sprites.count];

            sprite.y = sprite_y;
            sprite.x = gba.gameboy.oam[i + 1] - 8;
            sprite.i = gba.gameboy.oam[i + 2];
            sprite.a = dmg_get_sprite_attr(gba.gameboy.oam[i + 3]);

            sprites.count++;

            // only 10 sprites per line!
            if (sprites.count == 10)
            {
                break;
            }
        }
    }

    // now we need to sort the spirtes!
    // sprites are ordered by their xpos, however, if xpos match,
    // then the conflicting sprites are sorted based on pos in oam.

    // because the sprites are already sorted via oam index, the following
    // sort preserves the index position.

    if (sprites.count > 1) // silence gcc false positive stringop-overflow
    {
        const auto runfor = sprites.count - 1; // silence gcc false positive strict-overflow
        auto unsorted = true;

        while (unsorted)
        {
            unsorted = false;

            for (auto i = 0; i < runfor; i++)
            {
                if (sprites.sprite[i].x > sprites.sprite[i + 1].x)
                {
                    const auto tmp = sprites.sprite[i];

                    sprites.sprite[i + 0] = sprites.sprite[i + 1];
                    sprites.sprite[i + 1] = tmp;

                    unsorted = true;
                }
            }

        }
    }

    return sprites;
}

void render_bg_dmg(Gba& gba, u32 pixels[160], DmgPrioBuf* prio_buf)
{
    const auto scanline = IO_LY;
    const auto base_tile_x = IO_SCX >> 3;
    const auto sub_tile_x = (IO_SCX & 7);
    const auto pixel_y = (scanline + IO_SCY) & 0xFF;
    const auto tile_y = pixel_y >> 3;
    const auto sub_tile_y = (pixel_y & 7);

    const auto bit = PIXEL_BIT_SHRINK;
    const auto vram_map = gba.gameboy.vram[0] + ((get_bg_map_select(gba) + (tile_y * 32)) & 0x1FFF);

    for (auto tile_x = 0; tile_x <= 20; tile_x++)
    {
        const auto x_index_offset = (tile_x * 8) - sub_tile_x;

        // rest of the tiles won't be drawn onscreen
        // this only happens if sub_tile_x == 0
        if (x_index_offset >= SCREEN_WIDTH)
        {
            break;
        }

        const auto map_x = (base_tile_x + tile_x) & 31;

        const auto tile_num = vram_map[map_x];
        const auto offset = get_tile_offset(gba, tile_num, sub_tile_y);

        const auto byte_a = vram_read(gba, offset + 0, 0);
        const auto byte_b = vram_read(gba, offset + 1, 0);

        for (auto x = 0; x < 8; x++)
        {
            const auto x_index = x_index_offset + x;

            if (x_index >= SCREEN_WIDTH)
            {
                break;
            }

            if (x_index < 0)
            {
                continue;
            }

            const auto colour_id = ((!!(byte_b & bit[x])) << 1) | (!!(byte_a & bit[x]));

            prio_buf->colour_id[x_index] = colour_id;

            const auto colour = DMG_PPU.bg_colours[x_index>>3][colour_id];

            pixels[x_index] = colour;
        }
    }
}

void render_win_dmg(Gba& gba, u32 pixels[160], DmgPrioBuf* prio_buf, bool update_window_line = true)
{
    const auto base_tile_x = 20 - (IO_WX >> 3);
    const auto sub_tile_x = IO_WX - 7;
    const auto pixel_y = gba.gameboy.ppu.window_line;
    const auto tile_y = pixel_y >> 3;
    const auto sub_tile_y = (pixel_y & 7);

    auto did_draw = false;

    const auto bit = PIXEL_BIT_SHRINK;
    const auto vram_map = gba.gameboy.vram[0] + ((get_win_map_select(gba) + (tile_y * 32)) & 0x1FFF);

    for (auto tile_x = 0; tile_x <= base_tile_x; tile_x++)
    {
        const auto x_index_offset = (tile_x * 8) + sub_tile_x;

        // skip this one as it'll never be drawn
        if (x_index_offset >= SCREEN_WIDTH && x_index_offset <= 255-7)
        {
            continue;
        }

        const auto tile_num = vram_map[tile_x];
        const auto offset = get_tile_offset(gba, tile_num, sub_tile_y);

        const auto byte_a = vram_read(gba, offset + 0, 0);
        const auto byte_b = vram_read(gba, offset + 1, 0);

        for (auto x = 0; x < 8; x++)
        {
            const auto x_index = x_index_offset + x;

            if (x_index < 0 || x_index >= SCREEN_WIDTH)
            {
                continue; // we don't break because the screen scrolls in from the right
            }

            did_draw |= true;

            const auto colour_id = ((!!(byte_b & bit[x])) << 1) | (!!(byte_a & bit[x]));

            prio_buf->colour_id[x_index] = colour_id;

            const auto colour = DMG_PPU.bg_colours[x_index>>3][colour_id];

            pixels[x_index] = colour;
        }
    }

    if (did_draw && update_window_line)
    {
        gba.gameboy.ppu.window_line++;
    }
}

void render_obj_dmg(Gba& gba, u32 pixels[160], const DmgPrioBuf* prio_buf)
{
    const auto scanline = IO_LY;
    const auto sprite_size = get_sprite_size(gba);

    /* keep track of when an oam entry has already been written. */
    bool oam_priority[SCREEN_WIDTH]{};

    const auto sprites = dmg_sprite_fetch(gba);

    for (auto i = 0; i < sprites.count; i++)
    {
        const auto& sprite = sprites.sprite[i];

        /* check if the sprite has a chance of being on screen */
        /* + 8 because thats the width of each sprite (8 pixels) */
        if (sprite.x == -8 || sprite.x >= SCREEN_WIDTH)
        {
            continue;
        }

        const auto sprite_line = sprite.a.yflip ? sprite_size - 1 - (scanline - sprite.y) : scanline - sprite.y;
        /* when in 8x16 size, bit-0 is ignored of the tile_index */
        const auto tile_index = sprite_size == 16 ? sprite.i & 0xFE : sprite.i;
        const auto offset = (sprite_line << 1) + (tile_index << 4);

        const auto byte_a = vram_read(gba, offset + 0, 0);
        const auto byte_b = vram_read(gba, offset + 1, 0);

        const auto bit = sprite.a.xflip ? PIXEL_BIT_GROW : PIXEL_BIT_SHRINK;

        for (auto x = 0; x < 8; x++)
        {
            const auto x_index = sprite.x + x;

            /* sprite is offscreen, exit loop now */
            if (x_index >= SCREEN_WIDTH)
            {
                break;
            }

            /* ensure that we are in bounds */
            if (x_index < 0)
            {
                continue;
            }

            if (oam_priority[x_index])
            {
                continue;
            }

            const auto colour_id = ((!!(byte_b & bit[x])) << 1) | (!!(byte_a & bit[x]));

            if (colour_id == 0)
            {
                continue;
            }

            /* handle prio */
            if (sprite.a.prio && prio_buf->colour_id[x_index])
            {
                continue;
            }

            /* keep track of the sprite pixel that was written (so we don't overlap it!) */
            oam_priority[x_index] = true;

            const auto colour = DMG_PPU.obj_colours[sprite.a.pal][x_index>>3][colour_id];

            pixels[x_index] = colour;
        }
    }
}

} // namespace

void on_bgp_write(Gba& gba, u8 value)
{
    if (!is_system_gbc(gba))
    {
        on_dmg_palette_write(gba, DMG_PPU.bg_cache, &gba.gameboy.ppu.dirty_bg[0], IO_BGP, value);
    }

    IO_BGP = value;
}

void on_obp0_write(Gba& gba, u8 value)
{
    if (!is_system_gbc(gba))
    {
        on_dmg_palette_write(gba, DMG_PPU.obj_cache[0], &gba.gameboy.ppu.dirty_obj[0], IO_OBP0, value);
    }

    IO_OBP0 = value;
}

void on_obp1_write(Gba& gba, u8 value)
{
    if (!is_system_gbc(gba))
    {
        on_dmg_palette_write(gba, DMG_PPU.obj_cache[1], &gba.gameboy.ppu.dirty_obj[1], IO_OBP1, value);
    }

    IO_OBP1 = value;
}

void DMG_render_scanline(Gba& gba)
{
    DmgPrioBuf prio_buf{};
    u32 scanline[160]{};

    // update the DMG colour palettes
    dmg_update_colours(DMG_PPU.bg_cache, DMG_PPU.bg_colours, &gba.gameboy.ppu.dirty_bg[0], gba.gameboy.palette.BG, IO_BGP);
    dmg_update_colours(DMG_PPU.obj_cache[0], DMG_PPU.obj_colours[0], &gba.gameboy.ppu.dirty_obj[0], gba.gameboy.palette.OBJ0, IO_OBP0);
    dmg_update_colours(DMG_PPU.obj_cache[1], DMG_PPU.obj_colours[1], &gba.gameboy.ppu.dirty_obj[1], gba.gameboy.palette.OBJ1, IO_OBP1);

    if (is_bg_enabled(gba)) [[likely]]
    {
        render_bg_dmg(gba, scanline, &prio_buf);

        /* WX=0..166, WY=0..143 */
        if ((is_win_enabled(gba)) && (IO_WX <= 166) && (IO_WY <= 143) && (IO_WY <= IO_LY))
        {
            render_win_dmg(gba, scanline, &prio_buf);
        }

        if (is_obj_enabled(gba)) [[likely]]
        {
            render_obj_dmg(gba, scanline, &prio_buf);
        }
    }

    const auto x = 40;
    const auto y = gba.stride * (8 + IO_LY);
    write_scanline_to_frame(gba.pixels, gba.stride, gba.bpp, x, y, scanline);
}

auto DMG_render_layer(Gba& gba, std::span<u16> pixels, u8 layer) -> u8
{
    DmgPrioBuf prio_buf{};
    u32 scanline[160]{};

    switch (layer)
    {
        case 0:
            render_bg_dmg(gba, scanline, &prio_buf);
            break;

        case 1:
            render_win_dmg(gba, scanline, &prio_buf, false);
            break;

        case 2:
            render_obj_dmg(gba, scanline, &prio_buf);
            break;
    }

    const auto x = 40;
    const auto y = gba.stride * (8);
    write_scanline_to_frame(pixels.data(), 240, 16, x, y, scanline);

    return layer;
}

#undef DMG_PPU

} // namespace gba::gb
