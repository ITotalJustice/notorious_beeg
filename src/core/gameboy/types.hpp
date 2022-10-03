#pragma once

#include <cstddef>

#include <cstdint>
#include "palette_table.hpp"
#include "fwd.hpp"

namespace gba::gb {

#define USE_SCHED 1

// fwd declare structs
struct Core;
struct Rtc;
struct CartHeader;
struct Joypad;
struct Config;
struct MBC_RomBankInfo;


enum
{
    SCREEN_WIDTH = 160,
    SCREEN_HEIGHT = 144,

    ROM_SIZE_MAX = 1024 * 1024 * 4, // 4MiB

    BOOTROM_SIZE = 0x100,

#if 1
    CPU_CYCLES = 4213440, // 456 * 154 (clocks per line * number of lines * 60 fps)
    FRAME_CPU_CYCLES = 4213440 / 60, // 70224
#else
    CPU_CYCLES = 4194304,
    FRAME_CPU_CYCLES = CPU_CYCLES / 60,
#endif
};


enum SaveSizes
{
    SAVE_SIZE_NONE   = 0x00000,
    SAVE_SIZE_1      = 0x00800,
    SAVE_SIZE_2      = 0x02000,
    SAVE_SIZE_3      = 0x08000,
    SAVE_SIZE_4      = 0x20000,
    SAVE_SIZE_5      = 0x10000,

    SAVE_SIZE_MBC2   = 0x00200,

    SAVE_SIZE_MAX = SAVE_SIZE_4,
};

enum RegionName
{
    RegionName_ROM_BANK_0, // 0000-3FFF
    RegionName_ROM_BANK_X, // 4000-7FFF
    RegionName_VRAM, // 8000-9FFF
    RegionName_EXTERNAL_RAM, // A000-BFFF
    RegionName_WRAM_BANK_0, // C000-CFFF
    RegionName_WRAM_BANK_1, // D000-DFFF
    RegionName_WRAM_BANK_0_ECHO, // E000-EFFF
    RegionName_WRAM_BANK_1_ECHO, // F000-FDFF
    RegionName_OAM, // FE00-FE9F
    RegionName_UNUSED, // FEA0-FEFF
    RegionName_IO, // FF00-FF7F
    RegionName_HRAM, // FF80-FFFE
    RegionName_IE, // FFFF
};

enum MbcType
{
    MbcType_0 = 1,
    MbcType_1,
    MbcType_2,
    MbcType_3,
    MbcType_5,
};

enum MbcFlags
{
    MBC_FLAGS_NONE      = 0,
    MBC_FLAGS_RAM       = 1 << 0,
    MBC_FLAGS_BATTERY   = 1 << 1,
    MBC_FLAGS_RTC       = 1 << 2,
    MBC_FLAGS_RUMBLE    = 1 << 3,
};

enum RtcMappedReg
{
    RTC_MAPPED_REG_S,
    RTC_MAPPED_REG_M,
    RTC_MAPPED_REG_H,
    RTC_MAPPED_REG_DL,
    RTC_MAPPED_REG_DH,
};

enum CpuFlags
{
    CPU_FLAG_C,
    CPU_FLAG_H,
    CPU_FLAG_N,
    CPU_FLAG_Z
};

enum CpuRegisters
{
    CPU_REGISTER_B,
    CPU_REGISTER_C,
    CPU_REGISTER_D,
    CPU_REGISTER_E,
    CPU_REGISTER_H,
    CPU_REGISTER_L,
    CPU_REGISTER_A,
    CPU_REGISTER_F
};

enum CpuRegisterPairs
{
    CPU_REGISTER_PAIR_BC,
    CPU_REGISTER_PAIR_DE,
    CPU_REGISTER_PAIR_HL,
    CPU_REGISTER_PAIR_AF,
    CPU_REGISTER_PAIR_SP,
    CPU_REGISTER_PAIR_PC
};

enum Button
{
    BUTTON_A         = 1 << 0,
    BUTTON_B         = 1 << 1,
    BUTTON_SELECT    = 1 << 2,
    BUTTON_START     = 1 << 3,

    BUTTON_RIGHT     = 1 << 4,
    BUTTON_LEFT      = 1 << 5,
    BUTTON_UP        = 1 << 6,
    BUTTON_DOWN      = 1 << 7,

