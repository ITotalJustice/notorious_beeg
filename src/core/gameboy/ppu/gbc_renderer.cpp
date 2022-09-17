#include "bit.hpp"
#include "gba.hpp"

#include "ppu.hpp"
#include "../internal.hpp"
#include "../gb.hpp"
#include "../types.hpp"

#include <cassert>
#include <cstring>

namespace gba::gb {
namespace {

#define GBC_PPU gba.gameboy.ppu.system.gbc

// store a 1 when bg / win writes to the screen
// this is used by obj rendering to check firstly if
// the bg always has priority.
// if it does, then it checks this buffer for a 1
// at the same xpos, if its 1, rendering that pixel is skipped.
struct GbcPrioBuf
{
    // 1 = prio, 0 = no prio
    bool prio[SCREEN_WIDTH];
    // 0-3
    u8 colour_id[SCREEN_WIDTH];
};

inline void gbc_update_colours(Gba& gba, bool dirty[8], u32 map[8][4], const u8 palette_mem[64])
{
    #if 0
    if (gba.colour_callback == nullptr)
    {
        std::memset(dirty, 0, sizeof(bool) * 8);
        return;
    }
    #endif

    for (auto palette = 0; palette < 8; palette++)
    {
        if (dirty[palette])
        {
            dirty[palette] = false;

            for (auto colours = 0, pos = 0; colours < 4; colours++, pos += 2)
            {
                const auto col_a = palette_mem[(palette * 8) + pos + 0];
                const auto col_b = palette_mem[(palette * 8) + pos + 1];
                const auto pair = (col_b << 8) | col_a;

                if (gba.colour_callback)
                {
                    map[palette][colours] = gba.colour_callback(gba.userdata, Colour(pair));
                }
                else
                {
                    map[palette][colours] = pair;
                }
            }
        }
    }
}

inline auto is_bcps_auto_increment(const Gba& gba) -> bool
{
    return (IO_BCPS & 0x80) > 0;
}

inline auto is_ocps_auto_increment(const Gba& gba) -> bool
{
    return (IO_OCPS & 0x80) > 0;
}

inline auto get_bcps_index(const Gba& gba) -> u8
{
    return IO_BCPS & 0x3F;
}

inline auto get_ocps_index(const Gba& gba) -> u8
{
    return IO_OCPS & 0x3F;
}

inline void bcps_increment(Gba& gba)
{
    if (is_bcps_auto_increment(gba))
    {
        // only increment the lower 5 bits.
        // set back the increment bit after.
        IO_BCPS = ((IO_BCPS + 1) & 0x3F) | (0xC0);
    }
}

inline void ocps_increment(Gba& gba)
{
    if (is_ocps_auto_increment(gba))
    {
        // only increment the lower 5 bits.
        // set back the increment bit after.
        IO_OCPS = ((IO_OCPS + 1) & 0x3F) | (0xC0);
    }
}

inline auto hdma_read(const Gba& gba, const u16 addr) -> u8
{
    const auto& entry = gba.gameboy.rmap[addr >> 12];
    return entry.ptr[addr & entry.mask];
}

inline void hdma_write(Gba& gba, const u16 addr, const u8 value)
{
    gba.gameboy.vram[IO_VBK][addr & 0x1FFF] = value;
}


struct GBC_BgAttribute
{
    u8 pal : 3;
    u8 bank : 1;
    bool xflip : 1;
    bool yflip : 1;
    bool prio : 1;
};

struct GBC_BgAttributes
{
    GBC_BgAttribute a[32];
};

inline auto gbc_get_bg_attr(const u8 v) -> GBC_BgAttribute
{
    return {
        .pal    = bit::get_range<0, 2>(v),
        .bank   = bit::is_set<3>(v),
        .xflip  = bit::is_set<5>(v),
        .yflip  = bit::is_set<6>(v),
        .prio   = bit::is_set<7>(v),
    };
}

inline auto gbc_fetch_bg_attr(const Gba& gba, u16 map, u8 tile_y) -> GBC_BgAttributes
{
    GBC_BgAttributes attrs;

    const auto ptr = &gba.gameboy.vram[1][(map + (tile_y * 32)) & 0x1FFF];

    for (std::size_t i = 0; i < ARRAY_SIZE(attrs.a); i++)
    {
        attrs.a[i] = gbc_get_bg_attr(ptr[i]);
    }

    return attrs;
}

struct GBC_Sprite
{
    s16 y;
    s16 x;
    u8 i;
    // gbc sprite and bg attr have the same layout
    GBC_BgAttribute a;
};

struct GBC_Sprites
{
    GBC_Sprite sprite[10];
    u8 count{};
};

inline auto gbc_sprite_fetch(const Gba& gba) -> GBC_Sprites
{
    GBC_Sprites sprites;

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
            sprite.a = gbc_get_bg_attr(gba.gameboy.oam[i + 3]);

            sprites.count++;

            // only 10 sprites per line!
            if (sprites.count == 10)
            {
                break;
            }
        }
    }

