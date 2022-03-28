// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "mem.hpp"
#include "apu/apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "backup/backup.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "ppu.hpp"
#include "scheduler.hpp"
#include "timer.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <span>
#include <ranges>

namespace gba::mem {

// General Internal Memory
constexpr auto BIOS_MASK        = 0x00003FFF;
constexpr auto EWRAM_MASK       = 0x0003FFFF;
constexpr auto IWRAM_MASK       = 0x00007FFF;
constexpr auto IO_MASK          = 0x3FF;
// Internal Display Memory
constexpr auto PALETTE_RAM_MASK = 0x000003FF;
constexpr auto VRAM_MASK        = 0x0001FFFF;
constexpr auto OAM_MASK         = 0x000003FF;
// External Memory (Game Pak)
constexpr auto ROM_MASK         = 0x01FFFFFF;

// General Internal Memory
constexpr auto BIOS_SIZE        = BIOS_MASK + 1;
constexpr auto EWRAM_SIZE       = EWRAM_MASK + 1;
constexpr auto IWRAM_SIZE       = IWRAM_MASK + 1;
constexpr auto IO_SIZE          = IO_MASK + 1;
// Internal Display Memory
constexpr auto PALETTE_RAM_SIZE = PALETTE_RAM_MASK + 1;
constexpr auto VRAM_SIZE        = 0x00017FFF + 1;
constexpr auto OAM_SIZE         = OAM_MASK + 1;
// External Memory (Game Pak)
constexpr auto ROM_SIZE         = ROM_MASK + 1;

#define MEM gba.mem

auto setup_tables(Gba& gba) -> void
{
    std::ranges::fill(MEM.rmap_8, ReadArray{});
    std::ranges::fill(MEM.rmap_16, ReadArray{});
    std::ranges::fill(MEM.rmap_32, ReadArray{});
    std::ranges::fill(MEM.wmap_8, WriteArray{});
    std::ranges::fill(MEM.wmap_16, WriteArray{});
    std::ranges::fill(MEM.wmap_32, WriteArray{});

    MEM.rmap_16[0x0] = {gba.mem.bios, BIOS_MASK};
    MEM.rmap_16[0x2] = {gba.mem.ewram, EWRAM_MASK};
    MEM.rmap_16[0x3] = {gba.mem.iwram, IWRAM_MASK};
    MEM.rmap_16[0x5] = {gba.mem.palette_ram, PALETTE_RAM_MASK};
    MEM.rmap_16[0x7] = {gba.mem.oam, OAM_MASK};
    MEM.rmap_16[0x8] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0x9] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xA] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xB] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xC] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xD] = {gba.rom, ROM_MASK};

    MEM.wmap_16[0x2] = {gba.mem.ewram, EWRAM_MASK};
    MEM.wmap_16[0x3] = {gba.mem.iwram, IWRAM_MASK};
    MEM.wmap_16[0x5] = {gba.mem.palette_ram, PALETTE_RAM_MASK};
    MEM.wmap_16[0x7] = {gba.mem.oam, OAM_MASK};

    // this will be handled by the function handlers
    if (gba.backup.type == backup::Type::EEPROM)
    {
        MEM.rmap_16[0xD] = {};
        MEM.wmap_16[0xD] = {};
    }

    std::ranges::copy(MEM.rmap_16, MEM.rmap_8);
    std::ranges::copy(MEM.wmap_16, MEM.wmap_8);
    std::ranges::copy(MEM.rmap_16, MEM.rmap_32);
    std::ranges::copy(MEM.wmap_16, MEM.wmap_32);

    // tonc says byte reads are okay
    MEM.wmap_8[0x5] = {}; // ignore palette ram byte stores
    MEM.wmap_8[0x6] = {}; // ignore vram byte stores
    MEM.wmap_8[0x7] = {}; // ignore oam byte stores
}

auto reset(Gba& gba) -> void
{
    std::ranges::fill(MEM.ewram, 0);
    std::ranges::fill(MEM.iwram, 0);
    std::ranges::fill(MEM.palette_ram, 0);
    std::ranges::fill(MEM.vram, 0);
    std::ranges::fill(MEM.oam, 0);
    REG_KEY = 0xFFFF;

    setup_tables(gba);
}

