#include "apu/apu.hpp"
#include "gb.hpp"
#include "internal.hpp"
#include "mbc.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include "ppu/ppu.hpp"
#include "scheduler.hpp"

#include <array>
#include <cassert>
#include <cstdio>

namespace gba::gb {
namespace {

// table of unused bits
// OR'd with IO reads, see bus.c
// if dmg mode, GBC regs are set to 0xFF, so reads return
// correct values on dmg.
constexpr u8 IO_UNUSED_BIT_TABLE[0x80] =
{
    /*[0x00] =*/ 0xC0, // JOYP
    /*[0x01] =*/ 0x00, // SB
    /*[0x02] =*/ 0x7C, // SC
    /*[0x03] =*/ 0xFF, // DIV LOWER
    /*[0x04] =*/ 0x00, // DIV UPPER
    /*[0x05] =*/ 0x00, // TIMA
    /*[0x06] =*/ 0x00, // TMA
    /*[0x07] =*/ 0xF8, // TAC
    /*[0x08] =*/ 0xFF,
    /*[0x09] =*/ 0xFF,
    /*[0x0A] =*/ 0xFF,
    /*[0x0B] =*/ 0xFF,
    /*[0x0C] =*/ 0xFF,
    /*[0x0D] =*/ 0xFF,
    /*[0x0E] =*/ 0xFF,
    /*[0x0F] =*/ 0xE0, // IF

    /*[0x10] =*/ 0x80, // NR10
    /*[0x11] =*/ 0x3F, // NR11
    /*[0x12] =*/ 0x00, // NR12
    /*[0x13] =*/ 0xFF, // NR13
    /*[0x14] =*/ 0xBF, // NR14
    /*[0x15] =*/ 0xFF,
    /*[0x16] =*/ 0x3F, // NR21
    /*[0x17] =*/ 0x00, // NR22
    /*[0x18] =*/ 0xFF, // NR23
    /*[0x19] =*/ 0xBF, // NR24
    /*[0x1A] =*/ 0x7F, // NR30
    /*[0x1B] =*/ 0xFF, // NR31
    /*[0x1C] =*/ 0x9F, // NR32
    /*[0x1D] =*/ 0xFF, // NR33
    /*[0x1E] =*/ 0xBF, // NR34
    /*[0x1F] =*/ 0xFF,

    /*[0x20] =*/ 0xFF, // NR41
    /*[0x21] =*/ 0x00, // NR42
    /*[0x22] =*/ 0x00, // NR43
    /*[0x23] =*/ 0xBF, // NR44
    /*[0x24] =*/ 0x00, // NR50
    /*[0x25] =*/ 0x00, // NR51
    /*[0x26] =*/ 0x70, // NR52
    /*[0x27] =*/ 0xFF,
    /*[0x28] =*/ 0xFF,
    /*[0x29] =*/ 0xFF,
    /*[0x2A] =*/ 0xFF,
    /*[0x2B] =*/ 0xFF,
    /*[0x2C] =*/ 0xFF,
    /*[0x2D] =*/ 0xFF,
    /*[0x2E] =*/ 0xFF,
    /*[0x2F] =*/ 0xFF,

    // this can be read back as 0xFF or the actual value
    // this is handled in bus.c
    /*[0x30] =*/ 0x00, // WAVE RAM
    /*[0x31] =*/ 0x00, // WAVE RAM
    /*[0x32] =*/ 0x00, // WAVE RAM
    /*[0x33] =*/ 0x00, // WAVE RAM
    /*[0x34] =*/ 0x00, // WAVE RAM
    /*[0x35] =*/ 0x00, // WAVE RAM
    /*[0x36] =*/ 0x00, // WAVE RAM
    /*[0x37] =*/ 0x00, // WAVE RAM
    /*[0x38] =*/ 0x00, // WAVE RAM
    /*[0x39] =*/ 0x00, // WAVE RAM
    /*[0x3A] =*/ 0x00, // WAVE RAM
    /*[0x3B] =*/ 0x00, // WAVE RAM
    /*[0x3C] =*/ 0x00, // WAVE RAM
    /*[0x3D] =*/ 0x00, // WAVE RAM
    /*[0x3E] =*/ 0x00, // WAVE RAM
    /*[0x3F] =*/ 0x00, // WAVE RAM

    /*[0x40] =*/ 0x00, // LCDC
    /*[0x41] =*/ 0x80, // STAT
    /*[0x42] =*/ 0x00, // SCY
    /*[0x43] =*/ 0x00, // SCX
    /*[0x44] =*/ 0x00, // LY
    /*[0x45] =*/ 0x00, // LYC
    /*[0x46] =*/ 0x00, // DMA
    /*[0x47] =*/ 0x00, // BGP
    /*[0x48] =*/ 0x00, // OPB0
    /*[0x49] =*/ 0x00, // OPB1
    /*[0x4A] =*/ 0x00, // WY
    /*[0x4B] =*/ 0x00, // WX
    /*[0x4C] =*/ 0xFF,
    /*[0x4D] =*/ 0x7E, // KEY1 [GBC]
    /*[0x4E] =*/ 0xFF,
    /*[0x4F] =*/ 0xFE, // VBK [GBC]

    /*[0x50] =*/ 0xFF,
    /*[0x51] =*/ 0x00, // HDMA1 [GBC]
    /*[0x52] =*/ 0x00, // HDMA2 [GBC]
    /*[0x53] =*/ 0x00, // HDMA3 [GBC]
    /*[0x54] =*/ 0x00, // HDMA4 [GBC]
    /*[0x55] =*/ 0x00, // HDMA5 [GBC]
    /*[0x56] =*/ 0x3E, // RP [GBC]
    /*[0x57] =*/ 0xFF,
    /*[0x58] =*/ 0xFF,
    /*[0x59] =*/ 0xFF,
    /*[0x5A] =*/ 0xFF,
    /*[0x5B] =*/ 0xFF,
    /*[0x5C] =*/ 0xFF,
    /*[0x5D] =*/ 0xFF,
    /*[0x5E] =*/ 0xFF,
    /*[0x5F] =*/ 0xFF,

    /*[0x60] =*/ 0xFF,
    /*[0x61] =*/ 0xFF,
    /*[0x62] =*/ 0xFF,
    /*[0x63] =*/ 0xFF,
    /*[0x64] =*/ 0xFF,
    /*[0x65] =*/ 0xFF,
    /*[0x66] =*/ 0xFF,
    /*[0x67] =*/ 0xFF,
    /*[0x68] =*/ 0x40, // BCPS [GBC]
    /*[0x69] =*/ 0x00, // BCPD [GBC]
    /*[0x6A] =*/ 0x40, // OCPS [GBC]
    /*[0x6B] =*/ 0x00, // OCPD [GBC]
    /*[0x6C] =*/ 0xFE, // OPRI [GBC-MODE]
    /*[0x6D] =*/ 0xFF,
    /*[0x6E] =*/ 0xFF,
    /*[0x6F] =*/ 0xFF,

    /*[0x70] =*/ 0xF8, // SVBK [GBC]
    /*[0x71] =*/ 0xFF,
    /*[0x72] =*/ 0x00, // UNK [GBC]
    /*[0x73] =*/ 0x00, // UNK [GBC]
    /*[0x74] =*/ 0x00, // UNK [GBC-MODE]
    /*[0x75] =*/ 0x8F, // UNK [GBC]
    /*[0x76] =*/ 0x00, // UNK [GBC]
    /*[0x77] =*/ 0x00, // UNK [GBC]
    /*[0x78] =*/ 0xFF,
    /*[0x79] =*/ 0xFF,
    /*[0x7A] =*/ 0xFF,
    /*[0x7B] =*/ 0xFF,
    /*[0x7C] =*/ 0xFF,
    /*[0x7D] =*/ 0xFF,
    /*[0x7E] =*/ 0xFF,
    /*[0x7F] =*/ 0xFF,
};

inline auto is_vram_writeable(const Gba& gba) -> bool
{
    // vram cannot be written to during mode 3
    return get_status_mode(gba) != STATUS_MODE_TRANSFER;
}

inline auto is_oam_writeable(const Gba& gba) -> bool
{
    // oam cannot be written to during mode 2 and 3
    const auto mode = get_status_mode(gba);
    return mode != STATUS_MODE_SPRITE && mode != STATUS_MODE_TRANSFER;
}

inline auto read_unusable([[maybe_unused]] Gba& gba, [[maybe_unused]] u16 addr) -> u8
{
    return 0xFF;
}

inline auto read_vram(Gba& gba, u16 addr) -> u8
{
    return gba.gameboy.vram[gba.gameboy.mem.vbk][addr & 0x1FFF];
}

inline auto read_oam(Gba& gba, u16 addr) -> u8
{
    return gba.gameboy.oam[addr & 0xFF];
}

inline auto read_hram(Gba& gba, u16 addr) -> u8
{
    return gba.gameboy.hram[addr & 0x7F];
}

inline void write_unusable([[maybe_unused]] Gba& gba, [[maybe_unused]] u16 addr, [[maybe_unused]] u8 value)
{
}

inline void write_vram(Gba& gba, u16 addr, u8 value)
{
    if (is_vram_writeable(gba)) [[likely]]
    {
        gba.gameboy.vram[gba.gameboy.mem.vbk][addr & 0x1FFF] = value;
    }
}

inline void write_oam(Gba& gba, u16 addr, u8 value)
{
    if (is_oam_writeable(gba)) [[likely]]
    {
        gba.gameboy.oam[addr & 0xFF] = value;
    }
    else
    {
        printf("not allowed write: 0x%02X\n", value);
    }
}

inline void write_hram(Gba& gba, u16 addr, u8 value)
{
    addr &= 0x7F;
    gba.gameboy.hram[addr] = value;

    if (addr == 0x7F) // writing to IE
    {
        schedule_interrupt(gba);
    }
}

inline void write_io_gbc(Gba& gba, u16 addr, u8 value)
{
    assert(is_system_gbc(gba) == true);
    addr &= 0x7F;

    switch (addr)
    {
        case 0x4D:
            IO_KEY1 |= value & 0x1;
            gb_log("writing to key1 0x%02X\n", value);
            break;

        case 0x4F: // (VBK)
            gba.gameboy.mem.vbk = value & 1;
            IO_VBK = gba.gameboy.mem.vbk;
            update_vram_banks(gba);
            break;

        case 0x51: // (HDMA1)
            IO_HDMA1 = value;
            gba.gameboy.ppu.hdma_src_addr &= 0xF0;
            gba.gameboy.ppu.hdma_src_addr |= (value & 0xFF) << 8;
            break;

        case 0x52: // (HDMA2)
            IO_HDMA2 = value;
            gba.gameboy.ppu.hdma_src_addr &= 0xFF00;
            gba.gameboy.ppu.hdma_src_addr |= value & 0xF0;
            break;

        case 0x53: // (HMDA3)
            IO_HDMA3 = value;
            gba.gameboy.ppu.hdma_dst_addr &= 0xF0;
            gba.gameboy.ppu.hdma_dst_addr |= (value & 0x1F) << 8;
            break;

        case 0x54: // (HDMA4)
            IO_HDMA4 = value;
            gba.gameboy.ppu.hdma_dst_addr &= 0x1F00;
            gba.gameboy.ppu.hdma_dst_addr |= value & 0xF0;
            break;

        case 0x55: // (HDMA5)
            hdma5_write(gba, value);
            break;

        case 0x68: // BCPS
            IO_BCPS = value;
            GBC_on_bcpd_update(gba);
            break;

        case 0x69: // BCPD
            if (is_vram_writeable(gba)) [[likely]]
            {
                bcpd_write(gba, value);
            }
            break;

        case 0x6A: // OCPS
            IO_OCPS = value;
            GBC_on_ocpd_update(gba);
            break;

        case 0x6B: // OCPD
            if (is_vram_writeable(gba)) [[likely]]
            {
                ocpd_write(gba, value);
            }
            break;

        case 0x6C: // OPRI
            IO_OPRI = value;
            log_fatal("[INFO] IO_OPRI %d\n", value & 1);
            break;

        case 0x70: // (SVBK) always set between 1-7
            gba.gameboy.mem.svbk = value;
            if (!gba.gameboy.mem.svbk)
            {
                gba.gameboy.mem.svbk = 1;
            }
            IO_SVBK = value & ~0x1;
            update_wram_banks(gba);
            break;
    }
}

inline auto read_io(Gba& gba, u16 addr) -> u8
{
    addr &= 0x7F;
    u8 result;

    switch (addr)
    {
        case 0x10: result = REG_SOUND1CNT_L; break; // NR10
        case 0x11: result = REG_SOUND1CNT_H; break; // NR11
        case 0x12: result = REG_SOUND1CNT_H >> 8; break; // NR12
        case 0x13: result = REG_SOUND1CNT_X; break; // NR13
        case 0x14: result = REG_SOUND1CNT_X >> 8; break; // NR14

        case 0x16: result = REG_SOUND2CNT_L; break; // NR21
        case 0x17: result = REG_SOUND2CNT_L >> 8; break; // NR22
        case 0x18: result = REG_SOUND2CNT_H; break; // NR23
        case 0x19: result = REG_SOUND2CNT_H >> 8; break; // NR24
        case 0x1A: result = REG_SOUND3CNT_L; break; // NR30
        case 0x1B: result = REG_SOUND3CNT_H; break; // NR31
        case 0x1C: result = REG_SOUND3CNT_H >> 8; break; // NR32
        case 0x1D: result = REG_SOUND3CNT_X; break; // NR33
        case 0x1E: result = REG_SOUND3CNT_X >> 8; break; // NR34
        case 0x20: result = REG_SOUND4CNT_L; break; // NR41
        case 0x21: result = REG_SOUND4CNT_L >> 8; break; // NR42
        case 0x22: result = REG_SOUND4CNT_H; break; // NR43
        case 0x23: result = REG_SOUND4CNT_H >> 8; break; // NR44
        case 0x24: result = REG_SOUNDCNT_L; break; // NR50
        case 0x25: result = REG_SOUNDCNT_L >> 8; break; // NR51
        case 0x26: result = REG_SOUNDCNT_X; break; // NR52

        case 0x30: case 0x31: case 0x32: case 0x33: // WAVE RAM
        case 0x34: case 0x35: case 0x36: case 0x37: // WAVE RAM
        case 0x38: case 0x39: case 0x3A: case 0x3B: // WAVE RAM
        case 0x3C: case 0x3D: case 0x3E: case 0x3F: // WAVE RAM
            result = apu::read_WAVE(gba, addr);
            break;

        default:
            result = IO[addr];
            break;
    }

    return result | IO_UNUSED_BIT_TABLE[addr];
}

inline void write_io(Gba& gba, u16 addr, u8 value)
{
    addr &= 0x7F;

    switch (addr)
    {
        case 0x00: // joypad
            joypad_write(gba, value);
            break;

        case 0x01: // SB (Serial transfer data)
            break;

        case 0x02: // SC (Serial Transfer Control)
            break;

        case 0x04:
            div_write(gba, value);
            break;

        case 0x05:
            tima_write(gba, value);
            break;

        case 0x06:
            tma_write(gba, value);
            break;

        case 0x07:
            tac_write(gba, value);
            break;

        case 0x0F:
            GB_IO_IF = value;
            schedule_interrupt(gba);
            break;

        case 0x10: apu::write_NR10(gba, value); break;
        case 0x11: apu::write_NR11(gba, value); break;
        case 0x12: apu::write_NR12(gba, value); break;
        case 0x13: apu::write_NR13(gba, value); break;
        case 0x14: apu::write_NR14(gba, value); break;
        case 0x16: apu::write_NR21(gba, value); break;
        case 0x17: apu::write_NR22(gba, value); break;
        case 0x18: apu::write_NR23(gba, value); break;
        case 0x19: apu::write_NR24(gba, value); break;
        case 0x1A: apu::write_NR30(gba, value); break;
        case 0x1B: apu::write_NR31(gba, value); break;
        case 0x1C: apu::write_NR32(gba, value); break;
        case 0x1D: apu::write_NR33(gba, value); break;
        case 0x1E: apu::write_NR34(gba, value); break;
        case 0x20: apu::write_NR41(gba, value); break;
        case 0x21: apu::write_NR42(gba, value); break;
        case 0x22: apu::write_NR43(gba, value); break;
        case 0x23: apu::write_NR44(gba, value); break;
        case 0x24: apu::write_NR50(gba, value); break;
        case 0x25: apu::write_NR51(gba, value); break;
        case 0x26: apu::write_NR52(gba, value); break;

        case 0x30: case 0x31: case 0x32: case 0x33: // WAVE RAM
        case 0x34: case 0x35: case 0x36: case 0x37: // WAVE RAM
        case 0x38: case 0x39: case 0x3A: case 0x3B: // WAVE RAM
        case 0x3C: case 0x3D: case 0x3E: case 0x3F: // WAVE RAM
            apu::write_WAVE(gba, addr, value);
            break;

        case 0x40:
            on_lcdc_write(gba, value);
            break;

        case 0x41:
            on_stat_write(gba, value);
            break;

        case 0x42:
            IO_SCY = value;
            break;

        case 0x43:
            IO_SCX = value;
            break;

        case 0x45:
            IO_LYC = value;
            compare_LYC(gba);
            break;

        case 0x46:
            IO_DMA = value;
            DMA(gba);
            break;

        case 0x47:
            on_bgp_write(gba, value);
            break;

        case 0x48:
            on_obp0_write(gba, value);
            break;

        case 0x49:
            on_obp1_write(gba, value);
            break;

        case 0x4A:
            IO_WY = value;
            break;

        case 0x4B:
            IO_WX = value;
            break;

        case 0x50: // unused (bootrom?)
            break;

        // these registers are r/w always on cgb/agb
        case 0x72:
            IO_72 = value;
            break;

        case 0x73:
            IO_73 = value;
            break;

        case 0x75:
            IO_75 = value;
            break;

        default:
            if (is_system_gbc(gba))
            {
                write_io_gbc(gba, addr, value);
            }
            break;
    }
}

using ReadFunction = u8(*)(Gba& gba, u16 addr);
using WriteFunction = void(*)(Gba& gba, u16 addr, u8 value);

// either 0xE or 0xF, so even / odd.
// the next important value is the upper nibble or low byte.
// because of this, we can (>> 4), then mask 0xF.
// we can also mask 0x10 to see if its even / odd
constexpr auto get_table_index(u16 addr)
{
    return (addr >> 4) & 0x1F;
}

consteval auto setup_read_function_table()
{
    std::array<ReadFunction, 0x20> table{};
    table.fill(read_unusable);

    for (auto i = 0; i < 0x20; i++)
    {
        if (i >= 0x00 && i <= 0x09)
        {
            table[i] = read_oam;
        }
        else if (i >= 0x10 && i <= 0x17)
        {
            table[i] = read_io;
        }
        else if (i >= 0x18 && i <= 0x1F)
        {
            table[i] = read_hram;
        }
    }

    return table;
}

consteval auto setup_write_function_table()
{
    std::array<WriteFunction, 0x20> table{};
    table.fill(write_unusable);

    for (auto i = 0; i < 0x20; i++)
    {
        if (i >= 0x00 && i <= 0x09)
        {
            table[i] = write_oam;
        }
        else if (i >= 0x10 && i <= 0x17)
        {
            table[i] = write_io;
        }
        else if (i >= 0x18 && i <= 0x1F)
        {
            table[i] = write_hram;
        }
    }

    return table;
}

constexpr auto READ_FUNCTION = setup_read_function_table();
constexpr auto WRITE_FUNCTION = setup_write_function_table();

} // namespace

auto ffread8(Gba& gba, u8 addr) -> u8
{
    if (addr <= 0x7F)
    {
        return read_io(gba, addr);
    }
    else
    {
        return read_hram(gba, addr);
    }
}

void ffwrite8(Gba& gba, u8 addr, u8 value)
{
    if (addr <= 0x7F)
    {
        write_io(gba, addr, value);
    }
    else
    {
        write_hram(gba, addr, value);
    }
}

auto read8(Gba& gba, const u16 addr) -> u8
{
    if (addr < 0xFE00) [[likely]]
    {
        const auto& entry = gba.gameboy.rmap[addr >> 12];
        assert(entry.ptr);
        return entry.ptr[addr & entry.mask];
    }
    else
    {
        return READ_FUNCTION[get_table_index(addr)](gba, addr);
    }
}

void write8(Gba& gba, u16 addr, u8 value)
{
    if (addr < 0xFE00) [[likely]]
    {
        // currently, this will break vram writes when ppu is in mode 3
        // as writes to vram whilst in mode3 are not allowed
        #if 0
        const auto& entry = gba.gameboy.wmap[addr >> 12];
        if (entry.ptr) [[likely]]
        {
            entry.ptr[addr & entry.mask] = value;
        }
        else
        {
            assert(((addr >> 12) & 0xF) >= 0x0 && ((addr >> 12) & 0xF) <= 0xB);
            mbc_write(gba, addr, value);
        }
        #else
        switch ((addr >> 12) & 0xF)
        {
            case 0x0: case 0x1: case 0x2: case 0x3: case 0x4:
            case 0x5: case 0x6: case 0x7: case 0xA: case 0xB:
                mbc_write(gba, addr, value);
                break;

            case 0x8: case 0x9:
                write_vram(gba, addr, value);
                break;

            case 0xC: case 0xE:
                gba.gameboy.wram[0][addr & 0x0FFF] = value;
                break;

            case 0xD: case 0xF:
                gba.gameboy.wram[gba.gameboy.mem.svbk][addr & 0x0FFF] = value;
                break;
        }
        #endif
    }
    else
    {
        WRITE_FUNCTION[get_table_index(addr)](gba, addr, value);
    }
}

auto read16(Gba& gba, u16 addr) -> u16
{
    const u16 lo = read8(gba, addr + 0);
    const u16 hi = read8(gba, addr + 1);

    return (hi << 8) | lo;
}

void write16(Gba& gba, u16 addr, u16 value)
{
    write8(gba, addr + 0, value & 0xFF);
    write8(gba, addr + 1, value >> 8);
}

void update_rom_banks(Gba& gba)
{
    static u8 dummy_write;
    const auto rom_bank0 = mbc_get_rom_bank(gba, 0);
    const auto rom_bankx = mbc_get_rom_bank(gba, 1);

    gba.gameboy.rmap[0x0] = rom_bank0.entries[0];
    gba.gameboy.rmap[0x1] = rom_bank0.entries[1];
    gba.gameboy.rmap[0x2] = rom_bank0.entries[2];
    gba.gameboy.rmap[0x3] = rom_bank0.entries[3];

    gba.gameboy.rmap[0x4] = rom_bankx.entries[0];
    gba.gameboy.rmap[0x5] = rom_bankx.entries[1];
    gba.gameboy.rmap[0x6] = rom_bankx.entries[2];
    gba.gameboy.rmap[0x7] = rom_bankx.entries[3];

    for (auto i = 0; i < 0x8; i++)
    {
        // todo: what's going on here
        gba.gameboy.wmap[i].ptr = &dummy_write;
        gba.gameboy.wmap[i].ptr = nullptr;
        gba.gameboy.wmap[i].mask = 0x00;
    }
}

void update_ram_banks(Gba& gba)
{
    const auto ram = mbc_get_ram_bank(gba);

    gba.gameboy.rmap[0xA] = ram.r[0];
    gba.gameboy.rmap[0xB] = ram.r[1];

    gba.gameboy.wmap[0xA] = ram.w[0];
    gba.gameboy.wmap[0xB] = ram.w[1];
}

void update_vram_banks(Gba& gba)
{
    gba.gameboy.rmap[0x8].mask = 0x0FFF;
    gba.gameboy.rmap[0x9].mask = 0x0FFF;

    gba.gameboy.wmap[0x8].mask = 0x0FFF;
    gba.gameboy.wmap[0x9].mask = 0x0FFF;

    if (is_system_gbc(gba))
    {
        gba.gameboy.rmap[0x8].ptr = gba.gameboy.vram[gba.gameboy.mem.vbk] + 0x0000;
        gba.gameboy.rmap[0x9].ptr = gba.gameboy.vram[gba.gameboy.mem.vbk] + 0x1000;

        gba.gameboy.wmap[0x8].ptr = gba.gameboy.vram[gba.gameboy.mem.vbk] + 0x0000;
        gba.gameboy.wmap[0x9].ptr = gba.gameboy.vram[gba.gameboy.mem.vbk] + 0x1000;
    }
    else
    {
        gba.gameboy.rmap[0x8].ptr = gba.gameboy.vram[0] + 0x0000;
        gba.gameboy.rmap[0x9].ptr = gba.gameboy.vram[0] + 0x1000;

        gba.gameboy.wmap[0x8].ptr = gba.gameboy.vram[0] + 0x0000;
        gba.gameboy.wmap[0x9].ptr = gba.gameboy.vram[0] + 0x1000;
    }
}

void update_wram_banks(Gba& gba)
{
    gba.gameboy.rmap[0xC].mask = 0x0FFF;
    gba.gameboy.rmap[0xD].mask = 0x0FFF;
    gba.gameboy.rmap[0xE].mask = 0x0FFF;
    gba.gameboy.rmap[0xF].mask = 0x0FFF;

    gba.gameboy.wmap[0xC].mask = 0x0FFF;
    gba.gameboy.wmap[0xD].mask = 0x0FFF;
    gba.gameboy.wmap[0xE].mask = 0x0FFF;
    gba.gameboy.wmap[0xF].mask = 0x0FFF;

    gba.gameboy.rmap[0xC].ptr = gba.gameboy.wram[0];
    gba.gameboy.rmap[0xE].ptr = gba.gameboy.wram[0];

    gba.gameboy.wmap[0xC].ptr = gba.gameboy.wram[0];
    gba.gameboy.wmap[0xE].ptr = gba.gameboy.wram[0];

    if (is_system_gbc(gba))
    {
        gba.gameboy.rmap[0xD].ptr = gba.gameboy.wram[gba.gameboy.mem.svbk];
        gba.gameboy.rmap[0xF].ptr = gba.gameboy.wram[gba.gameboy.mem.svbk];

        gba.gameboy.wmap[0xD].ptr = gba.gameboy.wram[gba.gameboy.mem.svbk];
        gba.gameboy.wmap[0xF].ptr = gba.gameboy.wram[gba.gameboy.mem.svbk];
    }
    else
    {
        gba.gameboy.rmap[0xD].ptr = gba.gameboy.wram[1];
        gba.gameboy.rmap[0xF].ptr = gba.gameboy.wram[1];

        gba.gameboy.wmap[0xD].ptr = gba.gameboy.wram[1];
        gba.gameboy.wmap[0xF].ptr = gba.gameboy.wram[1];
    }
}

void setup_mmap(Gba& gba)
{
    update_rom_banks(gba);
    update_ram_banks(gba);
    update_vram_banks(gba);
    update_wram_banks(gba);
}

} // namespace gba::gb
