// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "mem.hpp"
#include "apu/apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "backup/backup.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "ppu/ppu.hpp"
#include "scheduler.hpp"
#include "timer.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <span>
#include <ranges>
#include <type_traits>

namespace gba::mem {

// General Internal Memory
constexpr auto BIOS_MASK        = 0x00003FFF;
constexpr auto EWRAM_MASK       = 0x0003FFFF;
constexpr auto IWRAM_MASK       = 0x00007FFF;
constexpr auto IO_MASK          = 0x3FF;
// Internal Display Memory
constexpr auto PRAM_MASK = 0x000003FF;
constexpr auto VRAM_MASK        = 0x0001FFFF;
constexpr auto OAM_MASK         = 0x000003FF;
// External Memory (Game Pak)
constexpr auto ROM_MASK         = 0x01FFFFFF;

// General Internal Memory
[[maybe_unused]] constexpr auto BIOS_SIZE        = BIOS_MASK + 1;
[[maybe_unused]] constexpr auto EWRAM_SIZE       = EWRAM_MASK + 1;
[[maybe_unused]] constexpr auto IWRAM_SIZE       = IWRAM_MASK + 1;
[[maybe_unused]] constexpr auto IO_SIZE          = IO_MASK + 1;
// Internal Display Memory
[[maybe_unused]] constexpr auto pram_SIZE = PRAM_MASK + 1;
[[maybe_unused]] constexpr auto VRAM_SIZE        = 0x00017FFF + 1;
[[maybe_unused]] constexpr auto OAM_SIZE         = OAM_MASK + 1;
// External Memory (Game Pak)
[[maybe_unused]] constexpr auto ROM_SIZE         = ROM_MASK + 1;

#define MEM gba.mem

static constexpr auto mirror_address(u32 addr) -> u32
{
    return addr & 0x0FFFFFFF;
}

template<typename T>
static constexpr auto align_address(u32 addr) -> u32
{
    if constexpr(std::is_same<T, u8>())
    {
        return addr;
    }
    if constexpr(std::is_same<T, u16>())
    {
        return addr & ~0x1;
    }
    if constexpr(std::is_same<T, u32>())
    {
        return addr & ~0x3;
    }
}

[[nodiscard]]
static inline auto get_memory_timing(u8 index, u32 addr) -> u8
{
    // https://problemkaputt.de/gbatek.htm#gbamemorymap
    static constexpr u8 timings[3][0x10] = // this has to be static otherwise it's slow
    {
        { 1, 1, 3, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 5, 1, },
        { 1, 1, 3, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 5, 1, },
        { 1, 1, 6, 1, 1, 2, 2, 1, 4, 4, 4, 4, 4, 4, 8, 1, },
    };

    return timings[index & 3][(addr >> 24) & 0xF];
}

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
    MEM.rmap_16[0x5] = {gba.mem.pram, PRAM_MASK};
    MEM.rmap_16[0x7] = {gba.mem.oam, OAM_MASK};
    MEM.rmap_16[0x8] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0x9] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xA] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xB] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xC] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xD] = {gba.rom, ROM_MASK};

    MEM.wmap_16[0x2] = {gba.mem.ewram, EWRAM_MASK};
    MEM.wmap_16[0x3] = {gba.mem.iwram, IWRAM_MASK};
    // MEM.wmap_16[0x5] = {gba.mem.pram, PRAM_MASK};
    // MEM.wmap_16[0x7] = {gba.mem.oam, OAM_MASK};

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
    std::ranges::fill(MEM.pram, 0);
    std::ranges::fill(MEM.vram, 0);
    std::ranges::fill(MEM.oam, 0);
    REG_KEY = 0xFFFF;

    setup_tables(gba);
}

template<typename T>
static constexpr auto empty_read([[maybe_unused]] Gba& gba, [[maybe_unused]] u32 addr) -> T
{
    // std::printf("empty read: 0x%08X\n", addr);
    return 0x0;
}