[[nodiscard]]
constexpr auto read_io16(Gba& gba, std::uint32_t addr) -> std::uint16_t
{
    switch (addr)
    {
        // todo: fix this for scheduler timers
        #if ENABLE_SCHEDULER
        case IO_TM0D: return timer::read_timer(gba, 0);
        case IO_TM1D: return timer::read_timer(gba, 1);
        case IO_TM2D: return timer::read_timer(gba, 2);
        case IO_TM3D: return timer::read_timer(gba, 3);
        #else
        case IO_TM0D: return REG_TM0D;
        case IO_TM1D: return REG_TM1D;
        case IO_TM2D: return REG_TM2D;
        case IO_TM3D: return REG_TM3D;
        #endif

        // write only regs
        // todo: use a lut
        case IO_BG0HOFS:
        case IO_BG0VOFS:
        case IO_BG1HOFS:
        case IO_BG1VOFS:
        case IO_BG2HOFS:
        case IO_BG2VOFS:
        case IO_BG3HOFS:
        case IO_BG3VOFS:
            return 0;

        default:
            //printf("unhandled io read addr: 0x%08X\n", addr);
            return read_array<u16>(MEM.io, IO_MASK, addr);
    }
}

[[nodiscard]]
constexpr auto read_io8(Gba& gba, std::uint32_t addr) -> std::uint8_t
{
    const auto value = read_io16(gba, addr & ~0x1);

    if (addr & 1) // do we want to lo or hi byte?
    {
        return value >> 8; // the hi byte
    }
    else
    {
        return value & 0xFF; // the lo byte
    }
}

[[nodiscard]]
constexpr auto read_io32(Gba& gba, std::uint32_t addr) -> std::uint32_t
{
    // todo: optimise for 32bit regs that games commonly read from
    const std::uint32_t lo = read_io16(gba, addr+0) << 0;
    const std::uint32_t hi = read_io16(gba, addr+2) << 16;
    return hi | lo;
}

constexpr auto write_io16(Gba& gba, std::uint32_t addr, std::uint16_t value) -> void
{
    switch (addr)
    {
        // read only regs
        case IO_KEY: return;
        case IO_VCOUNT: return;

        case IO_TM0D: gba.timer[0].reload = value; return;
        case IO_TM1D: gba.timer[1].reload = value; return;
        case IO_TM2D: gba.timer[2].reload = value; return;
        case IO_TM3D: gba.timer[3].reload = value; return;

        case IO_IF:
            REG_IF &= ~value;
            return;

        case IO_DISPSTAT:
            REG_DISPSTAT = (REG_DISPSTAT & 0x7) | (value & ~0x7);
            return;
    }

    write_array<u16>(MEM.io, IO_MASK, addr, value);

    switch (addr)
    {
        // todo: read only when apu is off
        case mem::IO_SOUND1CNT_L:
        case mem::IO_SOUND1CNT_H:
        case mem::IO_SOUND1CNT_X:
        case mem::IO_SOUND2CNT_L:
        case mem::IO_SOUND2CNT_H:
        case mem::IO_SOUND3CNT_L:
        case mem::IO_SOUND3CNT_H:
        case mem::IO_SOUND3CNT_X:
        case mem::IO_SOUND4CNT_L:
        case mem::IO_SOUND4CNT_H:
        case mem::IO_SOUNDCNT_L:
        case mem::IO_SOUNDCNT_X:
        case mem::IO_WAVE_RAM0_L:
        case mem::IO_WAVE_RAM0_H:
        case mem::IO_WAVE_RAM1_L:
        case mem::IO_WAVE_RAM1_H:
        case mem::IO_WAVE_RAM2_L:
        case mem::IO_WAVE_RAM2_H:
        case mem::IO_WAVE_RAM3_L:
        case mem::IO_WAVE_RAM3_H:
            apu::write_legacy(gba, addr, value);
            break;

        case IO_TM0CNT: timer::on_cnt0_write(gba, REG_TM0CNT); break;
        case IO_TM1CNT: timer::on_cnt1_write(gba, REG_TM1CNT); break;
        case IO_TM2CNT: timer::on_cnt2_write(gba, REG_TM2CNT); break;
        case IO_TM3CNT: timer::on_cnt3_write(gba, REG_TM3CNT); break;

        case IO_DMA0CNT: break;
        case IO_DMA0CNT + 2:
            dma::on_cnt_write(gba, 0);
            break;

        case IO_DMA1CNT: break;
        case IO_DMA1CNT + 2:
            dma::on_cnt_write(gba, 1);
            break;

        case IO_DMA2CNT: break;
        case IO_DMA2CNT + 2:
            dma::on_cnt_write(gba, 2);
            break;

        case IO_DMA3CNT: break;
        case IO_DMA3CNT + 2:
            dma::on_cnt_write(gba, 3);
            break;

        case IO_IME:
            arm7tdmi::schedule_interrupt(gba);
            break;

        case IO_HALTCNT_L:
        case IO_HALTCNT_H:
            arm7tdmi::on_halt_trigger(gba, arm7tdmi::HaltType::write);
            break;

        case IO_IE:
            arm7tdmi::schedule_interrupt(gba);
            break;

        case IO_FIFO_A_L:
        case IO_FIFO_A_H:
            apu::on_fifo_write16(gba, value, 0);
            break;

        case IO_FIFO_B_L:
        case IO_FIFO_B_H:
            apu::on_fifo_write16(gba, value, 1);
            break;

        case IO_SOUNDCNT_H:
            apu::on_soundcnt_write(gba);
            break;

        default:
            //printf("unhandled io write addr: 0x%08X value: 0x%02X pc: 0x%08X\n", addr, value, arm7tdmi::get_pc(gba));
            // assert(!"todo");
            break;
    }
}