    return sprites;
}

void render_bg_gbc(Gba& gba, u32* pixels, GbcPrioBuf& prio_buf)
{
    const auto scanline = IO_LY;
    const auto base_tile_x = IO_SCX >> 3;
    const auto sub_tile_x = (IO_SCX & 7);
    const auto pixel_y = (scanline + IO_SCY) & 0xFF;
    const auto tile_y = pixel_y >> 3;
    const auto sub_tile_y = (pixel_y & 7);

    /* array maps */
    const auto vram_map = &gba.gameboy.vram[0][(get_bg_map_select(gba) + (tile_y * 32)) & 0x1FFF];
    const auto attr_map = gbc_fetch_bg_attr(gba, get_bg_map_select(gba), tile_y);

    for (auto tile_x = 0; tile_x <= 20; tile_x++)
    {
        const auto x_index_offset = (tile_x * 8) - sub_tile_x;

        // rest of the tiles won't be drawn onscreen
        // this only happens if sub_tile_x == 0
        if (x_index_offset >= SCREEN_WIDTH)
        {
            break;
        }

        /* calc the map index */
        const auto map_x = (base_tile_x + tile_x) & 31;

        /* fetch the tile number and attributes */
        const auto tile_num = vram_map[map_x];
        const auto& attr = attr_map.a[map_x];

        const auto offset = get_tile_offset(gba, tile_num, attr.yflip ? 7 - sub_tile_y : sub_tile_y);

        const auto byte_a = vram_read(gba, offset + 0, attr.bank);
        const auto byte_b = vram_read(gba, offset + 1, attr.bank);

        const auto bit = attr.xflip ? PIXEL_BIT_GROW : PIXEL_BIT_SHRINK;

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

            /* set priority */
            prio_buf.prio[x_index] = attr.prio;
            prio_buf.colour_id[x_index] = colour_id;

            const auto colour = GBC_PPU.bg_colours[attr.pal][colour_id];

            pixels[x_index] = colour;
        }
    }
}

void render_win_gbc(Gba& gba, u32* pixels, GbcPrioBuf& prio_buf, bool update_window_line = true)
{
    const auto base_tile_x = 20 - (IO_WX >> 3);
    const auto sub_tile_x = IO_WX - 7;
    const auto pixel_y = gba.gameboy.ppu.window_line;
    const auto tile_y = pixel_y >> 3;
    const auto sub_tile_y = (pixel_y & 7);

    bool did_draw = false;

    const auto vram_map = &gba.gameboy.vram[0][(get_win_map_select(gba) + (tile_y * 32)) & 0x1FFF];
    const auto attr_map = gbc_fetch_bg_attr(gba, get_win_map_select(gba), tile_y);

    for (auto tile_x = 0; tile_x <= base_tile_x; tile_x++)
    {
        const auto x_index_offset = (tile_x * 8) + sub_tile_x;

        // skip this one as it'll never be drawn
        if (x_index_offset >= SCREEN_WIDTH && x_index_offset <= 255-7)
        {
            continue;
        }

        /* fetch the tile number and attributes */
        const auto tile_num = vram_map[tile_x];
        const auto& attr = attr_map.a[tile_x];

        const auto offset = get_tile_offset(gba, tile_num, attr.yflip ? 7 - sub_tile_y : sub_tile_y);

        const auto byte_a = vram_read(gba, offset + 0, attr.bank);
        const auto byte_b = vram_read(gba, offset + 1, attr.bank);

        const auto bit = attr.xflip ? PIXEL_BIT_GROW : PIXEL_BIT_SHRINK;

        for (auto x = 0; x < 8; x++)
        {
            const auto x_index = x_index_offset + x;

            if (x_index < 0 || x_index >= SCREEN_WIDTH)
            {
                continue; // we don't break because the screen scrolls in from the right
            }

            did_draw |= true;

            const auto colour_id = ((!!(byte_b & bit[x])) << 1) | (!!(byte_a & bit[x]));

            /* set priority */
            prio_buf.prio[x_index] = attr.prio;
            prio_buf.colour_id[x_index] = colour_id;

            const auto colour = GBC_PPU.bg_colours[attr.pal][colour_id];

            pixels[x_index] = colour;
        }
    }

    if (did_draw && update_window_line)
    {
        gba.gameboy.ppu.window_line++;
    }
}