template<typename T>
static constexpr auto empty_write([[maybe_unused]] Gba& gba, [[maybe_unused]] u32 addr, [[maybe_unused]] T value) -> void
{
}

[[nodiscard]]
static constexpr auto read_io16(Gba& gba, u32 addr) -> u16
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
        case IO_FIFO_A_L:
        case IO_FIFO_A_H:
        case IO_FIFO_B_L:
        case IO_FIFO_B_H:
        case IO_DMA0SAD:
        case IO_DMA1SAD:
        case IO_DMA2SAD:
        case IO_DMA3SAD:
        case IO_DMA0DAD:
        case IO_DMA1DAD:
        case IO_DMA2DAD:
        case IO_DMA3DAD:
        case IO_DMA0CNT_L:
        case IO_DMA1CNT_L:
        case IO_DMA2CNT_L:
        case IO_DMA3CNT_L:
        case IO_BG0HOFS:
        case IO_BG0VOFS:
        case IO_BG1HOFS:
        case IO_BG1VOFS:
        case IO_BG2HOFS:
        case IO_BG2VOFS:
        case IO_BG3HOFS:
        case IO_BG3VOFS:
        case IO_WIN0H:
        case IO_WIN1H:
        case IO_WIN0V:
        case IO_WIN1V:
        case IO_MOSAIC:
        case IO_COLEV:
        case IO_COLEY:
            return empty_read<u16>(gba, addr);

        default:
            //printf("unhandled io read addr: 0x%08X\n", addr);
            return read_array<u16>(MEM.io, IO_MASK, addr);
    }
}

[[nodiscard]]
static constexpr auto read_io8(Gba& gba, u32 addr) -> u8
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
static constexpr auto read_io32(Gba& gba, u32 addr) -> u32
{
    // todo: optimise for 32bit regs that games commonly read from
    const u32 lo = read_io16(gba, addr+0) << 0;
    const u32 hi = read_io16(gba, addr+2) << 16;
    return hi | lo;
}

static constexpr auto write_io16(Gba& gba, u32 addr, u16 value) -> void
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
        case IO_SOUND1CNT_L:
        case IO_SOUND1CNT_H:
        case IO_SOUND1CNT_X:
        case IO_SOUND2CNT_L:
        case IO_SOUND2CNT_H:
        case IO_SOUND3CNT_L:
        case IO_SOUND3CNT_H:
        case IO_SOUND3CNT_X:
        case IO_SOUND4CNT_L:
        case IO_SOUND4CNT_H:
        case IO_SOUNDCNT_L:
        case IO_SOUNDCNT_X:
        case IO_WAVE_RAM0_L:
        case IO_WAVE_RAM0_H:
        case IO_WAVE_RAM1_L:
        case IO_WAVE_RAM1_H:
        case IO_WAVE_RAM2_L:
        case IO_WAVE_RAM2_H:
        case IO_WAVE_RAM3_L:
        case IO_WAVE_RAM3_H:
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

static constexpr auto write_io32(Gba& gba, u32 addr, u32 value) -> void
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

static constexpr auto write_io8(Gba& gba, u32 addr, u8 value) -> void
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
        actual_value |= MEM.io[addr & 0x3FE];
    }
    else
    {
        actual_value |= static_cast<u16>(MEM.io[(addr+1) & 0x3FF]) << 8;
    }

    write_io16(gba, addr & ~0x1, actual_value);
}

template<typename T> [[nodiscard]]
constexpr T read_io_region(Gba& gba, u32 addr)
{
    addr = align_address<T>(addr);

    if constexpr(std::is_same<T, u32>())
    {
        return read_io32(gba, addr);
    }
    else if constexpr(std::is_same<T, u16>())
    {
        return read_io16(gba, addr);
    }
    else if constexpr(std::is_same<T, u8>())
    {
        return read_io8(gba, addr);
    }
}

