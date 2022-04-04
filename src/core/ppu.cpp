// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// credit to tonc for all of the below info.
// most of the very detailed comments are directly copied from tonc.
#include "ppu.hpp"
#include "mem.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>

namespace gba::ppu {

struct BGxCNT
{
    constexpr BGxCNT(u16 cnt) :
        Pr{static_cast<u16>(bit::get_range<0, 1>(cnt))},
        CBB{static_cast<u16>(bit::get_range<2, 3>(cnt))},
        Mos{static_cast<u16>(bit::is_set<6>(cnt))},
        CM{static_cast<u16>(bit::is_set<7>(cnt))},
        SBB{static_cast<u16>(bit::get_range<8, 12>(cnt))},
        Wr{static_cast<u16>(bit::is_set<13>(cnt))},
        Sz{static_cast<u16>(bit::get_range<14, 15>(cnt))} {}

    u16 Pr : 2;
    u16 CBB : 2;
    u16 Mos : 1;
    u16 CM : 1;
    u16 SBB : 5;
    u16 Wr : 1;
    u16 Sz : 2;
};

// NOTE: The affine screen entries are only 8 bits wide and just contain an 8bit tile index.
struct ScreenEntry
{
    constexpr ScreenEntry(u16 screen_entry) :
        tile_index{static_cast<u16>(bit::get_range<0, 9>(screen_entry))},
        hflip{static_cast<u16>(bit::is_set<10>(screen_entry))},
        vflip{static_cast<u16>(bit::is_set<11>(screen_entry))},
        palette_bank{static_cast<u16>(bit::get_range<12, 15>(screen_entry))} {}

    const u16 tile_index : 10; // max 512
    const u16 hflip : 1;
    const u16 vflip : 1;
    const u16 palette_bank : 4;
};

static auto add_event(Gba& gba)
{
    scheduler::add(gba, scheduler::Event::PPU, on_event, gba.ppu.period_cycles);
}

#define PPU gba.ppu

static auto update_period_cycles(Gba& gba) -> void
{
    switch (PPU.period)
    {
        case Period::hdraw: PPU.period_cycles = 960; break;
        case Period::hblank: PPU.period_cycles = 272; break;
        case Period::vdraw: PPU.period_cycles = 960; break;
        case Period::vblank: PPU.period_cycles = 272; break;
    }
};

auto get_mode(Gba& gba) -> u8
{
    return bit::get_range<0, 2>(REG_DISPCNT);
}

auto is_bitmap_mode(Gba & gba) -> bool
{
    const auto mode = get_mode(gba);
    return mode == 3 || mode == 4 || mode == 5;
}

auto reset(Gba& gba) -> void
{
    PPU.period = Period::hdraw;
    update_period_cycles(gba);
    add_event(gba);
}

/*
Sz-flag 	define 	(tiles)	(pixels)
00 	BG_REG_32x32 	32x32 	256x256
01 	BG_REG_64x32 	64x32 	512x256
10 	BG_REG_32x64 	32x64 	256x512
11 	BG_REG_64x64 	64x64 	512x512

Table 9.5b: affine bg sizes Sz-flag 	define 	(tiles) 	(pixels)
00 	BG_AFF_16x16 	16x16 	128x128
01 	BG_AFF_32x32 	32x32 	256x256
10 	BG_AFF_64x64 	64x64 	512x512
11 	BG_AFF_128x128 	128x128 	1024x1024
*/

template<bool Y> [[nodiscard]] // 0=Y, 1=X
static auto get_bg_offset(BGxCNT cnt, auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;

    constexpr u16 mod[2][4] = // [0] = Y, [1] = X
    {
        { 256, 256, 512, 512 },
        { 256, 512, 256, 512 },
    };

    static constexpr u16 result[2][4] = // [0] = Y, [1] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[Y][cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[Y][cnt.Sz] * next_block;
}

template<bool Y> [[nodiscard]] // 0=Y, 1=X
static auto get_bg_offset_affine(BGxCNT cnt, auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;
    constexpr u16 mod[4] = { 128, 256, 512, 1024 };
    static constexpr u16 result[2][4] = // [0] = Y, [1] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[Y][cnt.Sz] * next_block;
}

struct Attr0
{
    constexpr Attr0(u16 v) :
        Y{static_cast<s16>(bit::get_range<0, 7>(v))},
        OM{static_cast<u16>(bit::get_range<8, 9>(v))},
        GM{static_cast<u16>(bit::get_range<10, 11>(v))},
        Mos{static_cast<u16>(bit::is_set<12>(v))},
        CM{static_cast<u16>(bit::is_set<13>(v))},
        Sh{static_cast<u16>(bit::get_range<14, 15>(v))} {}