void render_obj_gbc(Gba& gba, u32* pixels, const GbcPrioBuf& prio_buf)
{
    const auto scanline = IO_LY;
    const auto sprite_size = get_sprite_size(gba);

    /* check if the bg always has prio over obj */
    const auto bg_prio = (IO_LCDC & 0x1) > 0;

    const auto sprites = gbc_sprite_fetch(gba);

    /* gbc uses oam prio rather than x-pos */
    /* so we need to keep track if a obj has already */
    /* been written to from a previous oam entry! */
    bool oam_priority[SCREEN_WIDTH]{};

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
        const auto offset = (((sprite_line) << 1) + (tile_index << 4));

        const auto byte_a = vram_read(gba, offset + 0, sprite.a.bank);
        const auto byte_b = vram_read(gba, offset + 1, sprite.a.bank);

        const auto* bit = sprite.a.xflip ? PIXEL_BIT_GROW : PIXEL_BIT_SHRINK;

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

            /* check if this has already been written to */
            /* from a previous oam entry */
            if (oam_priority[x_index])
            {
                continue;
            }

            const auto colour_id = ((!!(byte_b & bit[x])) << 1) | (!!(byte_a & bit[x]));

            /* this tests if the obj is transparrent */
            if (colour_id == 0)
            {
                continue;
            }

            if (bg_prio == 1)
            {
                /* this tests if bg always has priority over obj */
                if (prio_buf.prio[x_index] && prio_buf.colour_id[x_index])
                {
                    continue;
                }

                /* this tests if bg col 1-3 has priority, */
                /* then checks if the col is non-zero, if yes, skip */
                if (sprite.a.prio && prio_buf.colour_id[x_index])
                {
                    continue;
                }
            }

            /* save that we have already written to this xpos so the next */
            /* oam entries cannot overwrite this pixel! */
            oam_priority[x_index] = true;

            const auto colour = GBC_PPU.obj_colours[sprite.a.pal][colour_id];

            pixels[x_index] = colour;
        }
    }
}

} // namespace

void GBC_on_bcpd_update(Gba& gba)
{
    IO_BCPD = GBC_PPU.bg_palette[get_bcps_index(gba)];
}

void GBC_on_ocpd_update(Gba& gba)
{
    IO_OCPD = GBC_PPU.obj_palette[get_ocps_index(gba)];
}

void bcpd_write(Gba& gba, u8 value)
{
    const auto index = get_bcps_index(gba);

    // this is 0-7
    assert((index >> 3) <= 7);
    gba.gameboy.ppu.dirty_bg[index >> 3] |= GBC_PPU.bg_palette[index] != value;

    GBC_PPU.bg_palette[index] = value;
    bcps_increment(gba);

    GBC_on_bcpd_update(gba);
}

void ocpd_write(Gba& gba, u8 value)
{
    const auto index = get_ocps_index(gba);

    // this is 0-7
    assert((index >> 3) <= 7);
    gba.gameboy.ppu.dirty_obj[index >> 3] |= GBC_PPU.obj_palette[index] != value;

    GBC_PPU.obj_palette[index] = value;
    ocps_increment(gba);

    GBC_on_ocpd_update(gba);
}