template<typename T>
constexpr void write_io_region(Gba& gba, u32 addr, T value)
{
    addr = align_address<T>(addr);

    if constexpr(std::is_same<T, u32>())
    {
        write_io32(gba, addr, value);
    }
    else if constexpr(std::is_same<T, u16>())
    {
        write_io16(gba, addr, value);
    }
    else if constexpr(std::is_same<T, u8>())
    {
        write_io8(gba, addr, value);
    }
}

// unused, handled in array writes
template<typename T>
static constexpr auto write_ewram_region(Gba& gba, u32 addr, T value) -> void
{
    write_array<T>(MEM.ewram, EWRAM_MASK, addr, value);
}

// unused, handled in array writes
template<typename T>
static constexpr auto write_iwram_region(Gba& gba, u32 addr, T value) -> void
{
    write_array<T>(MEM.iwram, IWRAM_MASK, addr, value);
}

template<typename T>
static constexpr auto write_oam_region(Gba& gba, u32 addr, T value) -> void
{
    // only non-byte writes are allowed
    if constexpr(std::is_same<T, u8>() == false)
    {
        const auto dirty_index = (addr & OAM_MASK) / Gba::dirty_oam_shift;
        gba.dirty_oam[dirty_index] |= value != read_array<T>(MEM.oam, OAM_MASK, addr);
        gba.dirty_oam_any |= gba.dirty_oam[dirty_index];
        write_array<T>(MEM.oam, OAM_MASK, addr, value);
    }
}

// https://github.com/mgba-emu/mgba/issues/743 (thanks endrift)
static constexpr auto is_vram_dma_overflow_bug(Gba& gba, u32 addr)
{
    return addr > VRAM_MASK && (addr & (VRAM_SIZE | 0x14000)) == VRAM_SIZE && ppu::is_bitmap_mode(gba);
}

template<typename T> [[nodiscard]]
static constexpr auto read_vram_region(Gba& gba, u32 addr) -> T
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
static constexpr auto write_vram_region(Gba& gba, u32 addr, T value) -> void
{
    if ((addr & VRAM_MASK) > 0x17FFF)
    {
        if (is_vram_dma_overflow_bug(gba, addr)) [[unlikely]]
        {
            return;
        }
        addr -= 0x8000;
    }

    if constexpr(std::is_same<T, u8>())
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

            const auto dirty_index = (addr & VRAM_MASK) / Gba::dirty_vram_shift;
            gba.dirty_vram[dirty_index] |= new_value != read_array<u16>(MEM.vram, VRAM_MASK, addr);
            gba.dirty_vram_any |= gba.dirty_vram[dirty_index];

            write_array<u16>(MEM.vram, VRAM_MASK, addr, new_value);
        }
    }
    else
    {
        const auto dirty_index = (addr & VRAM_MASK) / Gba::dirty_vram_shift;
        gba.dirty_vram[dirty_index] |= value != read_array<T>(MEM.vram, VRAM_MASK, addr);
        gba.dirty_vram_any |= gba.dirty_vram[dirty_index];

        write_array<T>(MEM.vram, VRAM_MASK, addr, value);
    }
}

template<typename T>
static constexpr auto write_pram_region(Gba& gba, u32 addr, T value) -> void
{
    if constexpr(std::is_same<T, u8>())
    {
        const u32 end_region = 0x50003FF;

        // if we are in this region, then we do a 16bit write
        // where the 8bit value is written as the upper / lower half
        if (addr <= end_region)
        {
            // align to 16bits
            addr &= ~0x1;
            const u16 new_value = (value << 8) | value;

            const auto dirty_index = (addr & PRAM_MASK) / Gba::dirty_pram_shift;
            gba.dirty_pram[dirty_index] |= new_value != read_array<u16>(MEM.pram, PRAM_MASK, addr);
            gba.dirty_pram_any |= gba.dirty_pram[dirty_index];

            write_array<u16>(MEM.pram, PRAM_MASK, addr, new_value);
        }
    }
    else
    {
        const auto dirty_index = (addr & PRAM_MASK) / Gba::dirty_pram_shift;
        gba.dirty_pram[dirty_index] |= value != read_array<T>(MEM.pram, PRAM_MASK, addr);
        gba.dirty_pram_any |= gba.dirty_pram[dirty_index];

        write_array<T>(MEM.pram, PRAM_MASK, addr, value);
    }
}