    const s16 Y : 8; // Y coordinate. Marks the top of the sprite.
    const u16 OM : 2; // (Affine) object mode. Use to hide the sprite or govern affine mode.
    const u16 GM : 2; // Gfx mode. Flags for special effects.
    const u16 Mos : 1; // Enables mosaic effect. Covered here.
    const u16 CM : 1; // Color mode. 16 colors (4bpp) if cleared; 256 colors (8bpp) if set.
    const u16 Sh : 2; // Sprite shape. This and the sprite's size (attr1{E-F}) determines the sprite's real size

    [[nodiscard]] constexpr auto is_disabled() const { return OM == 0b10; }
    [[nodiscard]] constexpr auto is_4bpp() const { return CM == 0; }
    [[nodiscard]] constexpr auto is_8bpp() const { return CM == 1; }
};

struct Attr1
{
    constexpr Attr1(u16 v) :
        X{static_cast<s16>(bit::get_range<0, 8>(v))},
        AID{static_cast<u16>(bit::get_range<9, 13>(v))},
        HF{static_cast<u16>(bit::is_set<12>(v))},
        VF{static_cast<u16>(bit::is_set<13>(v))},
        Sz{static_cast<u16>(bit::get_range<14, 15>(v))} {}

    const s16 X : 9; // X coordinate. Marks left of the sprite.
    const u16 AID : 5; // Affine index. Specifies the OAM_AFF_ENTY this sprite uses. Valid only if the affine flag (attr0{8}) is set.
    const u16 HF : 1; // Horizontal flipping flag. Used only if the affine flag (attr0) is clear; otherwise they're part of the affine index.
    const u16 VF : 1; // vertical flipping flag. Used only if the affine flag (attr0) is clear; otherwise they're part of the affine index.
    const u16 Sz : 2; // Sprite size. Kinda. Together with the shape bits (attr0{E-F}) these determine the sprite's real size.
};

struct Attr2
{
    constexpr Attr2(u16 v) :
        TID{static_cast<u16>(bit::get_range<0, 9>(v))},
        Pr{static_cast<u16>(bit::get_range<10, 11>(v))},
        PB{static_cast<u16>(bit::get_range<12, 15>(v))}{}

    const u16 TID : 10; // Base tile-index of sprite. Note that in bitmap modes this must be 512 or higher.
    const u16 Pr : 2; // Priority. Higher priorities are drawn first (and therefore can be covered by later sprites and backgrounds). Sprites cover backgrounds of the same priority, and for sprites of the same priority, the higher OBJ_ATTRs are drawn first.
    const u16 PB : 4; // Palette-bank to use when in 16-color mode. Has no effect if the color mode flag (attr0{C}) is set.
};

struct OBJ_Attr
{
    constexpr OBJ_Attr(u64 v) :
        attr0{static_cast<u16>(v >> 0)},
        attr1{static_cast<u16>(v >> 16)},
        attr2{static_cast<u16>(v >> 32)},
        fill{static_cast<s16>(v >> 48)} {}

    const Attr0 attr0; // The first attribute controls a great deal, but the most important parts are for the y coordinate, and the shape of the sprite. Also important are whether or not the sprite is transformable (an affine sprite), and whether the tiles are considered to have a bitdepth of 4 (16 colors, 16 sub-palettes) or 8 (256 colors / 1 palette).
    const Attr1 attr1; // The primary parts of this attribute are the x coordinate and the size of the sprite. The role of bits 8 to 14 depend on whether or not this is a affine sprite (determined by attr0{8}). If it is, these bits specify which of the 32 OBJ_AFFINEs should be used. If not, they hold flipping flags.
    const Attr2 attr2; // This attribute tells the GBA which tiles to display and its background priority. If it's a 4bpp sprite, this is also the place to say what sub-palette should be used.
    const s16 fill; // used for affine stuff

    [[nodiscard]] constexpr auto is_yflip() const
    {
        // flipping is only used in normal mode
        if (attr0.OM == 0b00)
        {
            return attr1.VF == true;
        }
        return false;
    }

    [[nodiscard]] constexpr auto is_xflip() const
    {
        // flipping is only used in normal mode
        if (attr0.OM == 0b00)
        {
            return attr1.HF == true;
        }
        return false;
    }

    struct [[nodiscard]] SizePair { u8 x; u8 y; };