auto is_hdma_active(const Gba& gba) -> bool
{
    return gba.gameboy.ppu.hdma_length > 0;
}

void perform_hdma(Gba& gba)
{
    assert(is_hdma_active(gba) == true);
    // perform 16-block transfer
    for (auto i = 0; i < 0x10; i++)
    {
        hdma_write(gba, gba.gameboy.ppu.hdma_dst_addr++, hdma_read(gba, gba.gameboy.ppu.hdma_src_addr++));
    }

    gba.gameboy.ppu.hdma_length -= 0x10;

    IO_HDMA5--;

    // finished!
    if (gba.gameboy.ppu.hdma_length == 0)
    {
        gba.gameboy.ppu.hdma_length = 0;

        IO_HDMA5 = 0xFF;
    }
}

auto hdma5_read(const Gba& gba) -> u8
{
    return IO_HDMA5;
}

void hdma5_write(Gba& gba, u8 value)
{
    // lower 6-bits are the length + 1 * 0x10
    const auto dma_len = ((value & 0x7F) + 1) * 0x10;

    // by checking bit-7 of value, it returns the type of dma to perform.
    const auto mode = bit::is_set<7>(value);

    enum HDMA5Mode
    {
        GDMA = 0x00,
        HDMA = 0x80
    };

    if (mode == GDMA)
    {
        // setting bit-7 = 0 whilst a HDMA is currently active
        // actually disables that transfer
        if (is_hdma_active(gba))
        {
            IO_HDMA5 = ((gba.gameboy.ppu.hdma_length >> 4) - 1) | 0x80;
            gba.gameboy.ppu.hdma_length = 0;
        }
        else
        {
            // GDMA are performed immediately
            for (auto i = 0; i < dma_len; i++)
            {
                hdma_write(gba, gba.gameboy.ppu.hdma_dst_addr++, hdma_read(gba, gba.gameboy.ppu.hdma_src_addr++));
            }

            // it's unclear if all HDMA regs are set to 0xFF post transfer,
            // HDMA5 is, but not sure about the rest.
            IO_HDMA5 = 0xFF;
        }
    }
    else
    {
        gba.gameboy.ppu.hdma_length = dma_len;

        // set that the transfer is active.
        IO_HDMA5 = value & 0x7F;
    }
}

void GBC_render_scanline(Gba& gba)
{
    GbcPrioBuf prio_buf{};
    u32 scanline[160]{};

    // update the bg colour palettes
    gbc_update_colours(gba, gba.gameboy.ppu.dirty_bg, GBC_PPU.bg_colours, GBC_PPU.bg_palette);
    // update the obj colour palettes
    gbc_update_colours(gba, gba.gameboy.ppu.dirty_obj, GBC_PPU.obj_colours, GBC_PPU.obj_palette);

    render_bg_gbc(gba, scanline, prio_buf);

    /* WX=0..166, WY=0..143 */
    if ((is_win_enabled(gba)) && (IO_WX <= 166) && (IO_WY <= 143) && (IO_WY <= IO_LY))
    {
        render_win_gbc(gba, scanline, prio_buf);
    }

    if (is_obj_enabled(gba))
    {
        render_obj_gbc(gba, scanline, prio_buf);
    }

    const auto x = 40;
    const auto y = gba.stride * (8 + IO_LY);
    write_scanline_to_frame(gba.pixels, gba.stride, gba.bpp, x, y, scanline);
}

auto GBC_render_layer(Gba& gba, std::span<u16> pixels, u8 layer) -> u8
{
    GbcPrioBuf prio_buf{};
    u32 scanline[160]{};

    switch (layer)
    {
        case 0:
            render_bg_gbc(gba, scanline, prio_buf);
            break;

        case 1:
            render_win_gbc(gba, scanline, prio_buf, false);
            break;

        case 2:
            render_obj_gbc(gba, scanline, prio_buf);
            break;
    }

    const auto x = 40;
    const auto y = gba.stride * (8);
    write_scanline_to_frame(pixels.data(), 240, 16, x, y, scanline);

    return layer;
}

#undef GBC_PPU

} // namespace gba::gb