constexpr auto write_io32(Gba& gba, std::uint32_t addr, std::uint32_t value) -> void
{
    // games typically do 32-bit writes to 32-bit registers

    switch (addr)
    {
        case IO_DISPCNT:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            return;

        case IO_IME:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            arm7tdmi::schedule_interrupt(gba);
            return;

        case IO_DMA0SAD:
        case IO_DMA1SAD:
        case IO_DMA2SAD:
        case IO_DMA3SAD:
        case IO_DMA0DAD:
        case IO_DMA1DAD:
        case IO_DMA2DAD:
        case IO_DMA3DAD:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            return;

        case IO_DMA0CNT:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            dma::on_cnt_write(gba, 0);
            return;

        case IO_DMA1CNT:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            dma::on_cnt_write(gba, 1);
            return;

        case IO_DMA2CNT:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            dma::on_cnt_write(gba, 2);
            return;

        case IO_DMA3CNT:
            write_array<u32>(MEM.io, IO_MASK, addr, value);
            dma::on_cnt_write(gba, 3);
            return;

        case IO_FIFO_A_L:
        case IO_FIFO_A_H:
            apu::on_fifo_write32(gba, value, 0);
            return;

        case IO_FIFO_B_L:
        case IO_FIFO_B_H:
            apu::on_fifo_write32(gba, value, 1);
            return;
    }

    // std::printf("[IO] 32bit write to 0x%08X\n", addr);
    write_io16(gba, addr + 0, value >> 0x00);
    write_io16(gba, addr + 2, value >> 0x10);
}