template<typename T> [[nodiscard]]
static constexpr auto read_eeprom_region(Gba& gba, u32 addr) -> T
{
    if (gba.backup.type == backup::Type::EEPROM)
    {
        // todo: check rom size for region access
        if constexpr(std::is_same<T, u8>())
        {
            return gba.backup.eeprom.read(gba, addr);
        }
        else if constexpr(std::is_same<T, u16>())
        {
            return gba.backup.eeprom.read(gba, addr);
        }
        else if constexpr(std::is_same<T, u32>())
        {
            assert(!"32bit read from eeprom");
            T value{};
            value |= gba.backup.eeprom.read(gba, addr+0) << 0;
            value |= gba.backup.eeprom.read(gba, addr+1) << 16;
            return value;
        }
    }
    else
    {
        return read_array<T>(gba.rom, ROM_MASK, addr);
    }
}

template<typename T>
static constexpr auto write_eeprom_region(Gba& gba, u32 addr, T value) -> void
{
    if (gba.backup.type == backup::Type::EEPROM)
    {
        // todo: check rom size for region access
        if constexpr(std::is_same<T, u8>())
        {
            gba.backup.eeprom.write(gba, addr, value);
        }
        if constexpr(std::is_same<T, u16>())
        {
            gba.backup.eeprom.write(gba, addr, value);
        }
        if constexpr(std::is_same<T, u32>())
        {
            assert(!"32bit write to eeprom");
            std::printf("32bit write to eeprom\n");
            gba.backup.eeprom.write(gba, addr+0, value>>0);
            gba.backup.eeprom.write(gba, addr+1, value>>16);
        }
    }
}

template<typename T> [[nodiscard]]
static constexpr auto read_sram_region(Gba& gba, u32 addr) -> T
{
    // https://github.com/jsmolka/gba-tests/blob/a6447c5404c8fc2898ddc51f438271f832083b7e/save/none.asm#L21
    T value{0xFF};
    const auto type = gba.backup.type;
    using enum backup::Type;

    if (type == SRAM)
    {
        value = gba.backup.sram.read(gba, addr);
    }
    else if (type == FLASH || type == FLASH1M || type == FLASH512)
    {
        value = gba.backup.flash.read(gba, addr);
    }

    // 16/32bit reads from sram area mirror the byte
    if constexpr(std::is_same<T, u16>())
    {
        value *= 0x0101;
    }
    else if constexpr(std::is_same<T, u32>())
    {
        value *= 0x01010101;
    }

    return value;
}

template<typename T>
static constexpr auto write_sram_region(Gba& gba, u32 addr, T value) -> void
{
    // only byte store/loads are supported
    // if not byte transfer, only a single byte is written
    if constexpr(std::is_same<T, u16>())
    {
        value >>= (addr & 1) * 8;
    }
    if constexpr(std::is_same<T, u32>())
    {
        value >>= (addr & 3) * 8;
    }

    const auto type = gba.backup.type;
    using enum backup::Type;

    if (type == SRAM)
    {
        gba.backup.sram.write(gba, addr, value);
    }
    else if (type == FLASH || type == FLASH1M || type == FLASH512)
    {
        gba.backup.flash.write(gba, addr, value);
    }
}

template<typename T>
using ReadFunction = T(*)(Gba& gba, u32 addr);
template<typename T>
using WriteFunction = void(*)(Gba& gba, u32 addr, T value);

template<typename T>
static constexpr ReadFunction<T> READ_FUNCTION[0x10] =
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
    /*[0xF] =*/ read_sram_region<T>,
};

