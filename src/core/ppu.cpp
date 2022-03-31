// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "ppu.hpp"
#include "mem.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <ranges>

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

auto add_event(Gba& gba)
{
    scheduler::add(gba, scheduler::Event::PPU, on_event, gba.ppu.period_cycles);
}

#define PPU gba.ppu

auto update_period_cycles(Gba& gba) -> void
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

template<bool Y> // 0=Y, 1=X
constexpr auto get_bg_offset(BGxCNT cnt, auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;

    constexpr u16 mod[2][4] = // [0] = Y, [1] = X
    {
        { 256, 256, 512, 512 },
        { 256, 512, 256, 512 },
    };

    constexpr u16 result[2][4] = // [0] = Y, [1] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[Y][cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[Y][cnt.Sz] * next_block;
}

template<bool Y> // 0=Y, 1=X
constexpr auto get_bg_offset_affine(BGxCNT cnt, auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;
    constexpr u16 mod[4] = { 128, 256, 512, 1024 };
    constexpr u16 result[2][4] = // [0] = Y, [1] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[Y][cnt.Sz] * next_block;
}

auto render_line_bg(Gba& gba, BGxCNT cnt, u16 xscroll, u16 yscroll)
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
                pixel = charblock[(se.tile_index * 0x20) + tile_offset];

                if (tile_x & 1) // hi or lo nibble
                {
                    pixel >>= 4;
                }
                pixel &= 0xF; // 4bpp remember
                pram_addr = 2 * pixel;
                pram_addr += se.palette_bank * 0x20;
            }
            else // BG_8BPP
            {
                pixel = charblock[(se.tile_index * 0x40) + tile_offset];
                pram_addr = 2 * pixel;
            }

            if (pixel != 0) // don't render transparent pixel
            {
                gba.ppu.pixels[REG_VCOUNT][x] = pram[pram_addr / 2];
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

auto render_backdrop(Gba& gba)
{
    std::ranges::fill(gba.ppu.pixels[REG_VCOUNT], PALETTE_16[0]);
}

constexpr auto sort_priority_set(auto& set)
{
    std::ranges::stable_sort(set, [](auto lhs, auto rhs){
        return lhs.cnt.Pr > rhs.cnt.Pr;
    });
}

auto render_set(Gba& gba, auto& set)
{
    std::ranges::for_each(set, [&gba](auto& p){
        if (p.enable) {
            render_line_bg(gba, p.cnt, p.xscroll, p.yscroll);
        }
    });
}

// 4 regular
auto render_mode0(Gba& gba) -> void
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

    // todo: obj
}

// 2 regular, 1 affine
auto render_mode1(Gba& gba) -> void
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
    // todo: obj
}

// 4 affine
auto render_mode2(Gba& gba) -> void
{
    render_backdrop(gba);
    assert(0);

    // todo: affine*4
    // todo: obj
}

auto render_mode3(Gba& gba) noexcept -> void
{
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];
    std::ranges::copy(pixels, VRAM_16 + 240 * REG_VCOUNT);
}

auto render_mode4(Gba& gba) noexcept -> void
{
    const auto page = bit::is_set<4>(REG_DISPCNT) ? 0xA000 : 0;
    auto addr = page + (240 * REG_VCOUNT);
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];

    std::ranges::for_each(pixels, [&gba, &addr](auto& pixel){
        pixel = PALETTE_16[gba.mem.vram[addr++]];
    });
}

auto render(Gba& gba)
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
auto on_hblank(Gba& gba)
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
}

// called on line 160
auto on_vblank(Gba& gba)
{
    REG_DISPSTAT = bit::set<0>(REG_DISPSTAT, true);
    if (bit::is_set<3>(REG_DISPSTAT))
    {
        arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::VBlank);
    }
    dma::on_vblank(gba);
}

// called every time vcount is updated
auto on_vcount_update(Gba& gba, uint16_t new_vcount)
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

auto change_period(Gba& gba)
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

#undef PPU

auto on_event(Gba& gba) -> void
{
    change_period(gba);
    add_event(gba);
}

} // namespace gba::ppu
