// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "mem.hpp"
#include "apu/apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "backup/backup.hpp"
#include "bit.hpp"
#include "fat/fat.hpp"
#include "gba.hpp"
#include "key.hpp"
#include "ppu/ppu.hpp"
#include "scheduler.hpp"
#include "sio.hpp"
#include "timer.hpp"
#include "log.hpp"

#include <cassert>
#include <cstring>
#include <type_traits>
#include <bit>
#include <utility>

namespace gba::mem {
namespace {

[[nodiscard]]
constexpr auto mirror_address(const u32 addr) -> u32
{
    return addr & 0x0FFFFFFF;
}

inline auto is_new_region(u8 old_region, u8 new_region) -> bool
{
    switch (new_region)
    {
        case 0x8:
        case 0x9:
            return old_region != 0x8 && old_region != 0x9;

        case 0xA:
        case 0xB:
            return old_region != 0xA && old_region != 0xB;

        case 0xC:
        case 0xD:
            return old_region != 0xC && old_region != 0xD;

        case 0xE:
        case 0xF:
            return old_region != 0xE && old_region != 0xF;

        default:
            return old_region != new_region;
    }
}

template<typename T>
[[nodiscard]] inline auto get_memory_timing(Gba& gba, const u8 region) -> u8
{
    const auto new_region = is_new_region(gba.last_region, region);
    gba.last_region = region;

    if constexpr(std::is_same<T, u8>() || std::is_same<T, u16>())
    {
        return gba.timing_table_16[new_region][region];
    }
    else if constexpr(std::is_same<T, u32>())
    {
        return gba.timing_table_32[new_region][region];
    }
}

// ----- helpers for rw arrays (alignment and endianness are handled) -----
// compare impl: https://godbolt.org/z/57x44EE77
// clang bug: https://godbolt.org/z/1YjfeTYEa
template <typename T> [[nodiscard]]
inline auto read_array(const u8* array, u32 mask, u32 addr) -> T
{
    addr = align<T>(addr) & mask;

    T data;
    std::memcpy(&data, array + addr, sizeof(T));

    if constexpr(std::endian::native == std::endian::big)
    {
        return std::byteswap(data);
    }

    return data;
}

template <typename T>
inline auto write_array(u8* array, u32 mask, u32 addr, T v) -> void
{
    addr = align<T>(addr) & mask;

    if constexpr(std::endian::native == std::endian::big)
    {
        v = std::byteswap(v);
    }

    std::memcpy(array + addr, &v, sizeof(T));
}

template<typename T>
inline auto openbus(Gba& gba, const u32 addr) -> T
{
    log::print_warn(gba, log::Type::MEMORY, "openbus read: 0x%08X pipeline[0]: 0x%08X pipeline[1]: 0x%08X\n", addr, gba.cpu.pipeline[0], gba.cpu.pipeline[1]);

    if (addr <= 0x00003FFF)
    {
        return gba.mem.bios_openbus_value;
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
inline auto empty_write([[maybe_unused]] Gba& gba, [[maybe_unused]] u32 addr, [[maybe_unused]] T value) -> void
{
    log::print_warn(gba, log::Type::MEMORY, "empty write to: 0x%08X value: 0x%08X\n", addr, value);
}

void update_wscnt_table(Gba& gba)
{
    const auto sram = bit::get_range<0, 1>(REG_WSCNT);
    const auto ws0_nseq = bit::get_range<2, 3>(REG_WSCNT);
    const auto ws1_nseq = bit::get_range<5, 6>(REG_WSCNT);
    const auto ws2_nseq = bit::get_range<8, 9>(REG_WSCNT);
    const auto ws0_seq = bit::is_set<4>(REG_WSCNT);
    const auto ws1_seq = bit::is_set<7>(REG_WSCNT);
    const auto ws2_seq = bit::is_set<10>(REG_WSCNT);

    static constexpr u8 WS0_SEQ[2] = { 2+1, 1+1 };
    static constexpr u8 WS1_SEQ[2] = { 4+1, 1+1 };
    static constexpr u8 WS2_SEQ[2] = { 8+1, 1+1 };
    static constexpr u8 WS0_NSEQ[4] = { 4+1,3+1,2+1,8+1 };
    static constexpr u8 WS1_NSEQ[4] = { 4+1,3+1,2+1,8+1 };
    static constexpr u8 WS2_NSEQ[4] = { 4+1,3+1,2+1,8+1 };
    static constexpr u8 SRAM[4] = { 4+1,3+1,2+1,8+1 };

    gba.timing_table_16[SEQ][0x8] = WS0_SEQ[ws0_seq];
    gba.timing_table_16[SEQ][0x9] = WS0_SEQ[ws0_seq];
    gba.timing_table_16[SEQ][0xA] = WS1_SEQ[ws1_seq];
    gba.timing_table_16[SEQ][0xB] = WS1_SEQ[ws1_seq];
    gba.timing_table_16[SEQ][0xC] = WS2_SEQ[ws2_seq];
    gba.timing_table_16[SEQ][0xD] = WS2_SEQ[ws2_seq];

    gba.timing_table_16[NSEQ][0x8] = WS0_NSEQ[ws0_nseq];
    gba.timing_table_16[NSEQ][0x9] = WS0_NSEQ[ws0_nseq];
    gba.timing_table_16[NSEQ][0xA] = WS1_NSEQ[ws1_nseq];
    gba.timing_table_16[NSEQ][0xB] = WS1_NSEQ[ws1_nseq];
    gba.timing_table_16[NSEQ][0xC] = WS2_NSEQ[ws2_nseq];
    gba.timing_table_16[NSEQ][0xD] = WS2_NSEQ[ws2_nseq];

    // 32bit access simply 2 16bit accesses.
    gba.timing_table_32[SEQ][0x8] = WS0_SEQ[ws0_seq] * 2;
    gba.timing_table_32[SEQ][0x9] = WS0_SEQ[ws0_seq] * 2;
    gba.timing_table_32[SEQ][0xA] = WS1_SEQ[ws1_seq] * 2;
    gba.timing_table_32[SEQ][0xB] = WS1_SEQ[ws1_seq] * 2;
    gba.timing_table_32[SEQ][0xC] = WS2_SEQ[ws2_seq] * 2;
    gba.timing_table_32[SEQ][0xD] = WS2_SEQ[ws2_seq] * 2;

    // for nseq access, the second 16bit access is seq
    gba.timing_table_32[NSEQ][0x8] = WS0_NSEQ[ws0_nseq] + WS0_SEQ[ws0_seq];
    gba.timing_table_32[NSEQ][0x9] = WS0_NSEQ[ws0_nseq] + WS0_SEQ[ws0_seq];
    gba.timing_table_32[NSEQ][0xA] = WS1_NSEQ[ws1_nseq] + WS1_SEQ[ws1_seq];
    gba.timing_table_32[NSEQ][0xB] = WS1_NSEQ[ws1_nseq] + WS1_SEQ[ws1_seq];
    gba.timing_table_32[NSEQ][0xC] = WS2_NSEQ[ws2_nseq] + WS2_SEQ[ws2_seq];
    gba.timing_table_32[NSEQ][0xD] = WS2_NSEQ[ws2_nseq] + WS2_SEQ[ws2_seq];

    // timing seems to same regardless of access size
    // i think this is because on 8bit access is valid from
    // this range, so, 16/32 bit access just does a single 8bit access.
    gba.timing_table_16[SEQ][0xE] = SRAM[sram];
    gba.timing_table_32[SEQ][0xF] = SRAM[sram];
    gba.timing_table_16[NSEQ][0xE] = SRAM[sram];
    gba.timing_table_32[NSEQ][0xF] = SRAM[sram];
}

void update_wram_table(Gba& gba)
{
    const auto ewram = bit::get_range<8, 11>(REG_IMC_H);

    // this would be waitstate 0, which is invalid
    // TODO: is this invalid on ds, 3ds, gamecube gba player?
    if (ewram == 15)
    {
        return;
    }

    // NOTE: minimum waitstate for gba micro is 2 (3/3/6)
    static constexpr u8 EWRAM[0x10] = { 15+1, 14+1, 13+1, 12+1, 11+1, 10+1, 9+1, 8+1, 7+1, 6+1, 5+1, 4+1, 3+1, 2+1, 1+1, 0+1 };

    gba.timing_table_16[SEQ][0x2] = EWRAM[ewram];
    gba.timing_table_16[NSEQ][0x2] = EWRAM[ewram];
    gba.timing_table_32[SEQ][0x2] = EWRAM[ewram] * 2;
    gba.timing_table_32[NSEQ][0x2] = EWRAM[ewram] * 2;
}

void setup_timing_table(Gba& gba)
{
    constexpr u8 TIMING_UNMAPPED = 1;
    constexpr u8 TIMING_BIOS = 1;
    constexpr u8 TIMING_IWRAM = 1;
    constexpr u8 TIMING_IO = 1;
    constexpr u8 TIMING_PRAM = 1;
    constexpr u8 TIMING_VRAM = 1;
    constexpr u8 TIMING_OAM = 1;

    auto& seq_16 = gba.timing_table_16[SEQ];
    auto& seq_32 = gba.timing_table_32[SEQ];
    auto& nseq_16 = gba.timing_table_16[NSEQ];
    auto& nseq_32 = gba.timing_table_32[NSEQ];

    // TODO: wram can be unmapped which i imagine effects waitstates
    // also, ewram can be unmapped and have mirror of iwram instead
    // which might use 1/1/1 timing instead.
    seq_16[0x0] = nseq_16[0x0] = TIMING_BIOS;
    seq_16[0x1] = nseq_16[0x1] = TIMING_UNMAPPED;
    seq_16[0x3] = nseq_16[0x3] = TIMING_IWRAM;
    seq_16[0x4] = nseq_16[0x4] = TIMING_IO;
    seq_16[0x5] = nseq_16[0x5] = TIMING_PRAM;
    seq_16[0x6] = nseq_16[0x6] = TIMING_VRAM;
    seq_16[0x7] = nseq_16[0x7] = TIMING_OAM;

    seq_32[0x0] = nseq_32[0x0] = TIMING_BIOS;
    seq_32[0x1] = nseq_32[0x1] = TIMING_UNMAPPED;
    seq_32[0x3] = nseq_32[0x3] = TIMING_IWRAM;
    seq_32[0x4] = nseq_32[0x4] = TIMING_IO;
    seq_32[0x5] = nseq_32[0x5] = TIMING_PRAM * 2;
    seq_32[0x6] = nseq_32[0x6] = TIMING_VRAM * 2;
    seq_32[0x7] = nseq_32[0x7] = TIMING_OAM; // todo: verify it's 1 cycle

    update_wscnt_table(gba);
    update_wram_table(gba);
}

void on_wscnt_write(Gba& gba, u16 value)
{
    const auto old_value = bit::get_range<0, 14>(REG_WSCNT);
    const auto new_value = bit::get_range<0, 14>(value);

    REG_WSCNT = new_value;

    if (old_value != new_value)
    {
        update_wscnt_table(gba);
    }
}

void on_imcl_write(Gba& gba, u16 value)
{
    const auto disable_wram = bit::is_set<0>(value);
    const auto enable_ewram = bit::is_set<5>(value);

    // assert(bit::is_set<5>(REG_IMC_L) && "when bit 5 is unset, locks up gba");
    REG_IMC_L = value;

    if (disable_wram)
    {
        log::print_warn(gba, log::Type::MEMORY, "IMC_H disabled wram, this is not emulated yet!");
        assert(!disable_wram);
    }

    if (!enable_ewram)
    {
        log::print_warn(gba, log::Type::MEMORY, "IMC_H disabled ewram, this is not emulated yet!");
        assert(enable_ewram && "need to mirror iwram over this range, easy to do tbh");
    }
}

void on_imch_write(Gba& gba, u16 value)
{
    const auto old_value = bit::get_range<8, 11>(REG_IMC_H);
    const auto new_value = bit::get_range<8, 11>(value);

    REG_IMC_H = value;

    if (old_value != new_value)
    {
        update_wram_table(gba);
    }
}

void log_write(Gba& gba, char c)
{
    if (gba.log_buffer_index < sizeof(gba.log_buffer) - 1)
    {
        gba.log_buffer[gba.log_buffer_index] = c;
        gba.log_buffer_index++;
    }
}

void log_flag_write(Gba& gba, u16 value)
{
    const auto level = value & 7;
    const auto flush = bit::is_set<8>(value);

    if (flush && gba.log_buffer_index) // only flush if we have anything in buffer
    {
        gba.log_buffer[gba.log_buffer_index - 1] = '\n'; // ensure there's a newline
        gba.log_buffer[gba.log_buffer_index] = '\0'; // null terminate
        log::print(gba, log::Type::GAME, level, gba.log_buffer);
        gba.log_buffer_index = 0;
    }
}

[[nodiscard]]
inline auto read_io16(Gba& gba, const u32 addr) -> u16
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

        case IO_WAVE_RAM0_L:
        case IO_WAVE_RAM0_H:
        case IO_WAVE_RAM1_L:
        case IO_WAVE_RAM1_H:
        case IO_WAVE_RAM2_L:
        case IO_WAVE_RAM2_H:
        case IO_WAVE_RAM3_L:
        case IO_WAVE_RAM3_H:
            return (apu::read_WAVE(gba, addr + 0) << 8) | apu::read_WAVE(gba, addr + 1);

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

        case IO_KEYCNT: {
            constexpr auto mask =
                bit::get_mask<0, 9, u16>() |
                bit::get_mask<14, 15, u16>();
            return REG_KEY & mask;
        }

        case IO_SIOCNT: {
            // todo: mask bits
            return REG_SIOCNT;
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

        case IO_MGBA_CONTROL: // LON (log on)
            return gba.rom_logging ? IO_LOG_ON_RESULT : 0x0000;

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
inline auto read_io8(Gba& gba, const u32 addr) -> u8
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
inline auto read_io32(Gba& gba, const u32 addr) -> u32
{
    assert(!(addr & 0x3) && "unaligned addr in read_io32!");

    // todo: optimise for 32bit regs that games commonly read from
    const u32 lo = read_io16(gba, addr + 0) << 0;
    const u32 hi = read_io16(gba, addr + 2) << 16;
    return hi | lo;
}

inline auto write_io16(Gba& gba, const u32 addr, const u16 value) -> void
{
    assert(!(addr & 0x1) && "unaligned addr in write_io16!");

    switch (addr)
    {
        case IO_TM0D:
            timer::write_timer(gba, value, 0);
            break;

        case IO_TM1D:
            timer::write_timer(gba, value, 1);
            break;

        case IO_TM2D:
            timer::write_timer(gba, value, 2);
            break;

        case IO_TM3D:
            timer::write_timer(gba, value, 3);
            break;

        case IO_IF:
            REG_IF &= ~value;
            break;

        case IO_DISPSTAT:
            REG_DISPSTAT = (REG_DISPSTAT & 0x7) | (value & ~0x7);
            break;

        case IO_WSCNT:
            on_wscnt_write(gba, value);
            break;

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
        case IO_BG3PA:
        case IO_BG3PB:
        case IO_BG3PC:
        case IO_BG3PD:
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
        case IO_SOUNDBIAS:
        case IO_DMA0SAD_LO:
        case IO_DMA1SAD_LO:
        case IO_DMA2SAD_LO:
        case IO_DMA3SAD_LO:
        case IO_DMA0DAD_LO:
        case IO_DMA1DAD_LO:
        case IO_DMA2DAD_LO:
        case IO_DMA3DAD_LO:
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
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            break;

        case IO_KEYCNT:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            key::check_key_interrupt(gba);
            break;

        case IO_RCNT:
            sio::on_rcnt_write(gba, value);
            break;

        case IO_SIOCNT:
            sio::on_siocnt_write(gba, value);
            break;

        case IO_BG2X_LO:
        case IO_BG2X_HI:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            ppu::write_BG2X(gba, addr, value);
            break;

        case IO_BG2Y_LO:
        case IO_BG2Y_HI:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            ppu::write_BG2Y(gba, addr, value);
            break;

        case IO_BG3X_LO:
        case IO_BG3X_HI:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            ppu::write_BG3X(gba, addr, value);
            break;

        case IO_BG3Y_LO:
        case IO_BG3Y_HI:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            ppu::write_BG3Y(gba, addr, value);
            break;

        case IO_SOUND1CNT_L:
            apu::write_NR10(gba, value >> 0);
            break;
        case IO_SOUND1CNT_H:
            apu::write_NR11(gba, value >> 0);
            apu::write_NR12(gba, value >> 8);
            break;
        case IO_SOUND1CNT_X:
            apu::write_NR13(gba, value >> 0);
            apu::write_NR14(gba, value >> 8);
            break;
        case IO_SOUND2CNT_L:
            apu::write_NR21(gba, value >> 0);
            apu::write_NR22(gba, value >> 8);
            break;
        case IO_SOUND2CNT_H:
            apu::write_NR23(gba, value >> 0);
            apu::write_NR24(gba, value >> 8);
            break;
        case IO_SOUND3CNT_L:
            apu::write_NR30(gba, value >> 0);
            break;
        case IO_SOUND3CNT_H:
            apu::write_NR31(gba, value >> 0);
            apu::write_NR32(gba, value >> 8);
            break;
        case IO_SOUND3CNT_X:
            apu::write_NR33(gba, value >> 0);
            apu::write_NR34(gba, value >> 8);
            break;
        case IO_SOUND4CNT_L:
            apu::write_NR41(gba, value >> 0);
            apu::write_NR42(gba, value >> 8);
            break;
        case IO_SOUND4CNT_H:
            apu::write_NR43(gba, value >> 0);
            apu::write_NR44(gba, value >> 8);
            break;
        case IO_SOUNDCNT_L:
            apu::write_NR50(gba, value >> 0);
            apu::write_NR51(gba, value >> 8);
            break;
        case IO_SOUNDCNT_X:
            REG_SOUNDCNT_X = (REG_SOUNDCNT_X & 0xF) | (value & ~0xF); // only 8-bits of CNT_X are used
            apu::write_NR52(gba, value);
            break;

        case IO_WAVE_RAM0_L:
        case IO_WAVE_RAM0_H:
        case IO_WAVE_RAM1_L:
        case IO_WAVE_RAM1_H:
        case IO_WAVE_RAM2_L:
        case IO_WAVE_RAM2_H:
        case IO_WAVE_RAM3_L:
        case IO_WAVE_RAM3_H:
            gba.mem.io[(addr & IO_MASK) >> 1] = value;
            apu::write_WAVE(gba, addr + 0, value >> 0);
            apu::write_WAVE(gba, addr + 1, value >> 8);
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

        case IO_MGBA_CONTROL: // LON (log on)
            if (value == IO_LOG_ON)
            {
                gba.rom_logging = true;
                log::print_info(gba, log::Type::MEMORY, "[LOG] logging enabled\n");
            }
            else if (value == IO_LOG_OFF)
            {
                gba.rom_logging = false;
                log::print_info(gba, log::Type::MEMORY, "[LOG] logging disabled\n");
            }
            else
            {
                log::print_error(gba, log::Type::MEMORY, "[LOG] invalid write to [IO_MGBA_CONTROL]: 0x%04X\n", value);
            }
            break;

        case IO_MGBA_FLAGS:
            if (gba.rom_logging)
            {
                log_flag_write(gba, value);
            }
            break;

        default:
            if (gba.rom_logging && addr >= IO_MGBA_STDOUT && addr <= IO_MGBA_STDOUT + 0x100) [[unlikely]]
            {
                log_write(gba, (value >> 0) & 0xFF);
                log_write(gba, (value >> 8) & 0xFF);
            }
            // the only mirrored reg
            else if ((addr & 0xFFF) == (IO_IMC_L & 0xFFF))
            {
                on_imcl_write(gba, value);
            }
            else if ((addr & 0xFFF) == (IO_IMC_H & 0xFFF))
            {
                on_imch_write(gba, value);
            }
            break;
    }
}

inline auto write_io32(Gba& gba, const u32 addr, const u32 value) -> void
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

inline auto write_io8(Gba& gba, const u32 addr, const u8 value) -> void
{
    switch (addr)
    {
        case IO_SOUND1CNT_L + 0: apu::write_NR10(gba, value); break;
        case IO_SOUND1CNT_H + 0: apu::write_NR11(gba, value); break;
        case IO_SOUND1CNT_H + 1: apu::write_NR12(gba, value); break;
        case IO_SOUND1CNT_X + 0: apu::write_NR13(gba, value); break;
        case IO_SOUND1CNT_X + 1: apu::write_NR14(gba, value); break;
        case IO_SOUND2CNT_L + 0: apu::write_NR21(gba, value); break;
        case IO_SOUND2CNT_L + 1: apu::write_NR22(gba, value); break;
        case IO_SOUND2CNT_H + 0: apu::write_NR23(gba, value); break;
        case IO_SOUND2CNT_H + 1: apu::write_NR24(gba, value); break;
        case IO_SOUND3CNT_L + 0: apu::write_NR30(gba, value); break;
        case IO_SOUND3CNT_H + 0: apu::write_NR31(gba, value); break;
        case IO_SOUND3CNT_H + 1: apu::write_NR32(gba, value); break;
        case IO_SOUND3CNT_X + 0: apu::write_NR33(gba, value); break;
        case IO_SOUND3CNT_X + 1: apu::write_NR34(gba, value); break;
        case IO_SOUND4CNT_L + 0: apu::write_NR41(gba, value); break;
        case IO_SOUND4CNT_L + 1: apu::write_NR42(gba, value); break;
        case IO_SOUND4CNT_H + 0: apu::write_NR43(gba, value); break;
        case IO_SOUND4CNT_H + 1: apu::write_NR44(gba, value); break;
        case IO_SOUNDCNT_L + 0: apu::write_NR50(gba, value); break;
        case IO_SOUNDCNT_L + 1: apu::write_NR51(gba, value); break;

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
            if (addr & 1)
            {
                gba.mem.io[(addr & 0x3FF) >> 1] &= 0x00FF;
                gba.mem.io[(addr & 0x3FF) >> 1] |= value << 8;
            }
            else
            {
                gba.mem.io[(addr & 0x3FF) >> 1] &= 0xFF00;
                gba.mem.io[(addr & 0x3FF) >> 1] |= value;
            }
            apu::write_WAVE(gba, addr, value);
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
            if (gba.rom_logging && addr >= IO_MGBA_STDOUT && addr <= IO_MGBA_STDOUT + 0x100) [[unlikely]]
            {
                log_write(gba, value);
                break;
            }

            u16 actual_value = value;
            const u16 old_value = gba.mem.io[(addr & 0x3FF) >> 1];

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
auto read_io_region(Gba& gba, u32 addr) -> T
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
auto read_bios_region(Gba& gba, const u32 addr) -> T
{
    // this isn't perfect, i don't think the bios should be able
    // to read from itself, though the official bios likely doesn't
    // unofficial bios might do however
    if (arm7tdmi::get_pc(gba) <= BIOS_SIZE) [[likely]]
    {
        gba.mem.bios_openbus_value = read_array<T>(gba.bios, BIOS_MASK, addr);
        return gba.mem.bios_openbus_value;
    }
    else
    {
        return openbus<T>(gba, addr);
    }
}

template<typename T>
auto write_oam_region(Gba& gba, const u32 addr, const T value) -> void
{
    // only non-byte writes are allowed
    if constexpr(!std::is_same<T, u8>())
    {
        write_array<T>(gba.mem.oam, OAM_MASK, addr, value);
    }
}

template<typename T>
auto read_gpio(Gba& gba, const u32 addr) -> T
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
auto write_gpio(Gba& gba, const u32 addr, const T value)
{
    switch (addr)
    {
        case GPIO_DATA: // I/O Port Data (rw or W)
            log::print_info(gba, log::Type::GPIO, "data: 0x%02X\n", value);
            gba.gpio.data = value & gba.gpio.write_mask;
            gba.gpio.rtc.write(gba, addr, value & gba.gpio.write_mask);
            // gba.gpio.data = value & gba.gpio.write_mask;
            break;

        case GPIO_DIRECTION: // I/O Port Direction (rw or W)
            log::print_info(gba, log::Type::GPIO, "direction: 0x%02X\n", value);
            // the direction port acts as a mask for r/w bits
            // bitX = 0, read only (in)
            // bitX = 1, write only (out)
            gba.gpio.read_mask = bit::get_range<0, 4>(~value);
            gba.gpio.write_mask = bit::get_range<0, 4>(value);
            break;

        case GPIO_CONTROL: // I/O Port Control (rw or W)
            gba.gpio.rw = bit::is_set<0>(value);

            log::print_info(gba, log::Type::GPIO, "control: %s\n", gba.gpio.rw ? "rw" : "w only");
            if (gba.gpio.rw)
            {
                // for speed, unmap the rom from [0x8]
                // this will cause the function ptr handler to be called instead
                // which will handle the reads to gpio and rom
                gba.rmap[0x8] = {};
            }
            else
            {
                // gpio is now write only
                // remap rom array for faster reads
                mem::setup_tables(gba);
                // gba.rmap[0x8] = {gba.rom, ROM_MASK, Access_ALL};
            }
            break;
    }
}

// "In bitmap modes reads and writes to 0x06018000 - 0x0601BFFF
// do not work (writes are discarded; reads may always return 0?)."
// SOURCE: https://github.com/nba-emu/hw-test/tree/master/ppu/vram-mirror
inline auto is_vram_access_allowed(Gba& gba, const u32 addr) -> bool
{
    return !ppu::is_bitmap_mode(gba) || (addr & VRAM_MASK) > 0x1BFFF;
}

template<typename T> [[nodiscard]]
auto read_vram_region(Gba& gba, u32 addr) -> T
{
    addr &= VRAM_MASK;

    if (addr > 0x17FFF)
    {
        if (!is_vram_access_allowed(gba, addr)) [[unlikely]]
        {
            return 0x00;
        }

        addr -= 0x8000;
    }

    return read_array<T>(gba.mem.vram, VRAM_MASK, addr);
}

template<typename T>
auto write_vram_region(Gba& gba, u32 addr, const T value) -> void
{
    addr &= VRAM_MASK;

    if (addr > 0x17FFF)
    {
        if (!is_vram_access_allowed(gba, addr)) [[unlikely]]
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
            write_array<u16>(gba.mem.vram, VRAM_MASK, addr, new_value);
        }
    }
    else
    {
        write_array<T>(gba.mem.vram, VRAM_MASK, addr, value);
    }
}

template<typename T>
auto write_pram_region(Gba& gba, u32 addr, const T value) -> void
{
    if constexpr(std::is_same<T, u8>())
    {
        const u16 new_value = (value << 8) | value;
        write_array<u16>(gba.mem.pram, PRAM_MASK, addr, new_value);
    }
}

template<typename T> [[nodiscard]]
auto read_eeprom_region(Gba& gba, const u32 addr) -> T
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
        log::print_warn(gba, log::Type::EEPROM, "32bit read, is this 2 8 bit reads?\n");
        assert(!"32bit read from eeprom");
        T value{};
        value |= gba.backup.eeprom.read(gba, addr+0) << 0;
        value |= gba.backup.eeprom.read(gba, addr+1) << 16;
        return value;
    }
}

template<typename T>
auto write_eeprom_region(Gba& gba, const u32 addr, const T value) -> void
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
        log::print_warn(gba, log::Type::EEPROM, "32bit write, is this 2 8 bit writes?\n");
        gba.backup.eeprom.write(gba, addr+0, value>>0);
        gba.backup.eeprom.write(gba, addr+1, value>>16);
    }
}

template<typename T> [[nodiscard]]
auto read_sram_region(Gba& gba, const u32 addr) -> T
{
    if ((addr & 0xFFFFFF) > 0x00FFFF) [[unlikely]]
    {
        return openbus<T>(gba, addr);
    }

    // https://github.com/jsmolka/gba-tests/blob/a6447c5404c8fc2898ddc51f438271f832083b7e/save/none.asm#L21
    T value{0xFF};

    if (gba.backup.is_sram())
    {
        value = gba.backup.sram.read(gba, addr);
    }
    else if (gba.backup.is_flash())
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
auto write_sram_region(Gba& gba, const u32 addr, T value) -> void
{
    if ((addr & 0xFFFFFF) > 0x00FFFF) [[unlikely]]
    {
        return;
    }

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

    if (gba.backup.is_sram())
    {
        gba.backup.sram.write(gba, addr, value);
    }
    else if (gba.backup.is_flash())
    {
        gba.backup.flash.write(gba, addr, value);
    }
}

template<typename T> [[nodiscard]]
inline auto read_fat_region_helper(Gba& gba, const u32 addr, T result, bool handled) -> T
{
    if (handled)
    {
        return result;
    }

    const auto region = (addr >> 24) & 0xF;

    switch (region)
    {
        case 0x8:
            if (gba.gpio.rw)
            {
                return read_gpio<T>(gba, addr);
            }
            else
            {
                return read_array<T>(gba.rom, ROM_MASK, addr);
            }

        case 0x9:
        case 0xA:
        case 0xB:
        case 0xC:
            return read_array<T>(gba.rom, ROM_MASK, addr);

        case 0xD:
            if (gba.backup.is_eeprom())
            {
                return read_eeprom_region<T>(gba, addr);
            }
            break;

        case 0xE:
        case 0xF:
            return read_sram_region<T>(gba, addr);
    }

    return ~0;
}

template<typename T>
inline auto write_fat_region_helper(Gba& gba, const u32 addr, u32 value, bool handled) -> void
{
    if (handled)
    {
        return;
    }

    const auto region = (addr >> 24) & 0xF;

    switch (region)
    {
        case 0x8:
            write_gpio<T>(gba, addr, value);
            break;

        case 0xD:
            if (gba.backup.is_eeprom())
            {
                write_eeprom_region<T>(gba, addr, value);
            }
            break;

        case 0xE:
        case 0xF:
            if (gba.backup.is_flash() || gba.backup.is_sram())
            {
                write_sram_region<T>(gba, addr, value);
            }
            break;
    }
}

template<typename T> [[nodiscard]]
auto read_fat_mpcf_region(Gba& gba, const u32 addr) -> T
{
    bool handled;
    const auto result = gba.fat_device.mpcf->read(gba, addr, handled);
    return read_fat_region_helper<T>(gba, addr, result, handled);
}

template<typename T> [[nodiscard]]
auto read_fat_m3cf_region(Gba& gba, const u32 addr) -> T
{
    bool handled;
    const auto result = gba.fat_device.m3cf->read(gba, addr, handled);
    return read_fat_region_helper<T>(gba, addr, result, handled);
}

template<typename T> [[nodiscard]]
auto read_fat_sccf_region(Gba& gba, const u32 addr) -> T
{
    bool handled;
    const auto result = gba.fat_device.sccf->read(gba, addr, handled);
    return read_fat_region_helper<T>(gba, addr, result, handled);
}

template<typename T> [[nodiscard]]
auto read_fat_ezflash_region(Gba& gba, const u32 addr) -> T
{
    bool handled;
    const auto result = gba.fat_device.ezflash->read<T>(gba, addr, handled);
    return read_fat_region_helper<T>(gba, addr, result, handled);
}

template<typename T>
auto write_fat_mpcf_region(Gba& gba, const u32 addr, T value) -> void
{
    bool handled;
    gba.fat_device.mpcf->write(gba, addr, value, handled);
    write_fat_region_helper<T>(gba, addr, value, handled);
}

template<typename T>
auto write_fat_m3cf_region(Gba& gba, const u32 addr, T value) -> void
{
    bool handled;
    gba.fat_device.m3cf->write(gba, addr, value, handled);
    write_fat_region_helper<T>(gba, addr, value, handled);
}

template<typename T>
auto write_fat_sccf_region(Gba& gba, const u32 addr, T value) -> void
{
    bool handled;
    gba.fat_device.sccf->write(gba, addr, value, handled);
    write_fat_region_helper<T>(gba, addr, value, handled);
}

template<typename T>
auto write_fat_ezflash_region(Gba& gba, const u32 addr, T value) -> void
{
    bool handled;
    gba.fat_device.ezflash->write<T>(gba, addr, value, handled);
    write_fat_region_helper<T>(gba, addr, value, handled);
}

template<typename T> [[nodiscard]]
inline auto read_internal(Gba& gba, u32 addr) -> T
{
    addr = mirror_address(addr);
    const auto region = addr >> 24;
    gba.scheduler.tick(get_memory_timing<T>(gba, region));

    const auto& entry = gba.rmap[region];

    if (entry.access & sizeof(T)) [[likely]]
    {
        return read_array<T>(entry.array, entry.mask, addr);
    }
    else
    {
        if constexpr(std::is_same<T, u8>())
        {
            return gba.rfuncmap_8[addr >> 24](gba, addr);
        }
        else if constexpr(std::is_same<T, u16>())
        {
            return gba.rfuncmap_16[addr >> 24](gba, addr);
        }
        else if constexpr(std::is_same<T, u32>())
        {
            return gba.rfuncmap_32[addr >> 24](gba, addr);
        }
    }
}

template<typename T>
inline auto write_internal(Gba& gba, u32 addr, T value)
{
    addr = mirror_address(addr);
    const auto region = addr >> 24;
    gba.scheduler.tick(get_memory_timing<T>(gba, region));

    const auto& entry = gba.wmap[region];

    if (entry.access & sizeof(T)) // don't mark likely as vram,pram,io writes are common
    {
        write_array<T>(entry.array, entry.mask, addr, value);
    }
    else
    {
        if constexpr(std::is_same<T, u8>())
        {
            gba.wfuncmap_8[addr >> 24](gba, addr, value);
        }
        else if constexpr(std::is_same<T, u16>())
        {
            gba.wfuncmap_16[addr >> 24](gba, addr, value);
        }
        else if constexpr(std::is_same<T, u32>())
        {
            gba.wfuncmap_32[addr >> 24](gba, addr, value);
        }
    }
}

void set_read_function(Gba& gba, u8 index, ReadFunction<u8> f8, ReadFunction<u16> f16, ReadFunction<u32> f32)
{
    gba.rfuncmap_8[index] = f8;
    gba.rfuncmap_16[index] = f16;
    gba.rfuncmap_32[index] = f32;
}

void set_write_function(Gba& gba, u8 index, WriteFunction<u8> f8, WriteFunction<u16> f16, WriteFunction<u32> f32)
{
    gba.wfuncmap_8[index] = f8;
    gba.wfuncmap_16[index] = f16;
    gba.wfuncmap_32[index] = f32;
}

} // namespace

auto setup_tables(Gba& gba) -> void
{
    #define SET_READ_FUNCTION(gba, index, func) set_read_function(gba, index, func, func, func)
    #define SET_WRITE_FUNCTION(gba, index, func) set_write_function(gba, index, func, func, func)

    std::memset(gba.rmap, 0, sizeof(gba.rmap));
    std::memset(gba.wmap, 0, sizeof(gba.wmap));
    std::memset(gba.rfuncmap_8, 0, sizeof(gba.rfuncmap_8));
    std::memset(gba.rfuncmap_16, 0, sizeof(gba.rfuncmap_16));
    std::memset(gba.rfuncmap_32, 0, sizeof(gba.rfuncmap_32));
    std::memset(gba.wfuncmap_8, 0, sizeof(gba.wfuncmap_8));
    std::memset(gba.wfuncmap_16, 0, sizeof(gba.wfuncmap_16));
    std::memset(gba.wfuncmap_32, 0, sizeof(gba.wfuncmap_32));

    SET_READ_FUNCTION(gba, 0x0, read_bios_region);
    SET_READ_FUNCTION(gba, 0x1, openbus);
    SET_READ_FUNCTION(gba, 0x2, openbus);
    SET_READ_FUNCTION(gba, 0x3, openbus);
    SET_READ_FUNCTION(gba, 0x4, read_io_region);
    SET_READ_FUNCTION(gba, 0x5, openbus);
    SET_READ_FUNCTION(gba, 0x6, read_vram_region);
    SET_READ_FUNCTION(gba, 0x7, openbus);
    SET_READ_FUNCTION(gba, 0x8, read_gpio);
    SET_READ_FUNCTION(gba, 0x9, openbus);
    SET_READ_FUNCTION(gba, 0xA, openbus);
    SET_READ_FUNCTION(gba, 0xB, openbus);
    SET_READ_FUNCTION(gba, 0xC, openbus);
    SET_READ_FUNCTION(gba, 0xD, openbus);
    SET_READ_FUNCTION(gba, 0xE, read_sram_region);
    SET_READ_FUNCTION(gba, 0xF, read_sram_region);

    SET_WRITE_FUNCTION(gba, 0x0, empty_write);
    SET_WRITE_FUNCTION(gba, 0x1, empty_write);
    SET_WRITE_FUNCTION(gba, 0x2, empty_write);
    SET_WRITE_FUNCTION(gba, 0x3, empty_write);
    SET_WRITE_FUNCTION(gba, 0x4, write_io_region);
    SET_WRITE_FUNCTION(gba, 0x5, write_pram_region);
    SET_WRITE_FUNCTION(gba, 0x6, write_vram_region);
    SET_WRITE_FUNCTION(gba, 0x7, write_oam_region);
    SET_WRITE_FUNCTION(gba, 0x8, write_gpio);
    SET_WRITE_FUNCTION(gba, 0x9, empty_write);
    SET_WRITE_FUNCTION(gba, 0xA, empty_write);
    SET_WRITE_FUNCTION(gba, 0xB, empty_write);
    SET_WRITE_FUNCTION(gba, 0xC, empty_write);
    SET_WRITE_FUNCTION(gba, 0xD, empty_write);
    SET_WRITE_FUNCTION(gba, 0xE, write_sram_region);
    SET_WRITE_FUNCTION(gba, 0xF, write_sram_region);

    // todo: check if its still worth having raw ptr / func tables
    gba.rmap[0x2] = {gba.mem.ewram, EWRAM_MASK, Access_ALL};
    gba.rmap[0x3] = {gba.mem.iwram, IWRAM_MASK, Access_ALL};
    gba.rmap[0x5] = {gba.mem.pram, PRAM_MASK, Access_ALL};
    gba.rmap[0x7] = {gba.mem.oam, OAM_MASK, Access_ALL};
    gba.rmap[0x8] = {gba.rom, ROM_MASK, Access_ALL};
    gba.rmap[0x9] = {gba.rom, ROM_MASK, Access_ALL};
    gba.rmap[0xA] = {gba.rom, ROM_MASK, Access_ALL};
    gba.rmap[0xB] = {gba.rom, ROM_MASK, Access_ALL};
    gba.rmap[0xC] = {gba.rom, ROM_MASK, Access_ALL};
    gba.rmap[0xD] = {gba.rom, ROM_MASK, Access_ALL};

    gba.wmap[0x2] = {gba.mem.ewram, EWRAM_MASK, Access_ALL};
    gba.wmap[0x3] = {gba.mem.iwram, IWRAM_MASK, Access_ALL};
    gba.wmap[0x5] = {gba.mem.pram, PRAM_MASK, Access_16bit | Access_32bit};
    gba.wmap[0x7] = {gba.mem.oam, OAM_MASK, Access_16bit | Access_32bit};

    // unmap rom array from 0x8 and let the func fallback handle it
    if (gba.gpio.rw)
    {
        gba.rmap[0x8] = {};
    }

    // this will be handled by the function handlers
    if (gba.backup.is_eeprom())
    {
        gba.rmap[0xD] = {};
        gba.wmap[0xD] = {};
        SET_READ_FUNCTION(gba, 0xD, read_eeprom_region);
        SET_WRITE_FUNCTION(gba, 0xD, write_eeprom_region);
    }

    if (gba.backup.is_flash() || gba.backup.is_sram())
    {
        SET_READ_FUNCTION(gba, 0xE, read_sram_region);
        SET_READ_FUNCTION(gba, 0xF, read_sram_region);
        SET_WRITE_FUNCTION(gba, 0xE, write_sram_region);
        SET_WRITE_FUNCTION(gba, 0xF, write_sram_region);
    }

    switch (gba.fat_device.type)
    {
        case fat::Type::NONE:
            break;

        case fat::Type::MPCF:
            gba.rmap[0x9] = {};
            gba.wmap[0x9] = {};
            SET_READ_FUNCTION(gba, 0x9, read_fat_mpcf_region);
            SET_WRITE_FUNCTION(gba, 0x9, write_fat_mpcf_region);
            break;

        case fat::Type::M3CF:
            gba.rmap[0x8] = {};
            gba.rmap[0x9] = {};
            gba.wmap[0x8] = {};
            gba.wmap[0x9] = {};
            SET_READ_FUNCTION(gba, 0x8, read_fat_m3cf_region);
            SET_READ_FUNCTION(gba, 0x9, read_fat_m3cf_region);
            SET_WRITE_FUNCTION(gba, 0x8, write_fat_m3cf_region);
            SET_WRITE_FUNCTION(gba, 0x9, write_fat_m3cf_region);
            break;

        case fat::Type::SCCF:
            gba.rmap[0x9] = {};
            gba.wmap[0x9] = {};
            SET_READ_FUNCTION(gba, 0x9, read_fat_sccf_region);
            SET_WRITE_FUNCTION(gba, 0x9, write_fat_sccf_region);
            break;

        case fat::Type::EZFLASH:
        case fat::Type::EZFLASH_DE:
            for (u8 i = 0x8; i < 0x10; i++)
            {
                gba.rmap[i] = {};
                gba.wmap[i] = {};
                SET_READ_FUNCTION(gba, i, read_fat_ezflash_region);
                SET_WRITE_FUNCTION(gba, i, write_fat_ezflash_region);
            }
            break;
    }

    #undef SET_READ_FUNCTION
    #undef SET_WRITE_FUNCTION

    setup_timing_table(gba);
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
    return read_internal<u8>(gba, addr);
}

auto read16(Gba& gba, u32 addr) -> u16
{
    return read_internal<u16>(gba, addr);
}

auto read32(Gba& gba, u32 addr) -> u32
{
    return read_internal<u32>(gba, addr);
}

auto write8(Gba& gba, u32 addr, u8 value) -> void
{
    write_internal<u8>(gba, addr, value);
}

auto write16(Gba& gba, u32 addr, u16 value) -> void
{
    write_internal<u16>(gba, addr, value);
}

auto write32(Gba& gba, u32 addr, u32 value) -> void
{
    write_internal<u32>(gba, addr, value);
}

} // namespace gba::mem
