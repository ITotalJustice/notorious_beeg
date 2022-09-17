#include "bit.hpp"
#include "gba.hpp"
#include "ppu.hpp"
#include "../internal.hpp"
#include "../gb.hpp"
#include "scheduler.hpp"

#include <cstring>
#include <cassert>

namespace gba::gb {
namespace {

void stat_interrupt_update(Gba& gba)
{
    const auto mode = get_status_mode(gba);

    // SOURCE: https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf
    if (
        ((IO_LY == IO_LYC) && (IO_STAT & STAT_INT_MODE_COINCIDENCE)) ||
        ((mode == STATUS_MODE_HBLANK) && (IO_STAT & STAT_INT_MODE_0)) ||
        ((mode == STATUS_MODE_SPRITE) && (IO_STAT & STAT_INT_MODE_2)) ||
        // TCAGBD says that oam and vblank int enable flags are checked here...
        ((mode == STATUS_MODE_VBLANK) && (IO_STAT & (STAT_INT_MODE_1 | STAT_INT_MODE_2)))
    ) {
        // the interrupt is only fired if the line is low.
        // SEE: https://github.com/ITotalJustice/TotalGB/issues/50
        if (!gba.gameboy.ppu.stat_line)
        {
            enable_interrupt(gba, INTERRUPT_LCD_STAT);
        }

        // line goes high (or remains high) after.
        gba.gameboy.ppu.stat_line = true;
    }
    else
    {
        // line goes low
        gba.gameboy.ppu.stat_line = false;
    }
}

void change_status_mode(Gba& gba, const u8 new_mode)
{
    set_status_mode(gba, new_mode);

    // this happens on every mode switch because going to transfer
    // mode will set the stat_line=0 (unless LY==LYC)
    stat_interrupt_update(gba);

    #if USE_SCHED
    // TODO: check what the timing should actually be for ppu modes!
    switch (new_mode)
    {
        case STATUS_MODE_HBLANK:
            gba.gameboy.ppu.next_cycles = 204;
            draw_scanline(gba);

            if (gba.hblank_callback != nullptr)
            {
                gba.hblank_callback(gba.userdata, IO_LY);
            }
            break;

        case STATUS_MODE_VBLANK:
            enable_interrupt(gba, INTERRUPT_VBLANK);
            gba.gameboy.ppu.next_cycles = 456;

            if (gba.vblank_callback != nullptr)
            {
                gba.vblank_callback(gba.userdata);
            }
            break;

        case STATUS_MODE_SPRITE:
            gba.gameboy.ppu.next_cycles = 80;
            break;

        case STATUS_MODE_TRANSFER:
            gba.gameboy.ppu.next_cycles = 172;
            break;
    }
    #else
    switch (new_mode)
    {
        case STATUS_MODE_HBLANK:
            gba.gameboy.ppu.next_cycles += 204;
            draw_scanline(gba);

            if (gba.gameboy.callback.hblank != nullptr)
            {
                gba.gameboy.callback.hblank(gba.gameboy.callback.user_hblank);
            }
            break;

        case STATUS_MODE_VBLANK:
            enable_interrupt(gba, INTERRUPT_VBLANK);
            gba.gameboy.ppu.next_cycles += 456;

            if (gba.gameboy.callback.vblank != nullptr)
            {
                gba.gameboy.callback.vblank(gba.gameboy.callback.user_vblank);
            }
            break;

        case STATUS_MODE_SPRITE:
            gba.gameboy.ppu.next_cycles += 80;
            break;

        case STATUS_MODE_TRANSFER:
            gba.gameboy.ppu.next_cycles += 172;
            break;
    }
    #endif
}

void on_lcd_disable(Gba& gba)
{
    // this *should* only happen in vblank!
    if (get_status_mode(gba) != STATUS_MODE_VBLANK)
    {
        gb_log("[PPU-WARN] game is disabling lcd outside vblank: 0x%0X\n", get_status_mode(gba));
    }

    // in progress hdma should be stopped when ppu is turned off
    if (is_system_gbc(gba))
    {
        gba.gameboy.ppu.hdma_length = 0;
        IO_HDMA5 = 0xFF;
    }

    IO_LY = 0;
    IO_STAT &= ~(0x3);
    gba.gameboy.ppu.stat_line = false;

    #if USE_SCHED
    scheduler::remove(gba, scheduler::Event::PPU);
    #endif

    gb_log("disabling ppu...\n");
}

void on_lcd_enable(Gba& gba)
{
    gb_log("enabling ppu!\n");

    gba.gameboy.ppu.next_cycles = 0;
    gba.gameboy.ppu.stat_line = false;
    set_status_mode(gba, STATUS_MODE_TRANSFER);
    compare_LYC(gba);
    #if USE_SCHED
    // scheduler::add(gba, scheduler::Event::PPU, on_ppu_event, gba.gameboy.ppu.next_cycles);
    scheduler::add(gba, scheduler::Event::PPU, on_ppu_event, gba.gameboy.ppu.next_cycles);
    #endif
}

} // namespace

// these are extern, used for dmg / gbc / sgb render functions
const u8 PIXEL_BIT_SHRINK[] =
{
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

const u8 PIXEL_BIT_GROW[] =
{
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

void on_ppu_event(Gba& gba)
{
    #if USE_SCHED
    switch (get_status_mode(gba))
    {
        case STATUS_MODE_HBLANK:
            IO_LY++;
            compare_LYC(gba);

            if (is_hdma_active(gba))
            {
                perform_hdma(gba);
            }

            if (IO_LY == 144) [[unlikely]]
            {
                change_status_mode(gba, STATUS_MODE_VBLANK);
            }
            else
            {
                change_status_mode(gba, STATUS_MODE_SPRITE);
            }
            break;

        case STATUS_MODE_VBLANK:
            IO_LY++;

            // there is a bug in which on line 153, LY=153 only lasts
            // for 4-Tcycles.
            if (IO_LY == 153)
            {
                gba.gameboy.ppu.next_cycles = 4;
                compare_LYC(gba);
            }
            else if (IO_LY == 154)
            {
                gba.gameboy.ppu.next_cycles = 452;
                IO_LY = 0;
                gba.gameboy.ppu.window_line = 0;
                compare_LYC(gba);
            }
            else if (IO_LY == 1)
            {
                IO_LY = 0;
                change_status_mode(gba, STATUS_MODE_SPRITE);
            }
            else
            {
                gba.gameboy.ppu.next_cycles = 456;
                compare_LYC(gba);
            }
            break;

        case STATUS_MODE_SPRITE:
            change_status_mode(gba, STATUS_MODE_TRANSFER);
            break;

        case STATUS_MODE_TRANSFER:
            change_status_mode(gba, STATUS_MODE_HBLANK);
            break;
    }

    scheduler::add(gba, scheduler::Event::PPU, on_ppu_event, gba.gameboy.ppu.next_cycles);
    #endif
}

void write_scanline_to_frame(void* _pixels, u32 stride, u8 bpp, int x, int y, const u32 scanline[160])
{
    if (!_pixels)
    {
        assert(!"no pixels found!");
        return;
    }

    const auto func = [&scanline, x, y](auto pixels) {
        // pixels = &pixels[(stride * (8 + IO_LY)) + 40];
        pixels = &pixels[y + x];

        for (auto i = 0; i < SCREEN_WIDTH; i++)
        {
            pixels[i] = scanline[i];
        }
    };

    switch (bpp)
    {
        case 1:
        case 8:
            func(static_cast<u8*>(_pixels));
            break;

        case 2:
        case 15:
        case 16:
            func(static_cast<u16*>(_pixels));
            break;

        case 4:
        case 24:
        case 32:
            func(static_cast<u32*>(_pixels));
            break;

        default:
            assert(!"bpp invalid option!");
            break;
    }
}

auto vram_read(const Gba& gba, const u16 addr, const u8 bank) -> u8
{
    assert(bank < 2);
    return gba.gameboy.vram[bank][addr & 0x1FFF];
}

// data selects
auto get_bg_data_select(const Gba& gba) -> bool
{
    return bit::is_set<3>(IO_LCDC);
}

auto get_title_data_select(const Gba& gba) -> bool
{
    return bit::is_set<4>(IO_LCDC);
}

auto get_win_data_select(const Gba& gba) -> bool
{
    return bit::is_set<6>(IO_LCDC);
}

// map selects
auto get_bg_map_select(const Gba& gba) -> u16
{
    return get_bg_data_select(gba) ? 0x9C00 : 0x9800;
}

auto get_title_map_select(const Gba& gba) -> u16
{
    return get_title_data_select(gba) ? 0x8000 : 0x9000;
}

auto get_win_map_select(const Gba& gba) -> u16
{
    return get_win_data_select(gba) ? 0x9C00 : 0x9800;
}

auto get_tile_offset(const Gba& gba, const u8 tile_num, const u8 sub_tile_y) -> u16
{
    return (get_title_map_select(gba) + (((get_title_data_select(gba) ? tile_num : (int8_t)tile_num)) * 16) + (sub_tile_y << 1));
}

auto get_sprite_size(const Gba& gba) -> u8
{
    return bit::is_set<2>(IO_LCDC) ? 16 : 8;
}

void update_all_colours_gb(Gba& gba)
{
    for (auto i = 0; i < 8; ++i)
    {
        gba.gameboy.ppu.dirty_bg[i] = true;
        gba.gameboy.ppu.dirty_obj[i] = true;
    }
}

void set_coincidence_flag(Gba& gba, const bool n)
{
    IO_STAT = bit::set<2>(IO_STAT, n);
}

void set_status_mode(Gba& gba, u8 mode)
{
    IO_STAT &= ~0x3;
    IO_STAT |= mode;
}

auto get_status_mode(const Gba& gba) -> u8
{
    return bit::get_range<0, 1>(IO_STAT);
}

auto is_lcd_enabled(const Gba& gba) -> bool
{
    return bit::is_set<7>(IO_LCDC);
}

auto is_win_enabled(const Gba& gba) -> bool
{
    return bit::is_set<5>(IO_LCDC);
}

auto is_obj_enabled(const Gba& gba) -> bool
{
    return bit::is_set<1>(IO_LCDC);
}

auto is_bg_enabled(const Gba& gba) -> bool
{
    return bit::is_set<0>(IO_LCDC);
}

void compare_LYC(Gba& gba)
{
    if (IO_LY == IO_LYC) [[unlikely]]
    {
        set_coincidence_flag(gba, true);
    }
    else
    {
        set_coincidence_flag(gba, false);
    }

    stat_interrupt_update(gba);
}

void on_stat_write(Gba& gba, u8 value)
{
    // keep the read-only bits!
    IO_STAT = (IO_STAT & 0x7) | (value & 0x78);

    // interrupt for our current mode
    if (is_lcd_enabled(gba))
    {
        // stat_interrupt_update(gba);
        // this will internally call stat_interrupt_update!!!
        compare_LYC(gba);
    }
}

void on_lcdc_write(Gba& gba, const u8 value)
{
    // check if the game wants to disable the ppu
    if (is_lcd_enabled(gba) && (value & 0x80) == 0)
    {
        on_lcd_disable(gba);
    }

    // check if the game wants to re-enable the lcd
    else if (!is_lcd_enabled(gba) && (value & 0x80))
    {
        on_lcd_enable(gba);
    }

    IO_LCDC = value;
}

#if !USE_SCHED
void ppu_run(Gba& gba, u8 cycles)
{
    if (!is_lcd_enabled(gba)) [[unlikely]]
    {
        return;
    }

    gba.gameboy.ppu.next_cycles -= cycles;

    if (gba.gameboy.ppu.next_cycles > 0) [[unlikely]]
    {
        return;
    }

    switch (get_status_mode(gba))
    {
        case STATUS_MODE_HBLANK:
            IO_LY++;
            compare_LYC(gba);

            if (is_hdma_active(gba))
            {
                perform_hdma(gba);
            }

            if (IO_LY == 144) [[unlikely]]
            {
                change_status_mode(gba, STATUS_MODE_VBLANK);
            }
            else
            {
                change_status_mode(gba, STATUS_MODE_SPRITE);
            }
            break;

        case STATUS_MODE_VBLANK:
            IO_LY++;

            // there is a bug in which on line 153, LY=153 only lasts
            // for 4-Tcycles.
            if (IO_LY == 153)
            {
                gba.gameboy.ppu.next_cycles += 4;
                compare_LYC(gba);
            }
            else if (IO_LY == 154)
            {
                gba.gameboy.ppu.next_cycles += 452;
                IO_LY = 0;
                gba.gameboy.ppu.window_line = 0;
                compare_LYC(gba);
            }
            else if (IO_LY == 1)
            {
                IO_LY = 0;
                change_status_mode(gba, STATUS_MODE_SPRITE);
            }
            else
            {
                gba.gameboy.ppu.next_cycles += 456;
                compare_LYC(gba);
            }
            break;

        case STATUS_MODE_SPRITE:
            change_status_mode(gba, STATUS_MODE_TRANSFER);
            break;

        case STATUS_MODE_TRANSFER:
            change_status_mode(gba, STATUS_MODE_HBLANK);
            break;
    }
}
#endif

void DMA(Gba& gba)
{
    assert(IO_DMA <= 0xDF);

    // because it's possible for the index to be
    // cart ram, which may be invalid or RTC reg,
    // this first checks that the mask is zero,
    // if it is, then just std::memset the entire area
    // else, a normal copy happens.
    const auto& entry = gba.gameboy.rmap[IO_DMA >> 4];

    if (entry.mask == 0)
    {
        // std::memset(gba.gameboy.oam, entry.ptr[0], sizeof(gba.gameboy.oam));
        std::memset(gba.gameboy.oam, entry.ptr[0], 0xA0);
    }
    else
    {
        // TODO: check the math to see if this can go OOB for
        // mbc2-ram!!!
        // memcpy(gba.gameboy.oam, entry.ptr + ((IO_DMA & 0xF) << 8), sizeof(gba.gameboy.oam));
        constexpr auto max_range = 0xF << 8;
        assert(entry.mask >= max_range);
        memcpy(gba.gameboy.oam, entry.ptr + ((IO_DMA & 0xF) << 8), 0xA0);
    }
}

void draw_scanline(Gba& gba)
{
    // check if the user has set any pixels, if not, skip rendering!
    if (!gba.pixels || !gba.stride || !gba.bpp)
    {
        return;
    }

    switch (get_system_type(gba))
    {
        case SYSTEM_TYPE_DMG:
            DMG_render_scanline(gba);
            break;

         case SYSTEM_TYPE_GBC:
            GBC_render_scanline(gba);
            break;
    }
}

} // namespace gba::gb
