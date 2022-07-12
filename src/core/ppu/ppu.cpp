// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// credit to tonc for all of the below info.
// most of the very detailed comments are directly copied from tonc.
#include "ppu.hpp"
#include "render.hpp"
#include "mem.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "scheduler.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>

namespace gba::ppu {
namespace {

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

// called during hblank from lines 0-227
// this means that this is called during vblank as well
auto on_hblank(Gba& gba)
{
    REG_DISPSTAT = bit::set<1>(REG_DISPSTAT);

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
auto on_vblank(Gba& gba)
{
    REG_DISPSTAT = bit::set<0>(REG_DISPSTAT);
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
auto on_vcount_update(Gba& gba, const u16 new_vcount)
{
    REG_VCOUNT = new_vcount;
    const auto lyc = bit::get_range<8, 15>(REG_DISPSTAT);

    if (REG_VCOUNT == lyc)
    {
        REG_DISPSTAT = bit::set<2>(REG_DISPSTAT);
        if (bit::is_set<5>(REG_DISPSTAT))
        {
            arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::VCount);
        }
    }
    else
    {
        REG_DISPSTAT = bit::unset<2>(REG_DISPSTAT);
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
            REG_DISPSTAT = bit::unset<1>(REG_DISPSTAT);
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
                REG_DISPSTAT = bit::unset<0>(REG_DISPSTAT);
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

} // namespace

auto get_mode(Gba& gba) -> u8
{
    return bit::get_range<0, 2>(REG_DISPCNT);
}

auto is_bitmap_mode(Gba& gba) -> bool
{
    const auto mode = get_mode(gba);
    return mode == 3 || mode == 4 || mode == 5;
}

auto is_screen_blanked(Gba& gba) -> bool
{
    return bit::is_set<7>(REG_DISPCNT);
}

auto is_screen_visible(Gba& gba) -> bool
{
    return !is_screen_blanked(gba) && gba.ppu.period == Period::hdraw;
}

auto reset(Gba& gba, bool skip_bios) -> void
{
    gba.ppu = {};

    PPU.period = Period::hdraw;
    update_period_cycles(gba);
    add_event(gba);

    if (skip_bios)
    {
        REG_DISPCNT = 0x0080;
        // ignoring this for now as it causes screen tearing
        // will have to sync up to vblank in frontend.
        // REG_VCOUNT = 126; // 0x007E
        REG_BG2PA = 0x0100;
        REG_BG2PD = 0x0100;
        REG_BG3PA = 0x0100;
        REG_BG3PD = 0x0100;
    }
}

#undef PPU

auto on_event(Gba& gba) -> void
{
    change_period(gba);
    add_event(gba);
}

} // namespace gba::ppu
