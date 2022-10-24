#include "bit.hpp"
#include "gba.hpp"
#include "ppu.hpp"
#include "../internal.hpp"
#include "../gb.hpp"
#include "scheduler.hpp"
#include "logger.hpp"

#include <cstring>
#include <cassert>

namespace gba::gb {
namespace {

enum ModeCycles
{
    MODE_CYCLES_HBLANK = 204,
    MODE_CYCLES_VBLANK = 456,
    MODE_CYCLES_SPRITE = 80,
    MODE_CYCLES_TRANSFER = 172,
};

void stat_interrupt_update(Gba& gba)
{
    assert(is_lcd_enabled(gba));
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
            // line goes high
            gba.gameboy.ppu.stat_line = true;
            enable_interrupt(gba, INTERRUPT_LCD_STAT);
        }
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
    gba.gameboy.ppu.mode = new_mode;

    // this happens on every mode switch because going to transfer
    // mode will set the stat_line=0 (unless LY==LYC)
    stat_interrupt_update(gba);

    u16 next_cycles = 0;

    // TODO: check what the timing should actually be for ppu modes!
    switch (new_mode)
    {
        case STATUS_MODE_HBLANK:
            next_cycles = MODE_CYCLES_HBLANK;
            draw_scanline(gba);

            if (gba.hblank_callback != nullptr)
            {
                gba.hblank_callback(gba.userdata, IO_LY);
            }
            break;

        case STATUS_MODE_VBLANK:
            enable_interrupt(gba, INTERRUPT_VBLANK);
            next_cycles = MODE_CYCLES_VBLANK;

            if (gba.vblank_callback != nullptr)
            {
                gba.vblank_callback(gba.userdata);
            }
            gba.gameboy.ppu.first_frame_enabled = false;
            break;

        case STATUS_MODE_SPRITE:
            next_cycles = MODE_CYCLES_SPRITE;
            break;

        case STATUS_MODE_TRANSFER:
            next_cycles = MODE_CYCLES_TRANSFER;
            break;
    }

    #if USE_SCHED
    gba.gameboy.ppu.next_cycles = next_cycles;
    #else
    gba.gameboy.ppu.next_cycles += next_cycles;
    #endif
}

void on_lcd_disable(Gba& gba)
{
    log::print_info(gba, log::Type::GB_PPU, "disabling ppu...\n");

    // this *should* only happen in vblank!
    if (get_status_mode(gba) != STATUS_MODE_VBLANK)
    {
        log::print_warn(gba, log::Type::GB_PPU, "game is disabling lcd outside vblank: 0x%0X\n", get_status_mode(gba));
    }

    // in progress hdma should be stopped when ppu is turned off
    if (is_system_gbc(gba))
    {
        gba.gameboy.ppu.hdma_length = 0;
        IO_HDMA5 = 0xFF;
    }

    // reads as zero and we start from scanline zero on enable
    IO_LY = 0;
    // unset mode bits as they read as zero
    IO_STAT &= ~(0x3);
    gba.gameboy.ppu.mode = 0;
    // TODO: verify this. TCAGBD says it goes low when off, but doing
    // so breaks stat_lyc_onoff test
    // gba.gameboy.ppu.stat_line = false;

    #if USE_SCHED
    gba.delta.remove(scheduler::ID::PPU);
    gba.scheduler.remove(scheduler::ID::PPU);
    #endif
}

void on_lcd_enable(Gba& gba)
{
    printf("enabling ppu!\n");

    // first frame is skipped as no vblank signal is sent to lcd
    gba.gameboy.ppu.first_frame_enabled = true;
    // timing based off oam_bug/1-lcd_sync, basically 1 scanline of cycles-4
    // i am almost certain the -4 is time taken to read from LY
    // but as i do not have subinstruction timing, the -4 is handled here
    // a more correct-ish approach would be to apply the cycles before
    // executing an instructio and then checking the cycles elapsed on
    // lcd_enable and subtract them from MODE_CYCLES_VBLANK.
    // gba.gameboy.ppu.next_cycles = MODE_CYCLES_VBLANK - 4;
    gba.gameboy.ppu.next_cycles = MODE_CYCLES_SPRITE - 4;
    gba.gameboy.ppu.mode = STATUS_MODE_SPRITE;
    compare_LYC(gba);
    #if USE_SCHED
    gba.scheduler.add(scheduler::ID::PPU, gba.gameboy.ppu.next_cycles, on_ppu_event, &gba);
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

void on_ppu_event(void* user, s32 id, s32 late)
{
    #if USE_SCHED
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);

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
                gba.gameboy.ppu.next_cycles = MODE_CYCLES_VBLANK;
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

    gba.scheduler.add(id, gba.delta.get(id, gba.gameboy.ppu.next_cycles), on_ppu_event, &gba);
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
    for (auto i = 0; i < 8; i++)
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
    IO_STAT |= mode & 0x3;
}

// this returns the internal mode, which, in almost all cases will be
// the same mode as reported in stat, only excpetion is when the lcd
// is enabled as stat will report mode0 for ~MODE_CYCLES_SPRITE cycles whilst the internal
// mode is actually mode2.
auto get_status_mode(const Gba& gba) -> u8
{
    return gba.gameboy.ppu.mode;
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
    if (is_lcd_enabled(gba))
    {
        const auto was_equal = bit::is_set<2>(IO_STAT);
        const auto now_equal = IO_LY == IO_LYC;

        if (!was_equal && now_equal)
        {
            set_coincidence_flag(gba, true);
            stat_interrupt_update(gba);
        }
        else if (was_equal && !now_equal)
        {
            set_coincidence_flag(gba, false);
            stat_interrupt_update(gba);
        }
    }
}

void on_stat_write(Gba& gba, u8 value)
{
    // keep the read-only bits!
    IO_STAT = (IO_STAT & 0x7) | (value & 0x78);

    if (is_lcd_enabled(gba))
    {
        compare_LYC(gba);
        stat_interrupt_update(gba);
    }
}

void on_lcdc_write(Gba& gba, const u8 value)
{
    const auto was_enabled = bit::is_set<7>(IO_LCDC);
    const auto now_enabled = bit::is_set<7>(value);

    IO_LCDC = value;

    // check if the game wants to disable the ppu
    if (was_enabled && !now_enabled)
    {
        on_lcd_disable(gba);
    }

    // check if the game wants to re-enable the lcd
    else if (!was_enabled && now_enabled)
    {
        on_lcd_enable(gba);
    }
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
                gba.gameboy.ppu.next_cycles += MODE_CYCLES_VBLANK;
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
    assert(entry.ptr);

    // note: dma is not instant.
    // on gbc,agb the cpu cannot access the src area
    // mts/acceptance/oam_dma/sources-GS relies on this behaviour
    if (entry.mask == 0)
    {
        std::memset(gba.gameboy.oam, entry.ptr[0], 0xA0);
    }
    else
    {
        // TODO: check the math to see if this can go OOB for mbc2-ram!
        constexpr auto max_range = 0xF << 8;
        assert(entry.mask >= max_range);
        memcpy(gba.gameboy.oam, entry.ptr + ((IO_DMA & 0xF) << 8), 0xA0);
    }
}

void draw_scanline(Gba& gba)
{
    // first frame after the lcd is enabled is not displayed!
    if (gba.gameboy.ppu.first_frame_enabled) [[unlikely]]
    {
        u32 scanline[160]{};
        const auto x = 40;
        const auto y = gba.stride * (8 + IO_LY);
        write_scanline_to_frame(gba.pixels, gba.stride, gba.bpp, x, y, scanline);
        return;
    }

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
