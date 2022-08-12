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

#define MEM gba.mem

namespace gba::mem {
namespace {

[[nodiscard]]
constexpr auto mirror_address(const u32 addr) -> u32
{
    return addr & 0x0FFFFFFF;
}

[[nodiscard]]
inline auto get_memory_timing(const u8 index, const u32 addr) -> u8
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

// todo: make this part of struct when ready to break savestates
u32 bios_openbus_value = 0x0;

template<typename T>
constexpr auto openbus(Gba& gba, const u32 addr) -> T
{
    // std::printf("openbus read: 0x%08X v1: 0x%08X v2: 0x%08X\n", addr, gba.cpu.pipeline[0], gba.cpu.pipeline[1]);

    if (addr <= 0x00003FFF)
    {
        return bios_openbus_value;
    }

    // the below isn't actually how you do open bus, but it'll do for now
    switch (arm7tdmi::get_state(gba))
    {
        case arm7tdmi::State::ARM:
            return gba.cpu.pipeline[1];

        case arm7tdmi::State::THUMB:
            if (addr & 1)
            {
                return (gba.cpu.pipeline[1] << 16) | gba.cpu.pipeline[0];
            }
            else
            {
                return (gba.cpu.pipeline[0] << 16) | gba.cpu.pipeline[1];
            }
    }

    std::unreachable();
}

template<typename T>
constexpr auto empty_write([[maybe_unused]] Gba& gba, [[maybe_unused]] u32 addr, [[maybe_unused]] T value) -> void
{
}

[[nodiscard]]
constexpr auto read_io16(Gba& gba, const u32 addr) -> u16
{
    assert(!(addr & 0x1) && "unaligned addr in read_io16!");

    switch (addr)
    {
        case IO_DISPCNT:
        case IO_DISPSTAT:
        case IO_VCOUNT:
        case IO_BG2CNT:
        case IO_BG3CNT:
        case IO_SOUNDBIAS:
        case IO_WAVE_RAM0_L:
        case IO_WAVE_RAM0_H:
        case IO_WAVE_RAM1_L:
        case IO_WAVE_RAM1_H:
        case IO_WAVE_RAM2_L:
        case IO_WAVE_RAM2_H:
        case IO_WAVE_RAM3_L:
        case IO_WAVE_RAM3_H:
        case IO_TM0CNT:
        case IO_TM1CNT:
        case IO_TM2CNT:
        case IO_TM3CNT:
        case IO_RCNT:
        case IO_IE:
        case IO_IF:
        case IO_WSCNT:
        case IO_IME:
        case IO_HALTCNT_L:
        case IO_HALTCNT_H:
            return gba.mem.io[(addr & IO_MASK) >> 1];

        case IO_TM0D:
            return timer::read_timer(gba, 0);

        case IO_TM1D:
            return timer::read_timer(gba, 1);

        case IO_TM2D:
            return timer::read_timer(gba, 2);

        case IO_TM3D:
            return timer::read_timer(gba, 3);

        case IO_SOUND1CNT_L: {
            constexpr auto mask = bit::get_mask<0, 6, u16>();
            return REG_SOUND1CNT_L & mask;
        }

        case IO_SOUND1CNT_H: {
            constexpr auto mask = bit::get_mask<6, 15, u16>();
            return REG_SOUND1CNT_H & mask;
        }

        case IO_SOUND1CNT_X: {
            constexpr auto mask = bit::get_mask<14, 14, u16>();
            return REG_SOUND1CNT_X & mask;
        }

        case IO_SOUND2CNT_L: {
            constexpr auto mask = bit::get_mask<6, 15, u16>();
            return REG_SOUND2CNT_L & mask;
        }

        case IO_SOUND2CNT_H: {
            constexpr auto mask = bit::get_mask<14, 14, u16>();
            return REG_SOUND2CNT_H & mask;
        }

        case IO_SOUND3CNT_L: {
            constexpr auto mask = bit::get_mask<5, 7, u16>();
            return REG_SOUND3CNT_L & mask;
        }

        case IO_SOUND3CNT_H: {
            constexpr auto mask = bit::get_mask<13, 15, u16>();
            return REG_SOUND3CNT_H & mask;
        }

        case IO_SOUND3CNT_X: {
            constexpr auto mask = bit::get_mask<14, 14, u16>();
            return REG_SOUND3CNT_X & mask;
        }

        case IO_SOUND4CNT_L: {
            constexpr auto mask = bit::get_mask<8, 15, u16>();
            return REG_SOUND4CNT_L & mask;
        }

        case IO_SOUND4CNT_H: {
            constexpr auto mask =
                bit::get_mask<0, 7, u16>() |
                bit::get_mask<14, 14, u16>();
            return REG_SOUND4CNT_H & mask;
        }

        case IO_SOUNDCNT_L: {
            constexpr auto mask =
                bit::get_mask<0, 2, u16>() |
                bit::get_mask<4, 6, u16>() |
                bit::get_mask<8, 11, u16>() |
                bit::get_mask<12, 15, u16>();
            return REG_SOUNDCNT_L & mask;
        }

        case IO_SOUNDCNT_H: {
            constexpr auto mask =
                bit::get_mask<0, 1, u16>() |
                bit::get_mask<2, 2, u16>() |
                bit::get_mask<3, 3, u16>() |
                bit::get_mask<8, 8, u16>() |
                bit::get_mask<9, 9, u16>() |
                bit::get_mask<10, 10, u16>() |
                bit::get_mask<12, 12, u16>() |
                bit::get_mask<13, 13, u16>() |
                bit::get_mask<14, 14, u16>();
            return REG_SOUNDCNT_H & mask;
        }

        case IO_SOUNDCNT_X: {
            constexpr auto mask =
                bit::get_mask<0, 0, u16>() |
                bit::get_mask<1, 1, u16>() |
                bit::get_mask<2, 2, u16>() |
                bit::get_mask<3, 3, u16>() |
                bit::get_mask<7, 7, u16>();
            return REG_SOUNDCNT_X & mask;
        }

        case IO_DMA0CNT_H: {
            constexpr auto mask =
                bit::get_mask<5, 10, u16>() |
                bit::get_mask<12, 15, u16>();
            return REG_DMA0CNT_H & mask;
        }

        case IO_DMA1CNT_H: {
            constexpr auto mask =
                bit::get_mask<5, 10, u16>() |
                bit::get_mask<12, 15, u16>();
            return REG_DMA1CNT_H & mask;
        }

        case IO_DMA2CNT_H: {
            constexpr auto mask =
                bit::get_mask<5, 10, u16>() |
                bit::get_mask<12, 15, u16>();
            return REG_DMA2CNT_H & mask;
        }

        case IO_DMA3CNT_H: {
            constexpr auto mask = bit::get_mask<5, 15, u16>();
            return REG_DMA3CNT_H & mask;
        }

        case IO_BLDMOD: {
            constexpr auto mask = bit::get_mask<0, 13, u16>();
            return REG_BLDMOD & mask;
        }

        case IO_COLEV: {
            constexpr auto mask =
                bit::get_mask<0, 4, u16>() |
                bit::get_mask<8, 12, u16>();
            return REG_COLEV & mask;
        }

        case IO_WININ: {
            constexpr auto mask =
                bit::get_mask<0, 5, u16>() |
                bit::get_mask<8, 13, u16>();
            return REG_WININ & mask;
        }

        case IO_WINOUT: {
            constexpr auto mask =
                bit::get_mask<0, 5, u16>() |
                bit::get_mask<8, 13, u16>();
            return REG_WINOUT & mask;
        }

        case IO_BG0CNT: {
            constexpr auto mask =
                bit::get_mask<0, 12, u16>() |
                bit::get_mask<14, 15, u16>();
            return REG_BG0CNT & mask;
        }

        case IO_BG1CNT: {
            constexpr auto mask =
                bit::get_mask<0, 12, u16>() |
                bit::get_mask<14, 15, u16>();
            return REG_BG1CNT & mask;
        }

        case IO_KEY: {
            constexpr auto mask = bit::get_mask<0, 9, u16>();
            return REG_KEY & mask;
        }

        // these are registers with w only bits
        // these don't return openbus, instead return 0x0000
        case 0x4000066: // REG_SOUND1CNT_X (high 16 bits unreadable)
        case 0x400006A: // REG_SOUND2CNT_L (high 16 bits unreadable)
        case 0x400006E: // REG_SOUND2CNT_H (high 16 bits unreadable)
        case 0x4000076: // REG_SOUND3CNT_X (high 16 bits unreadable)
        case 0x400007A: // REG_SOUND4CNT_L (high 16 bits unreadable)
        case 0x400007E: // REG_SOUND4CNT_L (high 16 bits unreadable)
        case 0x4000086: // REG_SOUNDCNT_X (high 16 bits unreadable)
        case 0x400008A: // REG_SOUNDBIAS (high 16 bits unreadable)
        case IO_DMA0CNT_L:
        case IO_DMA1CNT_L:
        case IO_DMA2CNT_L:
        case IO_DMA3CNT_L:
        case 0x4000136: // REG_IR (high 16 bits unreadable)
        case 0x4000142: // ???
        case 0x400015A: // REG_JOYSTAT_H (high 16 bits unreadable)
        case 0x4000206: // REG_WSCNT (high 16 bits unreadable)
        case 0x400020A: // REG_IME (high 16 bits unreadable)
        case 0x4000302: // REG_PAUSE (high 16 bits unreadable)
            return 0x0000;

        default:
            // the only mirrored reg
            if ((addr & 0xFFF) == (IO_IMC_L & 0xFFF))
            {
                return REG_IMC_L;
            }
            else if ((addr & 0xFFF) == (IO_IMC_H & 0xFFF))
            {
                return REG_IMC_H;
            }

            // oob access, invalid regs and write-only regs return openbus
            return openbus<u16>(gba, addr);
    }
}

[[nodiscard]]
constexpr auto read_io8(Gba& gba, const u32 addr) -> u8
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
constexpr auto read_io32(Gba& gba, const u32 addr) -> u32
{
    assert(!(addr & 0x3) && "unaligned addr in read_io32!");

    // todo: optimise for 32bit regs that games commonly read from
    const u32 lo = read_io16(gba, addr + 0) << 0;
    const u32 hi = read_io16(gba, addr + 2) << 16;
    return hi | lo;
}

constexpr auto write_io16(Gba& gba, const u32 addr, const u16 value) -> void
{
    assert(!(addr & 0x1) && "unaligned addr in write_io16!");

    switch (addr)
    {
        case IO_TM0D:
            gba.timer[0].reload = value;
            break;

        case IO_TM1D:
            gba.timer[1].reload = value;
            break;

        case IO_TM2D:
            gba.timer[2].reload = value;
            break;

        case IO_TM3D:
            gba.timer[3].reload = value;
            break;

        case IO_IF:
            REG_IF &= ~value;
            break;

        case IO_DISPSTAT:
            REG_DISPSTAT = (REG_DISPSTAT & 0x7) | (value & ~0x7);
            break;

        case IO_SOUNDCNT_X:
            REG_SOUNDCNT_X = (REG_SOUNDCNT_X & 0xF) | (value & ~0xF);
            apu::write_legacy8(gba, addr, value); // only 8-bits of CNT_X are used
            break;

        case IO_WSCNT: {
            const auto old_value = REG_WSCNT;
            const auto new_value = value;

            // std::printf("[IO_WSCNT] 0x%04X\n", value);
            REG_WSCNT = value;

            if (old_value != new_value)
            {
                // assert(!"IO_WSCNT waitstate updated!");
            }
        } break;

        case IO_DISPCNT:
        case IO_BG0CNT:
        case IO_BG1CNT:
        case IO_BG2CNT:
        case IO_BG3CNT:
        case IO_BG0HOFS:
        case IO_BG0VOFS:
        case IO_BG1HOFS:
        case IO_BG1VOFS:
        case IO_BG2HOFS:
        case IO_BG2VOFS:
        case IO_BG3HOFS:
        case IO_BG3VOFS:
        case IO_BG2PA:
        case IO_BG2PB:
        case IO_BG2PC:
        case IO_BG2PD:
        case IO_BG2X:
        case IO_BG2Y:
        case IO_BG3PA:
        case IO_BG3PB:
        case IO_BG3PC:
        case IO_BG3PD:
        case IO_BG3X:
        case IO_BG3Y:
        case IO_BG2X_HI:
        case IO_BG2Y_HI:
        case IO_BG3X_HI:
        case IO_BG3Y_HI:
        case IO_WIN0H:
        case IO_WIN1H:
        case IO_WIN0V:
        case IO_WIN1V:
        case IO_WININ:
        case IO_WINOUT:
        case IO_MOSAIC:
        case IO_BLDMOD:
        case IO_COLEV:
        case IO_COLEY:
        case IO_SOUNDCNT_L:
        case IO_SOUNDBIAS:
        case IO_DMA0SAD:
        case IO_DMA1SAD:
        case IO_DMA2SAD:
        case IO_DMA3SAD:
        case IO_DMA0DAD:
        case IO_DMA1DAD:
        case IO_DMA2DAD:
        case IO_DMA3DAD:
        case IO_DMA0SAD_HI:
        case IO_DMA1SAD_HI:
        case IO_DMA2SAD_HI:
        case IO_DMA3SAD_HI:
        case IO_DMA0DAD_HI:
        case IO_DMA1DAD_HI:
        case IO_DMA2DAD_HI:
        case IO_DMA3DAD_HI:
        case IO_DMA0CNT_L:
        case IO_DMA1CNT_L:
        case IO_DMA2CNT_L:
        case IO_DMA3CNT_L:
        case IO_RCNT:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            break;

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
        // case IO_SOUNDCNT_X:
        case IO_WAVE_RAM0_L:
        case IO_WAVE_RAM0_H:
        case IO_WAVE_RAM1_L:
        case IO_WAVE_RAM1_H:
        case IO_WAVE_RAM2_L:
        case IO_WAVE_RAM2_H:
        case IO_WAVE_RAM3_L:
        case IO_WAVE_RAM3_H:
            if (apu::is_apu_enabled(gba))
            {
                gba.mem.io[(addr & IO_MASK) >> 1] = value;
                apu::write_legacy(gba, addr, value);
            }
            break;

        case IO_TM0CNT:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            timer::on_cnt_write(gba, 0);
            break;

        case IO_TM1CNT:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            timer::on_cnt_write(gba, 1);
            break;

        case IO_TM2CNT:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            timer::on_cnt_write(gba, 2);
            break;

        case IO_TM3CNT:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            timer::on_cnt_write(gba, 3);
            break;

        case IO_DMA0CNT_H:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            dma::on_cnt_write(gba, 0);
            break;

        case IO_DMA1CNT_H:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            dma::on_cnt_write(gba, 1);
            break;

        case IO_DMA2CNT_H:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            dma::on_cnt_write(gba, 2);
            break;

        case IO_DMA3CNT_H:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            dma::on_cnt_write(gba, 3);
            break;

        case IO_IME:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            arm7tdmi::schedule_interrupt(gba);
            break;

        case IO_HALTCNT_L:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            arm7tdmi::on_halt_trigger(gba, arm7tdmi::HaltType::write);
            break;

        case IO_IE:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
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
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            apu::on_soundcnt_write(gba);
            break;

        default:
            // the only mirrored reg
            if ((addr & 0xFFF) == (IO_IMC_L & 0xFFF))
            {
                assert(bit::is_set<5>(REG_IMC_L) && "when bit 5 is unset, locks up gba");
                REG_IMC_L = value;
            }
            else if ((addr & 0xFFF) == (IO_IMC_H & 0xFFF))
            {
                const auto old_value = bit::get_range<0x8, 0xB>(REG_IMC_H);
                const auto new_value = bit::get_range<0x8, 0xB>(value);
                assert((new_value == 0b1101 || new_value == 0b1110) && "invalid wram waitstate");

                REG_IMC_H = value;

                if (old_value != new_value)
                {
                    // todo: update waitstate!
                    assert(!"wram waitstate updated!");
                }
            }
            break;
    }
}

constexpr auto write_io32(Gba& gba, const u32 addr, const u32 value) -> void
{
    assert(!(addr & 0x3) && "unaligned addr in write_io32!");

    switch (addr)
    {
        case IO_FIFO_A_L:
        case IO_FIFO_A_H:
            apu::on_fifo_write32(gba, value, 0);
            return;

        case IO_FIFO_B_L:
        case IO_FIFO_B_H:
            apu::on_fifo_write32(gba, value, 1);
            return;
    }

    write_io16(gba, addr + 0, value >> 0x00);
    write_io16(gba, addr + 2, value >> 0x10);
}

constexpr auto write_io8(Gba& gba, const u32 addr, const u8 value) -> void
{
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
            if (apu::is_apu_enabled(gba))
            {
                if (addr & 1)
                {
                    gba.mem.io[(addr & IO_MASK) >> 1] &= 0x00FF;
                    gba.mem.io[(addr & IO_MASK) >> 1] |= value << 8;
                }
                else
                {
                    gba.mem.io[(addr & IO_MASK) >> 1] &= 0xFF00;
                    gba.mem.io[(addr & IO_MASK) >> 1] |= value;
                }
                apu::write_legacy8(gba, addr, value);
            }
            break;

        case IO_IF + 0:
            REG_IF &= ~value;
            break;

        case IO_IF + 1:
            REG_IF &= ~(value << 8);
            break;

        case IO_FIFO_A_L:
        case IO_FIFO_A_L+1:
        case IO_FIFO_A_H:
        case IO_FIFO_A_H+1:
            apu::on_fifo_write8(gba, value, 0);
            break;

        case IO_FIFO_B_L:
        case IO_FIFO_B_L+1:
        case IO_FIFO_B_H:
        case IO_FIFO_B_H+1:
            apu::on_fifo_write8(gba, value, 1);
            break;

        case IO_IME:
            REG_IME = value;
            arm7tdmi::schedule_interrupt(gba);
            break;

        case IO_HALTCNT_L:
            gba.mem.io[(addr & IO_MASK) >> 1] &= 0xFF00;
            gba.mem.io[(addr & IO_MASK) >> 1] |= value;
            break;

        case IO_HALTCNT_H:
            arm7tdmi::on_halt_trigger(gba, arm7tdmi::HaltType::write);
            break;

        default: {
            u16 actual_value = value;
            const u16 old_value = MEM.io[(addr & 0x3FF) >> 1];

            if (addr & 1)
            {
                actual_value <<= 8;
                actual_value |= old_value & 0x00FF;
            }
            else
            {
                actual_value |= old_value & 0xFF00;
            }

            write_io16(gba, addr & ~0x1, actual_value);
        } break;
    }
}

template<typename T> [[nodiscard]]
constexpr auto read_io_region(Gba& gba, u32 addr) -> T
{
    addr = align<T>(addr);

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
constexpr void write_io_region(Gba& gba, u32 addr, const T value)
{
    addr = align<T>(addr);

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

template<typename T> [[nodiscard]]
constexpr auto read_bios_region(Gba& gba, const u32 addr) -> T
{
    // this isn't perfect, i don't think the bios should be able
    // to read from itself, though the official bios likely doesn't
    // unofficial bios might do however
    if (arm7tdmi::get_pc(gba) <= BIOS_SIZE) [[likely]]
    {
        bios_openbus_value = read_array<T>(gba.bios, BIOS_MASK, addr);
        return bios_openbus_value;
    }
    else
    {
        return openbus<T>(gba, addr);
    }
}

// unused, handled in array writes
template<typename T>
constexpr auto write_ewram_region(Gba& gba, const u32 addr, const T value) -> void
{
    write_array<T>(MEM.ewram, EWRAM_MASK, addr, value);
}

// unused, handled in array writes
template<typename T>
constexpr auto write_iwram_region(Gba& gba, const u32 addr, const T value) -> void
{
    write_array<T>(MEM.iwram, IWRAM_MASK, addr, value);
}

// if accessing vram/pram/oam at the same time as the ppu
// then an 1 cycle penalty occurs.
inline auto extra_cycle_if_ppu_access_mem(Gba& gba) -> void
{
    if (ppu::is_screen_visible(gba)) [[unlikely]]
    {
        // std::printf("[MEM] 1 cycle penalty for ppu access\n");
        gba.scheduler.tick(1);
    }
}

template<typename T>
constexpr auto read_oam_region(Gba& gba, const u32 addr) -> T
{
    extra_cycle_if_ppu_access_mem(gba);

    return read_array<T>(MEM.oam, OAM_MASK, addr);
}

template<typename T>
constexpr auto write_oam_region(Gba& gba, const u32 addr, const T value) -> void
{
    extra_cycle_if_ppu_access_mem(gba);

    // only non-byte writes are allowed
    if constexpr(!std::is_same<T, u8>())
    {
        write_array<T>(MEM.oam, OAM_MASK, addr, value);
    }
}

template<typename T>
constexpr auto read_gpio(Gba& gba, const u32 addr) -> T
{
    assert(gba.gpio.rw && "this handler should only be called when gpio is rw");

    switch (addr)
    {
        case GPIO_DATA: // I/O Port Data (rw or W)
            return gba.gpio.data & gba.gpio.read_mask;

        case GPIO_DIRECTION: // I/O Port Direction (rw or W)
            return gba.gpio.write_mask; // remember we modify the rmask

        case GPIO_CONTROL: // I/O Port Control (rw or W)
            return gba.gpio.rw;

        default:
            return read_array<T>(gba.rom, ROM_MASK, addr);
    }
}

template<typename T>
constexpr auto write_gpio(Gba& gba, const u32 addr, const T value)
{
    switch (addr)
    {
        case GPIO_DATA: // I/O Port Data (rw or W)
            // std::printf("[GPIO] data: 0x%02X\n", value);
            gba.gpio.data = value & gba.gpio.write_mask;
            gba.gpio.rtc.write(gba, addr, value & gba.gpio.write_mask);
            // gba.gpio.data = value & gba.gpio.write_mask;
            break;

        case GPIO_DIRECTION: // I/O Port Direction (rw or W)
            // std::printf("[GPIO] direction: 0x%02X\n", value);
            // the direction port acts as a mask for r/w bits
            // bitX = 0, read only (in)
            // bitX = 1, write only (out)
            gba.gpio.read_mask = bit::get_range<0, 4>(~value);
            gba.gpio.write_mask = bit::get_range<0, 4>(value);
            break;

        case GPIO_CONTROL: // I/O Port Control (rw or W)
            gba.gpio.rw = bit::is_set<0>(value);

            std::printf("[GPIO] control: %s\n", gba.gpio.rw ? "rw" : "w only");
            if (gba.gpio.rw)
            {
                // for speed, unmap the rom from [0x8]
                // this will cause the function ptr handler to be called instead
                // which will handle the reads to gpio and rom
                MEM.rmap_32[0x8] = MEM.rmap_16[0x8] = MEM.rmap_8[0x8] = {};
            }
            else
            {
                // gpio is now write only
                // remap rom array for faster reads
                std::printf("unammped rom handler\n");
                MEM.rmap_32[0x8] = MEM.rmap_16[0x8] = MEM.rmap_8[0x8] = {gba.rom, ROM_MASK};
            }
            break;
    }
}

template<typename T> [[nodiscard]]
constexpr auto read_vram_region(Gba& gba, u32 addr) -> T
{
    extra_cycle_if_ppu_access_mem(gba);

    addr &= VRAM_MASK;

    if (addr > 0x17FFF)
    {
        if (ppu::is_bitmap_mode(gba) && addr <= 0x1BFFF) [[unlikely]]
        {
            return openbus<T>(gba, addr);
        }

        addr -= 0x8000;
    }

    return read_array<T>(MEM.vram, VRAM_MASK, addr);
}

template<typename T>
constexpr auto write_vram_region(Gba& gba, u32 addr, const T value) -> void
{
    extra_cycle_if_ppu_access_mem(gba);

    addr &= VRAM_MASK;

    if (addr > 0x17FFF)
    {
        if (ppu::is_bitmap_mode(gba) && addr <= 0x1BFFF) [[unlikely]]
        {
            return;
        }

        addr -= 0x8000;
    }

    if constexpr(std::is_same<T, u8>())
    {
        const bool bitmap = ppu::is_bitmap_mode(gba);
        const u32 end_region = bitmap ? 0x13FFF : 0xFFFF;

        // if we are in this region, then we do a 16bit write
        // where the 8bit value is written as the upper / lower half
        if (addr <= end_region)
        {
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
constexpr auto read_pram_region(Gba& gba, const u32 addr) -> T
{
    extra_cycle_if_ppu_access_mem(gba);

    return read_array<T>(MEM.pram, PRAM_MASK, addr);
}

template<typename T>
constexpr auto write_pram_region(Gba& gba, u32 addr, const T value) -> void
{
    extra_cycle_if_ppu_access_mem(gba);

    if constexpr(std::is_same<T, u8>())
    {
        const u16 new_value = (value << 8) | value;
        write_array<u16>(MEM.pram, PRAM_MASK, addr, new_value);
    }
    else
    {
        write_array<T>(MEM.pram, PRAM_MASK, addr, value);
    }
}

template<typename T> [[nodiscard]]
constexpr auto read_eeprom_region(Gba& gba, const u32 addr) -> T
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
constexpr auto write_eeprom_region(Gba& gba, const u32 addr, const T value) -> void
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
constexpr auto read_sram_region(Gba& gba, const u32 addr) -> T
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
constexpr auto write_sram_region(Gba& gba, const u32 addr, T value) -> void
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
constexpr ReadFunction<T> READ_FUNCTION[0x10] =
{
    /*[0x0] =*/ read_bios_region<T>,
    /*[0x1] =*/ openbus,
    /*[0x2] =*/ openbus, // todo: add this
    /*[0x3] =*/ openbus, // todo: add this
    /*[0x4] =*/ read_io_region<T>,
    /*[0x5] =*/ read_pram_region<T>,
    /*[0x6] =*/ read_vram_region<T>,
    /*[0x7] =*/ read_oam_region<T>,
    /*[0x8] =*/ read_gpio, // called when gpio is rw (w only default)
    /*[0x9] =*/ openbus, // todo: add this
    /*[0xA] =*/ openbus, // todo: add this
    /*[0xB] =*/ openbus, // todo: add this
    /*[0xC] =*/ openbus, // todo: add this
    /*[0xD] =*/ read_eeprom_region<T>,
    /*[0xE] =*/ read_sram_region<T>,
    /*[0xF] =*/ read_sram_region<T>,
};

template<typename T>
constexpr WriteFunction<T> WRITE_FUNCTION[0x10] =
{
    /*[0x0] =*/ empty_write,
    /*[0x1] =*/ empty_write,
    /*[0x2] =*/ write_ewram_region<T>,
    /*[0x3] =*/ write_iwram_region<T>,
    /*[0x4] =*/ write_io_region<T>,
    /*[0x5] =*/ write_pram_region<T>,
    /*[0x6] =*/ write_vram_region<T>,
    /*[0x7] =*/ write_oam_region<T>,
    /*[0x8] =*/ write_gpio,
    /*[0x9] =*/ empty_write,
    /*[0xA] =*/ empty_write,
    /*[0xB] =*/ empty_write,
    /*[0xC] =*/ empty_write,
    /*[0xD] =*/ write_eeprom_region<T>,
    /*[0xE] =*/ write_sram_region<T>,
    /*[0xF] =*/ write_sram_region<T>,
};

} // namespace

auto setup_tables(Gba& gba) -> void
{
    std::ranges::fill(MEM.rmap_8, ReadArray{});
    std::ranges::fill(MEM.rmap_16, ReadArray{});
    std::ranges::fill(MEM.rmap_32, ReadArray{});
    std::ranges::fill(MEM.wmap_8, WriteArray{});
    std::ranges::fill(MEM.wmap_16, WriteArray{});
    std::ranges::fill(MEM.wmap_32, WriteArray{});

    // todo: check if its still worth having raw ptr / func tables
    MEM.rmap_16[0x2] = {gba.mem.ewram, EWRAM_MASK};
    MEM.rmap_16[0x3] = {gba.mem.iwram, IWRAM_MASK};
    MEM.rmap_16[0x8] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0x9] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xA] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xB] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xC] = {gba.rom, ROM_MASK};
    MEM.rmap_16[0xD] = {gba.rom, ROM_MASK};

    MEM.wmap_16[0x2] = {gba.mem.ewram, EWRAM_MASK};
    MEM.wmap_16[0x3] = {gba.mem.iwram, IWRAM_MASK};

    // unmap rom array from 0x8 and let the func fallback handle it
    if (gba.gpio.rw)
    {
        MEM.rmap_16[0x8] = {};
    }

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
}

auto reset(Gba& gba, bool skip_bios) -> void
{
    gba.mem = {};

    REG_KEY = 0xFFFF; // all keys are up
    REG_IMC_L = bit::set<5>(REG_IMC_L); // always set
    REG_IMC_H = 0xD00; // wram 2 waitstates

    if (skip_bios)
    {
        REG_RCNT = 0x8000;
    }

    setup_tables(gba);
}

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