    auto get_size() const -> SizePair
    {
        static constexpr SizePair sizes[4][4] =
        {
            { { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
            { { 16, 8 }, { 32, 8 }, { 32, 16 }, { 64, 32 } },
            { { 8, 16 }, { 8, 32 }, { 16, 32 }, { 32, 64 } },
            { /* invalid */ }
        };

        return sizes[attr0.Sh][attr1.Sz];
    }
};

static auto render_obj(Gba& gba) -> void
{
    constexpr auto CHARBLOCK_SIZE = 0x4000;

    // sprites are offset by 0x20. so sprite 1 is at 0x20, 2 is at 0x40...

    // ovram is the last 2 entries of the charblock in vram.
    // in tile modes, this allows for 1024 tiles in total.
    // in bitmap modes, this allows for 512 tiles in total.
    const auto ovram = reinterpret_cast<u8*>(gba.mem.vram + 4 * CHARBLOCK_SIZE);
    const auto pram = reinterpret_cast<u16*>(gba.mem.palette_ram + 512);
    const auto oam = reinterpret_cast<u64*>(gba.mem.oam);

    for (auto i = 0; i < 128; i++)
    {
        const OBJ_Attr obj = oam[i];

        if (obj.attr0.is_disabled())
        {
            continue;
        }

        // skip all non-normal sprites for now
        if (obj.attr0.OM != 0b00)
        {
            std::printf("skipping affine sprite\n");
            continue;
        }

        // skip any non-normal sprites for now
        if (obj.attr0.GM != 0b00)
        {
            std::printf("skipping gm sprite\n");
            continue;
        }

        const auto [xSize, ySize] = obj.get_size();
        const auto mosY = obj.is_yflip() ? (ySize - 1) - (REG_VCOUNT - obj.attr0.Y) : REG_VCOUNT - obj.attr0.Y;
        const auto yMod = mosY % 8;

        if (REG_VCOUNT >= obj.attr0.Y && REG_VCOUNT < obj.attr0.Y + ySize)
        {
            for (auto x = 0; x < xSize; x++)
            {
                // this is the index into the pixel array
                const auto pixel_x = obj.attr1.X + x;

                // check its in bounds
                if (pixel_x < 0 || pixel_x >= 240)
                {
                    continue;
                }

                const auto mosX = obj.is_xflip() ? xSize - 1 - x : x;
                const auto xMod = mosX % 8;

                assert(obj.attr0.is_4bpp());

                // thank you Kellen for the below code, i wouldn't have firgured it out otherwise.
                auto tileRowAddress = obj.attr2.TID * 32;
                tileRowAddress += (((mosY / 8 * xSize / 8)) + mosX / 8) * 32;
                tileRowAddress += yMod * 4;

                // if the adddress is out of bounds, break early
                if (tileRowAddress >= CHARBLOCK_SIZE * 2) [[unlikely]]
                {
                    break;
                }

                // in bitmap mode, only the last charblock can be used for sprites.
                if (is_bitmap_mode(gba) && tileRowAddress < CHARBLOCK_SIZE) [[unlikely]]
                {
                    continue;
                }

                auto pram_addr = 0;
                auto pixel = 0;

                if (obj.attr0.is_4bpp())
                {
                    pixel = ovram[tileRowAddress + xMod/2];

                    if (xMod & 1) // odd/even (lo/hi nibble)
                    {
                        pixel >>= 4;
                    }
                    pixel &= 0xF;
                    pram_addr = pixel * 2;
                    pram_addr += obj.attr2.PB * 32;
                }
                else
                {
                    pixel = ovram[tileRowAddress + xMod];
                    pram_addr = pixel * 2;
                    pram_addr += obj.attr2.PB * 32;
                }

                if (pixel != 0)
                {
                    gba.ppu.pixels[REG_VCOUNT][pixel_x] = pram[pram_addr / 2];
                }
            }
        }
    }
}

static auto render_line_bg(Gba& gba, std::span<u16> pixels, BGxCNT cnt, u16 xscroll, u16 yscroll)
{
    constexpr auto CHARBLOCK_SIZE = 512 * 8 * sizeof(u32); // 0x4000
    constexpr auto SCREENBLOCK_SIZE = 1024 * sizeof(u16); // 0x800

    constexpr auto BG_4BPP = 0;
    constexpr auto BG_8BPP = 1;

    //
    const auto y = (yscroll + REG_VCOUNT) % 256;
    // pal_mem
    const auto pram = reinterpret_cast<u16*>(gba.mem.palette_ram);
    // tile_mem (where the tiles/tilesets are)
    const auto charblock = reinterpret_cast<u8*>(gba.mem.vram + cnt.CBB * CHARBLOCK_SIZE);
    // se_mem (where the tilemaps are)
    const auto screenblock = reinterpret_cast<u16*>(gba.mem.vram + (cnt.SBB * SCREENBLOCK_SIZE) + get_bg_offset<0>(cnt, yscroll + REG_VCOUNT) + ((y / 8) * 64));

    // todo: better name variables at somepoint
    const auto func = [&]<bool bpp>()
    {
        for (auto x = 0; x < 240; x++)
        {
            // get tilemap (has num of the tile)
            // tile set (index this is tile num)

            const auto tx = (x + xscroll) % 256;
            const auto se_number = (tx / 8) + (get_bg_offset<1>(cnt, x + xscroll) / 2); // SE-number n = tx+ty·tw,
            const ScreenEntry se = screenblock[se_number];

            const auto tile_x = se.hflip ? 7 - (tx & 7) : tx & 7;
            const auto tile_y = se.vflip ? 7 - (y & 7) : y & 7;

            const auto tile_offset = (tile_x + (tile_y * 8)) / 2;

            auto pram_addr = 0;
            auto pixel = 0;

            // todo: don't allow access to blocks 4,5
            if constexpr(bpp == BG_4BPP)
            {
                pixel = charblock[(se.tile_index * 32) + tile_offset];

                if (tile_x & 1) // hi or lo nibble
                {
                    pixel >>= 4;
                }
                pixel &= 0xF; // 4bpp remember
                pram_addr = 2 * pixel;
                pram_addr += se.palette_bank * 32;
            }
            else // BG_8BPP
            {
                pixel = charblock[(se.tile_index * 64) + tile_offset];
                pram_addr = 2 * pixel;
            }

            if (pixel != 0) // don't render transparent pixel
            {
                pixels[x] = pram[pram_addr / 2];
            }
        }
    };

    if (cnt.CM == BG_4BPP)
    {
        func.template operator()<BG_4BPP>();
    }
    else
    {
        func.template operator()<BG_8BPP>();
    }
}

// todo: create better name
struct Set
{
    BGxCNT cnt;
    u16 xscroll;
    u16 yscroll;
    bool enable;
};

static auto render_backdrop(Gba& gba)
{
    std::ranges::fill(gba.ppu.pixels[REG_VCOUNT], PALETTE_16[0]);
}

static constexpr auto sort_priority_set(auto& set)
{
    std::ranges::stable_sort(set, [](auto lhs, auto rhs){
        return lhs.cnt.Pr > rhs.cnt.Pr;
    });
}

static auto render_set(Gba& gba, auto& set)
{
    std::ranges::for_each(set, [&gba](auto& p){
        if (p.enable) {
            render_line_bg(gba, gba.ppu.pixels[REG_VCOUNT], p.cnt, p.xscroll, p.yscroll);
        }
    });
}

// 4 regular
static auto render_mode0(Gba& gba) -> void
{
    Set set[4] =
    {
        { REG_BG3CNT, REG_BG3HOFS, REG_BG3VOFS, bit::is_set<11>(REG_DISPCNT) },
        { REG_BG2CNT, REG_BG2HOFS, REG_BG2VOFS, bit::is_set<10>(REG_DISPCNT) },
        { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT) },
        { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT) }
    };

