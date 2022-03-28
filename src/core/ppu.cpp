// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "ppu.hpp"
#include "mem.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <ranges>
#include <execution>

namespace gba::ppu {

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

auto get_mode(Gba& gba) -> std::uint8_t
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

auto render_mode0_bg(Gba& gba) -> void
{

}

auto render_mode3_bg(Gba& gba) noexcept -> void
{
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];
    std::copy(std::execution::par_unseq, std::begin(pixels), std::end(pixels), VRAM_16 + 240 * REG_VCOUNT);
}

auto render_mode4_bg(Gba& gba) noexcept -> void
{
    const auto page = bit::is_set<4>(REG_DISPCNT) ? 0xA000 : 0;
    auto addr = page + (240 * REG_VCOUNT);
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];

    std::for_each(std::execution::par, std::begin(pixels), std::end(pixels), [&gba, &addr](auto& pixel){
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
        case 0:
            render_mode0_bg(gba);
            break;

        case 3:
            render_mode3_bg(gba);
            break;

        case 4:
            render_mode4_bg(gba);
            break;

        default:
            // std::printf("unhandled ppu mode: %u\n", mode);
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
auto run(Gba& gba, uint8_t cycles) -> void
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