template<typename T>
static constexpr WriteFunction<T> WRITE_FUNCTION[0x10] =
{
    /*[0x0] =*/ empty_write,
    /*[0x1] =*/ empty_write,
    /*[0x2] =*/ write_ewram_region<T>,
    /*[0x3] =*/ write_iwram_region<T>,
    /*[0x4] =*/ write_io_region<T>,
    /*[0x5] =*/ write_pram_region<T>,
    /*[0x6] =*/ write_vram_region<T>,
    /*[0x7] =*/ write_oam_region<T>,
    /*[0x8] =*/ empty_write,
    /*[0x9] =*/ empty_write,
    /*[0xA] =*/ empty_write,
    /*[0xB] =*/ empty_write,
    /*[0xC] =*/ empty_write,
    /*[0xD] =*/ write_eeprom_region<T>,
    /*[0xE] =*/ write_sram_region<T>,
    /*[0xF] =*/ write_sram_region<T>,
};

// all these functions are inlined
auto read8(Gba& gba, u32 addr) -> u8
{
    gba.scheduler.tick(get_memory_timing(0, addr));

    addr = mirror_address(addr);
    const auto& entry = MEM.rmap_8[addr >> 24];

    if (!entry.array.empty()) [[likely]]
    {
        return read_array<u8>(entry.array, entry.mask, addr);
    }
    else
    {
        return READ_FUNCTION<u8>[addr >> 24](gba, addr);
    }
}

auto read16(Gba& gba, u32 addr) -> u16
{
    gba.scheduler.tick(get_memory_timing(1, addr));

    addr = mirror_address(addr);
    const auto& entry = MEM.rmap_16[addr >> 24];

    if (!entry.array.empty()) [[likely]]
    {
        return read_array<u16>(entry.array, entry.mask, addr);
    }
    else
    {
        return READ_FUNCTION<u16>[addr >> 24](gba, addr);
    }
}

auto read32(Gba& gba, u32 addr) -> u32
{
    gba.scheduler.tick(get_memory_timing(2, addr));

    addr = mirror_address(addr);
    const auto& entry = MEM.rmap_32[addr >> 24];

    if (!entry.array.empty()) [[likely]]
    {
        return read_array<u32>(entry.array, entry.mask, addr);
    }
    else
    {
        return READ_FUNCTION<u32>[addr >> 24](gba, addr);
    }
}

auto write8(Gba& gba, u32 addr, u8 value) -> void
{
    gba.scheduler.tick(get_memory_timing(0, addr));

    addr = mirror_address(addr);
    const auto& entry = MEM.wmap_8[addr >> 24];

    if (!entry.array.empty()) // don't mark likely as vram,pram,io writes are common
    {
        write_array<u8>(entry.array, entry.mask, addr, value);
    }
    else
    {
        WRITE_FUNCTION<u8>[addr >> 24](gba, addr, value);
    }
}

auto write16(Gba& gba, u32 addr, u16 value) -> void
{
    gba.scheduler.tick(get_memory_timing(1, addr));

    addr = mirror_address(addr); // the functions write handlers will manually align.
    const auto& entry = MEM.wmap_16[addr >> 24];

    if (!entry.array.empty()) // don't mark likely as vram,pram,io writes are common
    {
        write_array<u16>(entry.array, entry.mask, addr, value);
    }
    else
    {
        WRITE_FUNCTION<u16>[addr >> 24](gba, addr, value);
    }
}

auto write32(Gba& gba, u32 addr, u32 value) -> void
{
    gba.scheduler.tick(get_memory_timing(2, addr));

    addr = mirror_address(addr);
    const auto& entry = MEM.wmap_32[addr >> 24];

    if (!entry.array.empty()) // don't mark likely as vram,pram,io writes are common
    {
        write_array<u32>(entry.array, entry.mask, addr, value);
    }
    else
    {
        WRITE_FUNCTION<u32>[addr >> 24](gba, addr, value);
    }
}

} // namespace gba::mem