    render_backdrop(gba);
    sort_priority_set(set);
    render_set(gba, set);

    if (bit::is_set<12>(REG_DISPCNT))
    {
        assert(bit::is_set<6>(REG_DISPCNT));
        render_obj(gba);
    }
}

// 2 regular, 1 affine
static auto render_mode1(Gba& gba) -> void
{
    Set set[2] =
    {
        { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT) },
        { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT) }
    };

    render_backdrop(gba);
    sort_priority_set(set);
    render_set(gba, set);

    // todo: affine
    if (bit::is_set<12>(REG_DISPCNT))
    {
        assert(bit::is_set<6>(REG_DISPCNT));
        render_obj(gba);
    }
}

// 4 affine
static auto render_mode2(Gba& gba) -> void
{
    render_backdrop(gba);
    assert(0);

    // todo: affine*4
    // todo: obj
}

static auto render_mode3(Gba& gba) noexcept -> void
{
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];
    std::memcpy(pixels, VRAM_16 + 240 * REG_VCOUNT, sizeof(pixels));
}

static auto render_mode4(Gba& gba) noexcept -> void
{
    const auto page = bit::is_set<4>(REG_DISPCNT) ? 0xA000 : 0;
    auto addr = page + (240 * REG_VCOUNT);
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];

    std::ranges::for_each(pixels, [&gba, &addr](auto& pixel){
        pixel = PALETTE_16[gba.mem.vram[addr++]];
    });
}