constexpr auto write_io8(Gba& gba, std::uint32_t addr, std::uint8_t value) -> void
{
    // printf("bit io write to 0x%08X\n", addr);
    switch (addr)
    {
        case IO_SOUND1CNT_L + 0:
        case IO_SOUND1CNT_H + 0:
        case IO_SOUND1CNT_H + 1:
        case IO_SOUND1CNT_X + 0:
        case IO_SOUND1CNT_X + 1:
        case IO_SOUND2CNT_L + 0:
        case IO_SOUND2CNT_L + 1:
        case IO_SOUND2CNT_H + 0:
        case IO_SOUND2CNT_H + 1:
        case IO_SOUND3CNT_L + 0:
        case IO_SOUND3CNT_H + 0:
        case IO_SOUND3CNT_H + 1:
        case IO_SOUND3CNT_X + 0:
        case IO_SOUND3CNT_X + 1:
        case IO_SOUND4CNT_L + 0:
        case IO_SOUND4CNT_L + 1:
        case IO_SOUND4CNT_H + 0:
        case IO_SOUND4CNT_H + 1:
        case IO_WAVE_RAM0_L + 0:
        case IO_WAVE_RAM0_L + 1:
        case IO_WAVE_RAM0_H + 0:
        case IO_WAVE_RAM0_H + 1:
        case IO_WAVE_RAM1_L + 0:
        case IO_WAVE_RAM1_L + 1:
        case IO_WAVE_RAM1_H + 0:
        case IO_WAVE_RAM1_H + 1:
        case IO_WAVE_RAM2_L + 0:
        case IO_WAVE_RAM2_L + 1:
        case IO_WAVE_RAM2_H + 0:
        case IO_WAVE_RAM2_H + 1:
        case IO_WAVE_RAM3_L + 0:
        case IO_WAVE_RAM3_L + 1:
        case IO_WAVE_RAM3_H + 0:
        case IO_WAVE_RAM3_H + 1:
            // printf("8bit apu write: 0x%08X\n", addr);
            write_array<u8>(MEM.io, IO_MASK, addr, value);
            apu::write_legacy8(gba, addr, value);
            return;

        case IO_IF + 0:
            REG_IF &= ~value;
            return;

        case IO_IF + 1:
            REG_IF &= ~(value << 8);
            return;

        case IO_FIFO_A_L:
        case IO_FIFO_A_L+1:
        case IO_FIFO_A_H:
        case IO_FIFO_A_H+1:
            apu::on_fifo_write8(gba, value, 0);
            return;

        case IO_FIFO_B_L:
        case IO_FIFO_B_L+1:
        case IO_FIFO_B_H:
        case IO_FIFO_B_H+1:
            apu::on_fifo_write8(gba, value, 1);
            return;

        case IO_IME:
            write_array<u8>(MEM.io, IO_MASK, addr, value);
            arm7tdmi::schedule_interrupt(gba);
            return;

        case IO_HALTCNT_H:
            arm7tdmi::on_halt_trigger(gba, arm7tdmi::HaltType::write);
            return;
    }

    u16 actual_value = value;
    if (addr & 1)
    {
        actual_value <<= 8;
        actual_value |= MEM.io[addr & 0x3FF];
    }
    else
    {
        actual_value |= static_cast<std::uint16_t>(MEM.io[addr & 0x3FF]) << 8;
    }

    write_io16(gba, addr & ~0x1, actual_value);
}

template<typename T> [[nodiscard]]
constexpr T read_io_region(Gba& gba, u32 addr)
{
    if constexpr(sizeof(T) == 4)
    {
        return read_io32(gba, addr);
    }
    else if constexpr(sizeof(T) == 2)
    {
        return read_io16(gba, addr);
    }
    else if constexpr(sizeof(T) == 1)
    {
        return read_io8(gba, addr);
    }
}

template<typename T>
constexpr void write_io_region(Gba& gba, u32 addr, T value)
{
    if constexpr(sizeof(T) == 4)
    {
        write_io32(gba, addr, value);
    }
    else if constexpr(sizeof(T) == 2)
    {
        write_io16(gba, addr, value);
    }
    else if constexpr(sizeof(T) == 1)
    {
        write_io8(gba, addr, value);
    }
}

// https://github.com/mgba-emu/mgba/issues/743 (thanks endrift)
constexpr auto is_vram_dma_overflow_bug(Gba& gba, u32 addr)
{
    return addr > VRAM_MASK && (addr & (VRAM_SIZE | 0x14000)) == VRAM_SIZE && ppu::is_bitmap_mode(gba);
}

template<typename T> [[nodiscard]]
constexpr auto read_vram_region(Gba& gba, u32 addr) -> T
{
    if ((addr & VRAM_MASK) > 0x17FFF)
    {
        if (is_vram_dma_overflow_bug(gba, addr)) [[unlikely]]
        {
            return 0;
        }
        addr -= 0x8000;
    }
    return read_array<T>(MEM.vram, VRAM_MASK, addr);
}

template<typename T>
constexpr auto write_vram_region(Gba& gba, u32 addr, T value) -> void
{
    if ((addr & VRAM_MASK) > 0x17FFF)
    {
        if (is_vram_dma_overflow_bug(gba, addr)) [[unlikely]]
        {
            return;
        }
        addr -= 0x8000;
    }

    if constexpr(sizeof(T) == 1)
    {
        const bool bitmap = ppu::is_bitmap_mode(gba);
        const u32 end_region = bitmap ? 0x6013FFF : 0x600FFFF;

        // if we are in this region, then we do a 16bit write
        // where the 8bit value is written as the upper / lower half
        if (addr <= end_region)
        {
            // align to 16bits
            addr &= ~0x1;
            const u16 new_value = (value << 8) | value;
            write_array<u16>(MEM.vram, VRAM_MASK, addr, new_value);
        }
    }
    else
    {
        write_array<T>(MEM.vram, VRAM_MASK, addr, value);
    }
}

