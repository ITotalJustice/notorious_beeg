#pragma once

#include "types.hpp"
#include "fwd.hpp"

namespace gba::gb {

// used mainly in debugging when i want to quickly silence
// the compiler about unsed vars.
#define UNUSED(var)

// ONLY use this for C-arrays, not pointers, not structs
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define gb_log(...)
#define log_err(...)
#define log_fatal(...)

enum
{
    // 4-mhz
    DMG_CPU_CLOCK = 4194304,
    // 8-mhz
    GBC_CPU_CLOCK = DMG_CPU_CLOCK * 2,
};

#define IO gba.gameboy.io
// JOYPAD
#define IO_JYP IO[0x00]
// SERIAL
#define IO_SB IO[0x01]
#define IO_SC IO[0x02]
// TIMERS
#define IO_DIV_LOWER IO[0x03]
#define IO_DIV IO[0x04]
#define IO_TIMA IO[0x05]
#define IO_TMA IO[0x06]
#define IO_TAC IO[0x07]
// PPU
#define IO_LCDC IO[0x40]
#define IO_STAT IO[0x41]
#define IO_SCY IO[0x42]
#define IO_SCX IO[0x43]
#define IO_LY IO[0x44]
#define IO_LYC IO[0x45]
#define IO_DMA IO[0x46]
#define IO_BGP IO[0x47]
#define IO_OBP0 IO[0x48]
#define IO_OBP1 IO[0x49]
#define IO_WY IO[0x4A]
#define IO_WX IO[0x4B]
#define IO_VBK IO[0x4F]
#define IO_HDMA1 IO[0x51]
#define IO_HDMA2 IO[0x52]
#define IO_HDMA3 IO[0x53]
#define IO_HDMA4 IO[0x54]
#define IO_HDMA5 IO[0x55]
#define IO_RP IO[0x56] // (GBC) infrared
#define IO_BCPS IO[0x68]
#define IO_BCPD IO[0x69]
#define IO_OCPS IO[0x6A]
#define IO_OCPD IO[0x6B]
#define IO_OPRI IO[0x6C] // (GBC) object priority
// MISC
#define IO_SVBK IO[0x70]
#define IO_KEY1 IO[0x4D]
#define IO_BOOTROM IO[0x50]
#define GB_IO_IF IO[0x0F]
#define GB_IO_IE gba.gameboy.hram[0x7F]
// undocumented registers (GBC)
#define IO_72 IO[0x72]
#define IO_73 IO[0x73]
#define IO_74 IO[0x74]
#define IO_75 IO[0x75] // only bit 4-6 are usable
#define IO_76 IO[0x76]
#define IO_77 IO[0x77]


enum Interrupts
{
    INTERRUPT_VBLANK     = 0x01,
    INTERRUPT_LCD_STAT   = 0x02,
    INTERRUPT_TIMER      = 0x04,
    INTERRUPT_SERIAL     = 0x08,
    INTERRUPT_JOYPAD     = 0x10,
};

enum StatusModes
{
    STATUS_MODE_HBLANK      = 0,
    STATUS_MODE_VBLANK      = 1,
    STATUS_MODE_SPRITE      = 2,
    STATUS_MODE_TRANSFER    = 3
};

enum StatIntModes
{
    STAT_INT_MODE_0             = 0x08,
    STAT_INT_MODE_1             = 0x10,
    STAT_INT_MODE_2             = 0x20,
    STAT_INT_MODE_COINCIDENCE   = 0x40
};

auto read8(Gba& gba, u16 addr) -> u8;
void write8(Gba& gba, u16 addr, u8 value);
auto read16(Gba& gba, u16 addr) -> u16;
void write16(Gba& gba, u16 addr, u16 value);

auto ffread8(Gba& gba, u8 addr) -> u8;
void ffwrite8(Gba& gba, u8 addr, u8 value);

void on_lcdc_write(Gba& gba, u8 value);
void on_stat_write(Gba& gba, u8 value);

void div_write(Gba& gba, u8 value);
void tima_write(Gba& gba, u8 value);
void tma_write(Gba& gba, u8 value);
void tac_write(Gba& gba, u8 value);

void bcpd_write(Gba& gba, u8 value);
void ocpd_write(Gba& gba, u8 value);
void GBC_on_bcpd_update(Gba& gba);
void GBC_on_ocpd_update(Gba& gba);
void hdma5_write(Gba& gba, u8 value);

// these should also be static
auto get_mbc_flags(u8 cart_type, u8* flags_out) -> bool;
auto get_cart_ram_size(const struct CartHeader* header, u32* size) -> bool;
auto setup_mbc(Gba& gba, const struct CartHeader* header) -> bool;
void setup_mmap(Gba& gba);
void update_rom_banks(Gba& gba);
void update_ram_banks(Gba& gba);
void update_vram_banks(Gba& gba);
void update_wram_banks(Gba& gba);

// used internally
void DMA(Gba& gba);
void draw_scanline(Gba& gba);
void set_coincidence_flag(Gba& gba, bool n);

void set_status_mode(Gba& gba, u8 mode);
auto get_status_mode(const Gba& gba) -> u8;

void compare_LYC(Gba& gba);

void joypad_write(Gba& gba, u8 value);

void enable_interrupt(Gba& gba, enum Interrupts interrupt);
void disable_interrupt(Gba& gba, enum Interrupts interrupt);

// used internally
void cpu_run(Gba& gba);
#if !USE_SCHED
void ppu_run(Gba& gba, u8 cycles);
#endif

auto is_lcd_enabled(const Gba& gba) -> bool;
auto is_win_enabled(const Gba& gba) -> bool;
auto is_obj_enabled(const Gba& gba) -> bool;
auto is_bg_enabled(const Gba& gba) -> bool;

void on_timer_reload_event(void* user, s32 id = 0, s32 late = 0);
void on_timer_event(void* user, s32 id = 0, s32 late = 0);
void on_div_event(void* user, s32 id, s32 late);

void on_halt_event(void* user, s32 id = 0, s32 late = 0);
void on_interrupt_event(void* user, s32 id, s32 late);
void schedule_interrupt(Gba& gba, u8 cycles_delay = 0);

} // namespace gba::gb
