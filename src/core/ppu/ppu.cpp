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
#include "log.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <utility>

namespace gba::ppu {
namespace {

auto add_event(Gba& gba, s32 cycles)
{
    gba.scheduler.add(scheduler::ID::PPU, cycles, on_event, &gba);
}

auto update_period_cycles(Gba& gba) -> u16
{
    switch (gba.ppu.period)
    {
        case Period::hdraw: return 960;
        case Period::hblank: return 272;
        case Period::vdraw: return 960;
        case Period::vblank: return 272;
    }

    std::unreachable();
}

// called during hblank from lines 0-227
// this means that this is called during vblank as well
auto on_hblank(Gba& gba)
{
    log::print_info(gba, log::Type::PPU, "entered hblank\n");

    REG_DISPSTAT = bit::set<1>(REG_DISPSTAT);

    if (bit::is_set<4>(REG_DISPSTAT))
    {
        arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::HBlank);
    }

    if (gba.ppu.period == Period::hblank)
    {
        render(gba);
        dma::on_hblank(gba);
    }

    if (gba.hblank_callback != nullptr)
    {
        gba.hblank_callback(gba.userdata, REG_VCOUNT);
    }
}

// called on line 160
auto on_vblank(Gba& gba)
{
    log::print_info(gba, log::Type::PPU, "entered hblank\n");

    REG_DISPSTAT = bit::set<0>(REG_DISPSTAT);

    if (bit::is_set<3>(REG_DISPSTAT))
    {
        arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::VBlank);
    }

    dma::on_vblank(gba);

    if (gba.vblank_callback != nullptr)
    {
        gba.vblank_callback(gba.userdata);
    }
}

// called every time vcount is updated
auto on_vcount_update(Gba& gba, const u16 new_vcount)
{
    REG_VCOUNT = new_vcount;
    const auto lyc = bit::get_range<8, 15>(REG_DISPSTAT);

    if (REG_VCOUNT >= 2 && REG_VCOUNT <= 162)
    {
        dma::on_dma3_special(gba);
    }

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
    switch (gba.ppu.period)
    {
        case Period::hdraw:
            gba.ppu.period = Period::hblank;
            on_hblank(gba);
            break;

        case Period::hblank:
            on_vcount_update(gba, REG_VCOUNT + 1);
            REG_DISPSTAT = bit::unset<1>(REG_DISPSTAT);
            gba.ppu.period = Period::hdraw;
            if (REG_VCOUNT == 160)
            {
                gba.ppu.period = Period::vdraw;
                on_vblank(gba);
            }
            else
            {
                // incremented at the end of every visible scanline
                gba.ppu.bg2x = bit::sign_extend<27>(gba.ppu.bg2x + static_cast<s16>(REG_BG2PB));
                gba.ppu.bg2y = bit::sign_extend<27>(gba.ppu.bg2y + static_cast<s16>(REG_BG2PD));
                gba.ppu.bg3x = bit::sign_extend<27>(gba.ppu.bg3x + static_cast<s16>(REG_BG3PB));
                gba.ppu.bg3y = bit::sign_extend<27>(gba.ppu.bg3y + static_cast<s16>(REG_BG3PD));
            }
            break;

        case Period::vdraw:
            on_hblank(gba);
            gba.ppu.period = Period::vblank;
            break;

        case Period::vblank:
            on_vcount_update(gba, REG_VCOUNT + 1);
            gba.ppu.period = Period::vdraw;
            if (REG_VCOUNT == 227)
            {
                REG_DISPSTAT = bit::unset<0>(REG_DISPSTAT);
            }
            if (REG_VCOUNT == 228)
            {
                // reload the affine regs at the end of vblank
                gba.ppu.bg2x = bit::sign_extend<27>((REG_BG2X_HI << 16) | REG_BG2X_LO);
                gba.ppu.bg2y = bit::sign_extend<27>((REG_BG2Y_HI << 16) | REG_BG2Y_LO);
                gba.ppu.bg3x = bit::sign_extend<27>((REG_BG3X_HI << 16) | REG_BG3X_LO);
                gba.ppu.bg3y = bit::sign_extend<27>((REG_BG3Y_HI << 16) | REG_BG3Y_LO);

                on_vcount_update(gba, 0);
                gba.ppu.period = Period::hdraw;
            }
            break;
    }
}

// NOTE: tonc says this happens outside of vblank, but it doesn't
// matter really because when in vblank, rendering isn't happening
// and a reload happens at the end of vblank so writes can always be allowed.
auto write_BGX(s32& bgx_shadow, u16 value, bool upper_half)
{
    if (upper_half)
    {
        bgx_shadow &= 0x0000FFFF;
        bgx_shadow |= value << 16;
    }
    else
    {
        bgx_shadow &= 0xFFFF0000;
        bgx_shadow |= value;
    }
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

    gba.ppu.period = Period::hdraw;
    const auto cycles = update_period_cycles(gba);
    add_event(gba, cycles);

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

auto write_BG2X(Gba& gba, u32 addr, u16 value) -> void
{
    write_BGX(gba.ppu.bg2x, value, addr & 2);
}

auto write_BG2Y(Gba& gba, u32 addr, u16 value) -> void
{
    write_BGX(gba.ppu.bg2y, value, addr & 2);
}

auto write_BG3X(Gba& gba, u32 addr, u16 value) -> void
{
    write_BGX(gba.ppu.bg3x, value, addr & 2);
}

auto write_BG3Y(Gba& gba, u32 addr, u16 value) -> void
{
    write_BGX(gba.ppu.bg3y, value, addr & 2);
}

auto on_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);

    change_period(gba);
    const auto cycles = update_period_cycles(gba);
    add_event(gba, gba.delta.get(id, cycles));
}

} // namespace gba::ppu