template<typename T>
constexpr auto write_pram_region(Gba& gba, u32 addr, T value) -> void
{
    if constexpr(sizeof(T) == 1)
    {
        const u32 end_region = 0x50003FF;

        // if we are in this region, then we do a 16bit write
        // where the 8bit value is written as the upper / lower half
        if (addr <= end_region)
        {
            // align to 16bits
            addr &= ~0x1;
            const u16 new_value = (value << 8) | value;

            write_array<u16>(MEM.palette_ram, PALETTE_RAM_MASK, addr, new_value);
        }
    }
}

template<typename T> [[nodiscard]]
constexpr auto read_eeprom_region(Gba& gba, u32 addr) -> T
{
    if (gba.backup.type == backup::Type::EEPROM)
    {
        T value{};
        // todo: check rom size for region access
        if constexpr(sizeof(T) == 1)
        {
            value |= gba.backup.eeprom.read(gba, addr);
        }
        if constexpr(sizeof(T) == 2)
        {
            value |= gba.backup.eeprom.read(gba, addr);
        }
        if constexpr(sizeof(T) == 4)
        {
            assert(!"32bit read from eeprom");
            value |= gba.backup.eeprom.read(gba, addr+0) << 0;
            value |= gba.backup.eeprom.read(gba, addr+1) << 16;
        }
        return value;
    }
    else
    {
        return read_array<T>(gba.rom, ROM_MASK, addr);
    }
}

template<typename T>
constexpr auto write_eeprom_region(Gba& gba, u32 addr, T value) -> void
{
    if (gba.backup.type == backup::Type::EEPROM)
    {
        // todo: check rom size for region access
        if constexpr(sizeof(T) == 1)
        {
            gba.backup.eeprom.write(gba, addr, value);
        }
        if constexpr(sizeof(T) == 2)
        {
            gba.backup.eeprom.write(gba, addr, value);
        }
        if constexpr(sizeof(T) == 4)
        {
            assert(!"32bit write to eeprom");
            std::printf("32bit write to eeprom\n");
            gba.backup.eeprom.write(gba, addr+0, value>>0);
            gba.backup.eeprom.write(gba, addr+1, value>>16);
        }
    }
}

template<typename T> [[nodiscard]]
constexpr auto read_sram_region(Gba& gba, u32 addr) -> T
{
    T value{};
    const auto backup_type = gba.backup.type;

    if (backup_type == backup::Type::SRAM)
    {
        value = gba.backup.sram.read(gba, addr);
    }
    else if (backup_type == backup::Type::FLASH || backup_type == backup::Type::FLASH1M || backup_type == backup::Type::FLASH512)
    {
        value = gba.backup.flash.read(gba, addr);
    }
    else // no backup. todo: open bus
    {
        return 0;
    }

    // 16/32bit reads from sram area mirror the byte
    if constexpr(sizeof(T) == 2)
    {
        value |= value << 8;
    }
    else if constexpr(sizeof(T) == 4)
    {
        value |= value << 8;
        value |= value << 16;
    }

    return value;
}

template<typename T>
constexpr auto write_sram_region(Gba& gba, u32 addr, T value) -> void
{
    const auto backup_type = gba.backup.type;

    if (backup_type == backup::Type::SRAM)
    {
        gba.backup.sram.write(gba, addr, value);
    }
    else if (backup_type == backup::Type::FLASH || backup_type == backup::Type::FLASH1M || backup_type == backup::Type::FLASH512)
    {
        gba.backup.flash.write(gba, addr, value);
    }
}

template<typename T>
constexpr T empty_read(Gba& gba, u32 addr)
{
    return 0;
}

template<typename T>
constexpr void empty_write(Gba& gba, u32 addr, T value)
{
}

template<typename T>
using ReadFunction = T(*)(Gba& gba, u32 addr);
template<typename T>
using WriteFunction = void(*)(Gba& gba, u32 addr, T value);