    // helpers
    BUTTON_XAXIS = BUTTON_RIGHT | BUTTON_LEFT,
    BUTTON_YAXIS = BUTTON_UP | BUTTON_DOWN,
    BUTTON_DIRECTIONAL = BUTTON_XAXIS | BUTTON_YAXIS,
};

// the system type is set based on the game that is loaded
// for example, if a gbc ONLY game is loaded, the system type is
// set to GBC.
// TODO: support manually overriding the system type for supported roms
// for example, forcing DMG type for games that support both DMG and GBC.
// for this, it's probably best to return a custom error if the loaded rom
// is valid, but ONLY suppports GBC type, but DMG was forced...
enum SystemType
{
    SYSTEM_TYPE_DMG = 1 << 0,
    SYSTEM_TYPE_GBC = 1 << 2,
};

// this setting only applies when the game is loaded as DMG or SGB game.
// GBC games set their own colour palette and cannot be customised!
enum PaletteConfig
{
    // uses the default palette
    PALETTE_CONFIG_NONE          = 0,
    // these can be OR'd together to set an additional option.
    // if both are OR'd, first, the game will try to use a builtin palette.
    // if a builtin palette cannot be found, then it will fallback to the
    // custom palette instead.
    PALETTE_CONFIG_USE_CUSTOM    = 1 << 0,
    PALETTE_CONFIG_USE_BUILTIN   = 1 << 1,
};

struct Config
{
    enum PaletteConfig palette_config;
    struct PaletteEntry custom_palette;
};

struct CartHeader
{
    u8 entry_point[0x4];
    u8 logo[0x30];
    char title[0x10];
    u8 new_licensee_code[2];
    u8 sgb_flag;
    u8 cart_type;
    u8 rom_size;
    u8 ram_size;
    u8 destination_code;
    u8 old_licensee_code;
    u8 rom_version;
    u8 header_checksum;
    u8 global_checksum[2];
};


struct RomInfo
{
    u32 rom_size;
    u32 ram_size;
    u8 flags; /* flags ored together */
    u8 _padding[3];
};

struct Rtc
{
    u8 S;
    u8 M;
    u8 H;
    u8 DL;
    u8 DH;
};

struct Joypad
{
    u8 var;
};

struct Cpu
{
    u16 cycles;
    u16 SP;
    u16 PC;
    u8 registers[8];

    bool c;
    bool h;
    bool n;
    bool z;

    bool ime;
    bool ime_delay;
    bool halt;
    bool halt_bug;
    bool double_speed;
    bool _padding;
};

struct PalCache
{
    bool used;
    u8 pal;
};

struct Ppu
{
    s16 next_cycles;

    // these are set when a hdma occurs (not a DMA or GDMA)
    u16 hdma_src_addr;
    u16 hdma_dst_addr;
    u16 hdma_length;

    // this is the internal line counter which is used as the index
    // for the window instead of IO_LY.
    u8 window_line;
    bool stat_line;

    // stat mode is seperate as on lcd enable, stat reports that the lcd is
    // in mode0, when it is infact in mode2.
    // stat will correct itself after ~80 cycles when it changes to mode3.
    u8 mode;

    // when the lcd is enabled, the first frame is not disabled.
    bool first_frame_enabled;

    union
    {
        struct
        {
            u32 bg_colours[8][4]; // calculate the colours from the palette once.
            u32 obj_colours[8][4];

            u8 bg_palette[64]; // background palette memory.
            u8 obj_palette[64]; // sprite palette memory.
        } gbc;

        struct
        {
            u32 bg_colours[20][4];
            u32 obj_colours[2][20][4];

            struct PalCache bg_cache[20];
            struct PalCache obj_cache[2][20];
        } dmg;
    } system;

    bool dirty_bg[8]; // only update the colours if the palette changes values.
    bool dirty_obj[8];
};

// todo: fix bad name
struct ReadMapEntry
{
    const u8* ptr;
    u16 mask;
};

struct WriteMapEntry
{
    u8* ptr;
    u16 mask;
};

struct MBC_RomBankInfo
{
    struct ReadMapEntry entries[4];
};

struct MBC_RamBankInfo
{
    struct ReadMapEntry r[2];
    struct WriteMapEntry w[2];
};

struct Cart
{
    u32 rom_size; // set by the header
    u32 ram_size; // set by the header

    u16 rom_bank_max;
    u16 rom_bank;
    u8 rom_bank_lo;
    u8 rom_bank_hi;

    u8 ram_bank_max;
    u8 ram_bank;

    u8 rtc_mapped_reg;
    struct Rtc rtc;
    u8 internal_rtc_counter;

    bool bank_mode;
    bool ram_enabled;
    bool in_ram;

    u8 type;
    u8 flags;
};

struct Timer
{
    u8 reserved[3];
    bool reloading;
};

struct mem
{
    u8 vbk;
    u8 svbk;
};

// TODO: this struct needs to be re-organised.
// atm, i've just been dumping vars in here as one big container,
// which works fine, though, it's starting to get messy, and could be
// *slightly* optimised by putting common accessed vars next to each other.
struct Core
{
    struct ReadMapEntry rmap[16];
    struct WriteMapEntry wmap[16];

    // to optimise on space, the gb core uses the memory already
    // avaliable on the gba!
    u8* vram[2];
    u8* oam;
    u8* wram[8];
    u8* hram;
    u8* io;

    u16 cycles;
    struct mem mem;
    struct Cpu cpu;
    struct Ppu ppu;
    struct Cart cart;
    struct Timer timer;
    struct Joypad joypad;

    struct PaletteEntry palette; /* default */

    enum SystemType system_type; // set by the rom itself

    struct Config config;

    u8* ram;
    std::size_t ram_size; // set by the user
    bool ram_dirty;
};

struct State
{
    u16 magic;
    u16 version;
    u32 size;

    struct mem mem;
    struct Cpu cpu;
    struct Ppu ppu;
    struct Cart cart;
    struct Timer timer;
};

struct CartName
{
    char name[0x11]; // this is NULL terminated
};

} // namespace gba::gb