static auto render(Gba& gba)
{
    // if forced blanking is enabled, the screen is black
    if (bit::is_set<7>(REG_DISPCNT)) [[unlikely]]
    {
        std::ranges::fill(gba.ppu.pixels[REG_VCOUNT], 0);
        return;
    }

    const auto mode = get_mode(gba);

    switch (mode)
    {
        case 0: render_mode0(gba); break;
        case 1: render_mode1(gba); break;
        case 2: render_mode2(gba); break;
        case 3: render_mode3(gba); break;
        case 4: render_mode4(gba); break;
        // case 5: // only unhandled mode atm

        default:
            std::printf("unhandled ppu mode: %u\n", mode);
            break;
    }
}

// called during hblank from lines 0-227
// this means that this is called during vblank as well
static auto on_hblank(Gba& gba)
{
    REG_DISPSTAT = bit::set<1>(REG_DISPSTAT, true);

    if (bit::is_set<4>(REG_DISPSTAT))
    {
        arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::HBlank);
    }

    if (PPU.period == Period::hblank)
    {
        dma::on_hblank(gba);
        render(gba);
    }

    if (gba.hblank_callback != nullptr)
    {
        gba.hblank_callback(gba.userdata, REG_VCOUNT);
    }
}

// called on line 160
static auto on_vblank(Gba& gba)
{
    REG_DISPSTAT = bit::set<0>(REG_DISPSTAT, true);
    if (bit::is_set<3>(REG_DISPSTAT))
    {
        arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::VBlank);
    }
    dma::on_vblank(gba);

    if (gba.vblank_callback != nullptr)
    {
        gba.vblank_callback(gba.userdata, REG_VCOUNT);
    }
}

// called every time vcount is updated
static auto on_vcount_update(Gba& gba, uint16_t new_vcount)
{
    REG_VCOUNT = new_vcount;
    const auto lyc = bit::get_range<8, 15>(REG_DISPSTAT);

    if (REG_VCOUNT == lyc)
    {
        REG_DISPSTAT = bit::set<2>(REG_DISPSTAT, true);
        if (bit::is_set<5>(REG_DISPSTAT))
        {
            arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::VCount);
        }
    }
    else
    {
        REG_DISPSTAT = bit::set<2>(REG_DISPSTAT, false);
    }
}

static auto change_period(Gba& gba)
{
    switch (PPU.period)
    {
        case Period::hdraw:
            PPU.period = Period::hblank;
            on_hblank(gba);
            break;

        case Period::hblank:
            on_vcount_update(gba, REG_VCOUNT + 1);
            REG_DISPSTAT = bit::set<1>(REG_DISPSTAT, false);
            PPU.period = Period::hdraw;
            if (REG_VCOUNT == 160)
            {
                PPU.period = Period::vdraw;
                on_vblank(gba);
            }
            break;

        case Period::vdraw:
            on_hblank(gba);
            PPU.period = Period::vblank;
            break;

        case Period::vblank:
            on_vcount_update(gba, REG_VCOUNT + 1);
            PPU.period = Period::vdraw;
            if (REG_VCOUNT == 227)
            {
                REG_DISPSTAT = bit::set<0>(REG_DISPSTAT, false);
            }
            if (REG_VCOUNT == 228)
            {
                on_vcount_update(gba, 0);
                PPU.period = Period::hdraw;
            }
            break;
    }

    update_period_cycles(gba);
}

#if ENABLE_SCHEDULER == 0
auto run(Gba& gba, u8 cycles) -> void
{
    PPU.cycles += cycles;

    if (PPU.cycles >= PPU.period_cycles)
    {
        PPU.cycles -= PPU.period_cycles;
        change_period(gba);
    }
}
#endif

auto render_bg_mode(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> void
{
    if (mode == 0)
    {
        const Set set[4] =
        {
            { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT) },
            { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT) },
            { REG_BG2CNT, REG_BG2HOFS, REG_BG2VOFS, bit::is_set<10>(REG_DISPCNT) },
            { REG_BG3CNT, REG_BG3HOFS, REG_BG3VOFS, bit::is_set<11>(REG_DISPCNT) },
        };

        render_line_bg(gba, pixels, set[layer].cnt, set[layer].xscroll, set[layer].yscroll);
    }
    if (mode == 1)
    {
        const Set set[2] =
        {
            { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT) },
            { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT) },
        };

        render_line_bg(gba, pixels, set[layer].cnt, set[layer].xscroll, set[layer].yscroll);
    }
}

#undef PPU

auto on_event(Gba& gba) -> void
{
    change_period(gba);
    add_event(gba);
}

} // namespace gba::ppu