template<typename T>
constexpr ReadFunction<T> READ_FUNCTION[0x10] =
{
    /*[0x0] =*/ empty_read,
    /*[0x1] =*/ empty_read,
    /*[0x2] =*/ empty_read,
    /*[0x3] =*/ empty_read,
    /*[0x4] =*/ read_io_region<T>,
    /*[0x5] =*/ empty_read,
    /*[0x6] =*/ read_vram_region<T>,
    /*[0x7] =*/ empty_read,
    /*[0x8] =*/ empty_read,
    /*[0x9] =*/ empty_read,
    /*[0xA] =*/ empty_read,
    /*[0xB] =*/ empty_read,
    /*[0xC] =*/ empty_read,
    /*[0xD] =*/ read_eeprom_region<T>,
    /*[0xE] =*/ read_sram_region<T>,
    /*[0xF] =*/ empty_read,
};

template<typename T>
constexpr WriteFunction<T> WRITE_FUNCTION[0x10] =
{
    /*[0x0] =*/ empty_write,
    /*[0x1] =*/ empty_write,
    /*[0x2] =*/ empty_write,
    /*[0x3] =*/ empty_write,
    /*[0x4] =*/ write_io_region<T>,
    /*[0x5] =*/ write_pram_region<T>,
    /*[0x6] =*/ write_vram_region<T>,
    /*[0x7] =*/ empty_write,
    /*[0x8] =*/ empty_write,
    /*[0x9] =*/ empty_write,
    /*[0xA] =*/ empty_write,
    /*[0xB] =*/ empty_write,
    /*[0xC] =*/ empty_write,
    /*[0xD] =*/ write_eeprom_region<T>,
    /*[0xE] =*/ write_sram_region<T>,
    /*[0xF] =*/ empty_write,
};

// all these functions are inlined
auto read8(Gba& gba, std::uint32_t addr) -> std::uint8_t
{
    gba.cycles++;
    auto& entry = MEM.rmap_8[(addr >> 24) & 0xF];

    if (entry.array.size()) [[likely]]
    {
        return read_array<u8>(entry.array, entry.mask, addr);
    }
    else
    {
        return READ_FUNCTION<u8>[(addr >> 24) & 0xF](gba, addr);
    }
}

auto read16(Gba& gba, std::uint32_t addr) -> std::uint16_t
{
    addr &= ~0x1;
    gba.cycles++;
    auto& entry = MEM.rmap_16[(addr >> 24) & 0xF];

    if (entry.array.size()) [[likely]]
    {
        return read_array<u16>(entry.array, entry.mask, addr);
    }
    else
    {
        return READ_FUNCTION<u16>[(addr >> 24) & 0xF](gba, addr);
    }
}

auto read32(Gba& gba, std::uint32_t addr) -> std::uint32_t
{
    addr &= ~0x3;
    gba.cycles++;
    auto& entry = MEM.rmap_32[(addr >> 24) & 0xF];

    if (entry.array.size()) [[likely]]
    {
        return read_array<u32>(entry.array, entry.mask, addr);
    }
    else
    {
        return READ_FUNCTION<u32>[(addr >> 24) & 0xF](gba, addr);
    }
}

auto write8(Gba& gba, std::uint32_t addr, std::uint8_t value) -> void
{
    gba.cycles++;
    auto& entry = MEM.wmap_8[(addr >> 24) & 0xF];

    if (entry.array.size()) // don't mark likely as vram,pram,io writes are common
    {
        write_array<u8>(entry.array, entry.mask, addr, value);
    }
    else
    {
        WRITE_FUNCTION<u8>[(addr >> 24) & 0xF](gba, addr, value);
    }
}

auto write16(Gba& gba, std::uint32_t addr, std::uint16_t value) -> void
{
    addr &= ~0x1;
    gba.cycles++;
    auto& entry = MEM.wmap_16[(addr >> 24) & 0xF];

    if (entry.array.size()) // don't mark likely as vram,pram,io writes are common
    {
        write_array<u16>(entry.array, entry.mask, addr, value);
    }
    else
    {
        WRITE_FUNCTION<u16>[(addr >> 24) & 0xF](gba, addr, value);
    }
}

auto write32(Gba& gba, std::uint32_t addr, std::uint32_t value) -> void
{
    addr &= ~0x3;
    gba.cycles++;
    auto& entry = MEM.wmap_32[(addr >> 24) & 0xF];

    if (entry.array.size()) // don't mark likely as vram,pram,io writes are common
    {
        write_array<u32>(entry.array, entry.mask, addr, value);
    }
    else
    {
        WRITE_FUNCTION<u32>[(addr >> 24) & 0xF](gba, addr, value);
    }
}

} // namespace gba::mem
