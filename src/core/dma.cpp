// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backup/backup.hpp"
#include "backup/eeprom.hpp"
#include "gba.hpp"
#include "dma.hpp"
#include "mem.hpp"
#include "bit.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "scheduler.hpp"
#include "log.hpp"
#include "waitloop.hpp"
#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility> // for std::unreachable c++23

// tick scheduler after every dma transfer
// there's a few ways to speed this up but none have been
// expored yet.
#define ACCURATE_BUT_SLOW_DMA_TIMING 1

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#DMA%20Source%20Registers
// https://problemkaputt.de/gbatek.htm#gbadmatransfers
namespace gba::dma {
namespace {

constexpr auto INTERNAL_MEMORY_RANGE = 0x07FFFFFF;
constexpr auto ANY_MEMORY_RANGE = 0x0FFFFFFF;

constexpr log::Type LOG_TYPE[4] =
{
    log::Type::DMA0,
    log::Type::DMA1,
    log::Type::DMA2,
    log::Type::DMA3,
};

constexpr arm7tdmi::Interrupt INTERRUPTS[4] =
{
    arm7tdmi::Interrupt::DMA0,
    arm7tdmi::Interrupt::DMA1,
    arm7tdmi::Interrupt::DMA2,
    arm7tdmi::Interrupt::DMA3,
};

constexpr u32 SRC_MASK[4] =
{
    INTERNAL_MEMORY_RANGE,
    ANY_MEMORY_RANGE,
    ANY_MEMORY_RANGE,
    ANY_MEMORY_RANGE,
};

constexpr u32 DST_MASK[4] =
{
    INTERNAL_MEMORY_RANGE,
    INTERNAL_MEMORY_RANGE,
    INTERNAL_MEMORY_RANGE,
    ANY_MEMORY_RANGE,
};

struct [[nodiscard]] Registers
{
    u32 dmasad;
    u32 dmadad;
    u16 dmacnt_h;
    u16 dmacnt_l;
};

auto get_channel_registers(Gba& gba, const u8 channel_num) -> Registers
{
    switch (channel_num)
    {
        case 0: return { static_cast<u32>((REG_DMA0SAD_HI << 16) | REG_DMA0SAD_LO), static_cast<u32>((REG_DMA0DAD_HI << 16) | REG_DMA0DAD_LO), REG_DMA0CNT_H, REG_DMA0CNT_L };
        case 1: return { static_cast<u32>((REG_DMA1SAD_HI << 16) | REG_DMA1SAD_LO), static_cast<u32>((REG_DMA1DAD_HI << 16) | REG_DMA1DAD_LO), REG_DMA1CNT_H, REG_DMA1CNT_L };
        case 2: return { static_cast<u32>((REG_DMA2SAD_HI << 16) | REG_DMA2SAD_LO), static_cast<u32>((REG_DMA2DAD_HI << 16) | REG_DMA2DAD_LO), REG_DMA2CNT_H, REG_DMA2CNT_L };
        case 3: return { static_cast<u32>((REG_DMA3SAD_HI << 16) | REG_DMA3SAD_LO), static_cast<u32>((REG_DMA3DAD_HI << 16) | REG_DMA3DAD_LO), REG_DMA3CNT_H, REG_DMA3CNT_L };
    }

    std::unreachable();
}

void disable_channel(Gba& gba, u8 channel_num)
{
    switch (channel_num)
    {
        case 0:
            REG_DMA0CNT_H = bit::unset<15>(REG_DMA0CNT_H);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA0CNT_L);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA0CNT_H);
            break;

        case 1:
            REG_DMA1CNT_H = bit::unset<15>(REG_DMA1CNT_H);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA1CNT_L);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA1CNT_H);
            break;

        case 2:
            REG_DMA2CNT_H = bit::unset<15>(REG_DMA2CNT_H);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA2CNT_L);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA2CNT_H);
            break;

        case 3:
            REG_DMA3CNT_H = bit::unset<15>(REG_DMA3CNT_H);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA3CNT_L);
            gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IO, mem::IO_DMA3CNT_H);
            break;
    }

    gba.dma[channel_num].enabled = false;
}

void advance_scheduler([[maybe_unused]] Gba& gba)
{
    #if ACCURATE_BUT_SLOW_DMA_TIMING
    if (gba.scheduler.should_fire())
    {
        gba.scheduler.fire();
    }
    #endif
}

enum DmaType
{
    DMA_TYPE_NORMAL,
    DMA_TYPE_SLOW,
    DMA_TYPE_INVALID,
};

struct RW
{
    u8* ptr;
    u32 size;
    u32 addr; // relative addr
    u8 type;
    u8 cycles16;
    u8 cycles32;

    [[nodiscard]] auto is_oob(s32 inc) const
    {
        return addr + inc > size;
    }

    template<typename T>
    [[nodiscard]] constexpr auto get_cycles() const -> u8
    {
        if constexpr(std::is_same<T, u16>())
        {
            return cycles16;
        }
        return cycles32;
    }
};

auto get_region(u32 addr) -> u8
{
    return (addr >> 24) & 0xF;
}

auto get_read_data(Gba& gba, u32 addr) -> RW
{
    RW src{};
    const auto region = get_region(addr);
    src.cycles16 = mem::get_cycles_for_region_16(gba, region, mem::SEQ);
    src.cycles32 = mem::get_cycles_for_region_32(gba, region, mem::SEQ);

    switch (region)
    {
        case 0x0:
        case 0x1:
            src.size = 0x02000000;
            src.addr = addr & 0x01FFFFFF;
            src.type = DMA_TYPE_INVALID;
            break;

        case 0x2:
            src.ptr = gba.mem.ewram;
            src.size = mem::EWRAM_SIZE;
            src.addr = addr & mem::EWRAM_MASK;
            src.type = DMA_TYPE_NORMAL;
            break;

        case 0x3:
            src.ptr = gba.mem.iwram;
            src.size = mem::IWRAM_SIZE;
            src.addr = addr & mem::IWRAM_MASK;
            src.type = DMA_TYPE_NORMAL;
            break;

        case 0x4:
            src.type = DMA_TYPE_SLOW;
            break;

        case 0x5:
            src.ptr = gba.mem.pram;
            src.size = mem::PRAM_SIZE;
            src.addr = addr & mem::PRAM_MASK;
            src.type = DMA_TYPE_NORMAL;
            break;

        case 0x6:
            // todo: optimise for this!
            if ((addr & mem::VRAM_MASK) > 0x17FFF)
            {
                src.type = DMA_TYPE_SLOW;
            }
            else
            {
                src.ptr = gba.mem.vram;
                src.size = mem::VRAM_SIZE;
                src.addr = addr & mem::VRAM_MASK;
                src.type = DMA_TYPE_NORMAL;
            }
            break;

        case 0x7:
            src.ptr = gba.mem.oam;
            src.size = mem::OAM_SIZE;
            src.addr = addr & mem::OAM_MASK;
            src.type = DMA_TYPE_NORMAL;
            break;

        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
            if (gba.rmap[region].array == nullptr)
            {
                src.type = DMA_TYPE_SLOW;
            }
            else
            {
                src.ptr = gba.rom;
                src.size = mem::ROM_SIZE;
                src.addr = addr & mem::ROM_MASK;
                src.type = DMA_TYPE_NORMAL;
            }
            break;

        case 0xE:
        case 0xF:
            src.type = DMA_TYPE_SLOW;
            break;
    }

    return src;
}

auto get_write_data(Gba& gba, u32 addr) -> RW
{
    RW dst{};
    const auto region = get_region(addr);
    dst.cycles16 = mem::get_cycles_for_region_16(gba, region, mem::SEQ);
    dst.cycles32 = mem::get_cycles_for_region_32(gba, region, mem::SEQ);

    switch (region)
    {
        case 0x0:
        case 0x1:
            dst.size = 0x02000000;
            dst.addr = addr & 0x01FFFFFF;
            dst.type = DMA_TYPE_INVALID;
            break;

        case 0x2:
            dst.ptr = gba.mem.ewram;
            dst.size = mem::EWRAM_SIZE;
            dst.addr = addr & mem::EWRAM_MASK;
            dst.type = DMA_TYPE_NORMAL;
            break;

        case 0x3:
            dst.ptr = gba.mem.iwram;
            dst.size = mem::IWRAM_SIZE;
            dst.addr = addr & mem::IWRAM_MASK;
            dst.type = DMA_TYPE_NORMAL;
            break;

        case 0x4:
            dst.type = DMA_TYPE_SLOW;
            break;

        case 0x5:
            dst.ptr = gba.mem.pram;
            dst.size = mem::PRAM_SIZE;
            dst.addr = addr & mem::PRAM_MASK;
            dst.type = DMA_TYPE_NORMAL;
            break;

        case 0x6:
            // todo: optimise for this!
            if ((addr & mem::VRAM_MASK) > 0x17FFF)
            {
                dst.type = DMA_TYPE_SLOW;
            }
            else
            {
                dst.ptr = gba.mem.vram;
                dst.size = mem::VRAM_SIZE;
                dst.addr = addr & mem::VRAM_MASK;
                dst.type = DMA_TYPE_NORMAL;
            }
            break;

        case 0x7:
            dst.ptr = gba.mem.oam;
            dst.size = mem::OAM_SIZE;
            dst.addr = addr & mem::OAM_MASK;
            dst.type = DMA_TYPE_NORMAL;
            break;

        // todo: change this when merged with libfat branch
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xC:
            if (gba.wfuncmap_8[region] || gba.wfuncmap_16[region] || gba.wfuncmap_32[region])
            {
                dst.type = DMA_TYPE_SLOW;
            }
            else
            {

            }
            dst.size = 0x02000000;
            dst.addr = addr & 0x01FFFFFF;
            dst.type = DMA_TYPE_INVALID;
            break;

        case 0xD:
            if (gba.backup.is_eeprom())
            {
                dst.type = DMA_TYPE_SLOW;
            }
            else if (gba.wfuncmap_8[region] || gba.wfuncmap_16[region] || gba.wfuncmap_32[region])
            {
                dst.type = DMA_TYPE_SLOW;
            }
            else
            {
                dst.size = 0x02000000;
                dst.addr = addr & 0x01FFFFFF;
                dst.type = DMA_TYPE_INVALID;
            }
            break;

        case 0xE:
        case 0xF:
            dst.type = DMA_TYPE_SLOW;
            break;
    }

    return dst;
}

enum DmaTransfer
{
    DMA_TRANSFER_COPY_SRC_INC_DST_INC, // memcpy / memmove
    DMA_TRANSFER_COPY_SRC_INC_DST_DEC, // memcpy / memmove
    DMA_TRANSFER_COPY_SRC_DEC_DST_INC, // memcpy / memmove
    DMA_TRANSFER_COPY_SRC_DEC_DST_DEC, // memcpy / memmove

    DMA_TRANSFER_FIXED_SRC_INC, // memset
    DMA_TRANSFER_FIXED_SRC_DEC, // memset
    DMA_TRANSFER_FIXED_DST_INC, // memset
    DMA_TRANSFER_FIXED_DST_DEC, // memset
    DMA_TRANSFER_FIXED_BOTH, // very strange...

    #if 0
    DMA_TRANSFER_INVALID_SRC_INC, // usually dma from bios
    DMA_TRANSFER_INVALID_SRC_DEC, // usually dma from bios
    DMA_TRANSFER_INVALID_SRC_FIX, // usually dma from bios
    DMA_TRANSFER_INVALID_DST_INC, // usually dma to bios or rom
    DMA_TRANSFER_INVALID_DST_DEC, // usually dma to bios or rom
    DMA_TRANSFER_INVALID_DST_FIX, // usually dma to bios or rom
    DMA_TRANSFER_INVALID_BOTH, // very very strange...
    #endif

    // if set, then slow dma is picked
    DMA_TRANSFER_UNKNOWN,
};

template<typename T, s8 src_inc, s8 dst_inc>
void fast_dma_copy(Gba& gba, Channel& dma, RW src, RW dst)
{
    u32 len = 0;
    const u32 len_end = dma.len;
    const u32 total_cycles = src.get_cycles<T>() + dst.get_cycles<T>();

    do {
        const u32 event_cycles = std::max<u32>(1, gba.scheduler.get_next_event_cycles() / total_cycles);
        const u32 run_length = std::min<u32>(event_cycles, len_end - len);

        for (u32 i = 0; i < run_length; i++)
        {
            dst.ptr[dst.addr + 0] = src.ptr[src.addr + 0];
            dst.ptr[dst.addr + 1] = src.ptr[src.addr + 1];

            if constexpr(std::is_same<T, u32>())
            {
                dst.ptr[dst.addr + 2] = src.ptr[src.addr + 2];
                dst.ptr[dst.addr + 3] = src.ptr[src.addr + 3];
            }

            src.addr += sizeof(T) * src_inc;
            dst.addr += sizeof(T) * dst_inc;
        }

        len += run_length;
        gba.scheduler.tick(run_length * total_cycles);

        if (gba.scheduler.should_fire())
        {
            gba.scheduler.fire();
        }
    } while (len < len_end);

    dma.src_addr += len * sizeof(T) * src_inc;
    dma.dst_addr += len * sizeof(T) * dst_inc;
    dma.len -= len;
}

template<typename T, s8 dst_inc>
void fast_dma_fixed_src(Gba& gba, Channel& dma, RW src, RW dst)
{
    u32 len = 0;
    const u32 len_end = dma.len;
    const u32 total_cycles = src.get_cycles<T>() + dst.get_cycles<T>();

    do {
        const u32 event_cycles = std::max<u32>(1, gba.scheduler.get_next_event_cycles() / total_cycles);
        const u32 run_length = std::min<u32>(event_cycles, len_end - len);

        // as the src never changes, we don't really have to read
        // from src each time, however, this didn't provide any
        // speed improvement, likely the compiler also figured
        // it out and cache doing it's thing.
        for (u32 i = 0; i < run_length; i++)
        {
            dst.ptr[dst.addr + 0] = src.ptr[src.addr + 0];
            dst.ptr[dst.addr + 1] = src.ptr[src.addr + 1];

            if constexpr(std::is_same<T, u32>())
            {
                dst.ptr[dst.addr + 2] = src.ptr[src.addr + 2];
                dst.ptr[dst.addr + 3] = src.ptr[src.addr + 3];
            }

            dst.addr += sizeof(T) * dst_inc;
        }

        len += run_length;
        gba.scheduler.tick(run_length * total_cycles);

        if (gba.scheduler.should_fire())
        {
            gba.scheduler.fire();
        }
    } while (len < len_end);

    dma.dst_addr += len * sizeof(T) * dst_inc;
    dma.len -= len;
}

// memset-like dma to a fixed destination
template<typename T, s8 src_inc>
void fast_dma_fixed_dst(Gba& gba, Channel& dma, RW src, RW dst)
{
    u32 len = 0;
    const u32 len_end = dma.len;
    const u32 total_cycles = src.get_cycles<T>() + dst.get_cycles<T>();

    do {
        const u32 event_cycles = std::max<u32>(1, gba.scheduler.get_next_event_cycles() / total_cycles);
        const u32 run_length = std::min<u32>(event_cycles, len_end - len);

        // because the dst never changes, we can simply skip the addr
        // of the last value that will be written and write that.
        src.addr += (run_length - 1) * sizeof(T) * src_inc;

        dst.ptr[dst.addr + 0] = src.ptr[src.addr + 0];
        dst.ptr[dst.addr + 1] = src.ptr[src.addr + 1];

        if constexpr(std::is_same<T, u32>())
        {
            dst.ptr[dst.addr + 2] = src.ptr[src.addr + 2];
            dst.ptr[dst.addr + 3] = src.ptr[src.addr + 3];
        }

        src.addr += sizeof(T) * src_inc;
        len += run_length;
        gba.scheduler.tick(run_length * total_cycles);

        if (gba.scheduler.should_fire())
        {
            gba.scheduler.fire();
        }
    } while (len < len_end);

    dma.src_addr += len * sizeof(T) * src_inc;
    dma.len -= len;
}

template<typename T>
void fast_dma_fixed_both(Gba& gba, Channel& dma, RW src, RW dst)
{
    const u32 total_cycles = src.get_cycles<T>() + dst.get_cycles<T>();
    u32 len = dma.len;

    do {
        const u32 event_cycles = std::max<u32>(1, gba.scheduler.get_next_event_cycles() / total_cycles);
        const u32 run_length = std::min<u32>(event_cycles, len);

        // because both the src and dst do not change, there is no
        // need for a loop, a single write is the same thing.
        dst.ptr[dst.addr + 0] = src.ptr[src.addr + 0];
        dst.ptr[dst.addr + 1] = src.ptr[src.addr + 1];

        if constexpr(std::is_same<T, u32>())
        {
            dst.ptr[dst.addr + 2] = src.ptr[src.addr + 2];
            dst.ptr[dst.addr + 3] = src.ptr[src.addr + 3];
        }

        len -= run_length;
        dma.len = len;
        gba.scheduler.tick(run_length * total_cycles);

        if (gba.scheduler.should_fire())
        {
            gba.scheduler.fire();
        }
    } while (len > 0);
}

// simply, if a dma is to / from
template<typename T>
void fast_dma_setup(Gba& gba, Channel& dma)
{
    static_assert(std::is_same<T, u16>() || std::is_same<T, u32>());

    const auto src = get_read_data(gba, dma.src_addr);
    const auto dst = get_write_data(gba, dma.dst_addr);

    const u32 max_len_inc = dma.len * sizeof(T);
    const s32 max_len_dec = dma.len * sizeof(T) * -1;

    // never handle any copy to or from IO, too many edge cases
    if (!src.ptr || !dst.ptr)
    {
        return;
    }

    // figure out what type of dma we are going to do
    DmaTransfer tranfer_type;
    bool clipped;

    // the below figures out is the dma is a memcpy or memset
    // and whether or not the transfer will be clipped
    // clipped meaning that transfer underflows /overflows into the
    // next memory region.

    // check for copy
    if (dma.src_increment > 0 && dma.dst_increment > 0)
    {
        tranfer_type = DMA_TRANSFER_COPY_SRC_INC_DST_INC;
        clipped = src.is_oob(max_len_inc) || dst.is_oob(max_len_inc);
    }
    else if (dma.src_increment > 0 && dma.dst_increment < 0)
    {
        tranfer_type = DMA_TRANSFER_COPY_SRC_INC_DST_DEC;
        clipped = src.is_oob(max_len_inc) || dst.is_oob(max_len_dec);
    }
    else if (dma.src_increment < 0 && dma.dst_increment > 0)
    {
        tranfer_type = DMA_TRANSFER_COPY_SRC_DEC_DST_INC;
        clipped = src.is_oob(max_len_dec) || dst.is_oob(max_len_inc);
    }
    else if (dma.src_increment < 0 && dma.dst_increment < 0)
    {
        tranfer_type = DMA_TRANSFER_COPY_SRC_DEC_DST_DEC;
        clipped = src.is_oob(max_len_dec) || dst.is_oob(max_len_dec);
    }
    // check for fixed
    else if (dma.src_increment == 0 && dma.dst_increment > 0)
    {
        tranfer_type = DMA_TRANSFER_FIXED_SRC_INC;
        clipped = dst.is_oob(max_len_inc);
    }
    else if (dma.src_increment == 0 && dma.dst_increment < 0)
    {
        tranfer_type = DMA_TRANSFER_FIXED_SRC_DEC;
        clipped = dst.is_oob(max_len_dec);
    }
    else if (dma.src_increment > 0 && dma.dst_increment == 0)
    {
        tranfer_type = DMA_TRANSFER_FIXED_DST_INC;
        clipped = src.is_oob(max_len_inc);
    }
    else if (dma.src_increment < 0 && dma.dst_increment == 0)
    {
        tranfer_type = DMA_TRANSFER_FIXED_DST_DEC;
        clipped = src.is_oob(max_len_dec);
    }
    else if (dma.src_increment == 0 && dma.dst_increment == 0)
    {
        tranfer_type = DMA_TRANSFER_FIXED_BOTH;
        clipped = false;
    }
    else
    {
        tranfer_type = DMA_TRANSFER_UNKNOWN;
        clipped = false;
    }

    // clipped dma's are not handled in the fast path yet!
    if (clipped)
    {
        return;
    }

    switch (tranfer_type)
    {
        case DMA_TRANSFER_COPY_SRC_INC_DST_INC: fast_dma_copy<T, +1, +1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_COPY_SRC_INC_DST_DEC: fast_dma_copy<T, +1, -1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_COPY_SRC_DEC_DST_INC: fast_dma_copy<T, -1, +1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_COPY_SRC_DEC_DST_DEC: fast_dma_copy<T, -1, -1>(gba, dma, src, dst); break;

        case DMA_TRANSFER_FIXED_SRC_INC: fast_dma_fixed_src<T, +1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_FIXED_SRC_DEC: fast_dma_fixed_src<T, -1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_FIXED_DST_INC: fast_dma_fixed_dst<T, +1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_FIXED_DST_DEC: fast_dma_fixed_dst<T, -1>(gba, dma, src, dst); break;
        case DMA_TRANSFER_FIXED_BOTH: fast_dma_fixed_both<T>(gba, dma, src, dst); break;

        case DMA_TRANSFER_UNKNOWN:
            break;
    }
}

template<bool Special = false>
auto start_dma(Gba& gba, Channel& dma, const u8 channel_num) -> void
{
    log::print_info(gba, LOG_TYPE[channel_num], "firing dma from: 0x%08X to: 0x%08X len: 0x%04X\n", dma.src_addr, dma.dst_addr, dma.len, dma.size_type);

    const auto len = dma.len;
    // const auto src = dma.src_addr;
    const auto dst = dma.dst_addr;

    if constexpr(Special)
    {
        dma.src_addr = mem::align<u32>(dma.src_addr);

        for (int i = 0; i < 4; i++)
        {
            dma.src_addr &= SRC_MASK[channel_num];
            dma.dst_addr &= DST_MASK[channel_num];

            const auto value = mem::read32(gba, dma.src_addr);
            apu::on_fifo_write32(gba, value, channel_num-1);
            gba.scheduler.tick(1); // for fifo write

            dma.src_addr += dma.src_increment;

            advance_scheduler(gba);
        }
    }
    else
    {
        // eeprom size detection
        if (channel_num == 3) // todo: make constexpr
        {
            // order is important here because we dont want
            // to read from union if not eeprom as thats UB in C / C++.
            if (gba.backup.is_eeprom() && dma.dst_addr >= 0x0D000000 && dma.dst_addr <= 0x0DFFFFFFF)
            {
                if (gba.backup.eeprom.width == backup::eeprom::Width::unknown)
                {
                    // values and what they mean!
                    // 9: exact number of bits setup a eeprom read
                    // 73: exact number of bits to setup and complete an eeprom write (gaunlet uses this)
                    if (dma.len == 17 || dma.len == 81)
                    {
                        gba.backup.eeprom.set_width(gba, backup::eeprom::Width::beeg);
                    }
                    else if (dma.len == 9 || dma.len == 73)
                    {
                        gba.backup.eeprom.set_width(gba, backup::eeprom::Width::small);
                    }
                    else
                    {
                        assert(!"unknown dma len for setting eeprom width!");
                    }
                }
            }
        }

        if (dma.size_type == SizeType::half)
        {
            fast_dma_setup<u16>(gba, dma);
        }
        else
        {
            fast_dma_setup<u32>(gba, dma);
        }

        switch (dma.size_type)
        {
            case SizeType::half:
                while (dma.len--)
                {
                    dma.src_addr &= SRC_MASK[channel_num];
                    dma.dst_addr &= DST_MASK[channel_num];

                    const auto value = mem::read16(gba, dma.src_addr);
                    mem::write16(gba, dma.dst_addr, value);

                    dma.src_addr += dma.src_increment;
                    dma.dst_addr += dma.dst_increment;

                    advance_scheduler(gba);
                }
                break;

            case SizeType::word:
                while (dma.len--)
                {
                    dma.src_addr &= SRC_MASK[channel_num];
                    dma.dst_addr &= DST_MASK[channel_num];

                    const auto value = mem::read32(gba, dma.src_addr);
                    mem::write32(gba, dma.dst_addr, value);

                    dma.src_addr += dma.src_increment;
                    dma.dst_addr += dma.dst_increment;

                    advance_scheduler(gba);
                }
                break;
        }

        gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_DMA, dst, dma.dst_addr);
    }

    if (dma.irq)
    {
        arm7tdmi::fire_interrupt(gba, INTERRUPTS[channel_num]);
    }

    if (dma.repeat && dma.mode != Mode::immediate)
    {
        [[maybe_unused]] const auto [sad, dad, cnt_h, cnt_l] = get_channel_registers(gba, channel_num);

        // reload len if repeat is set
        if (dma.mode != Mode::special)
        {
            assert(len == cnt_l);
        }
        dma.len = len;
        // dma.len = cnt_l;
        // optionally reload dst if increment type 3 is used
        if (dma.dst_increment_type == IncrementType::special)
        {
            assert(dst == dad);
            dma.dst_addr = dst;
            // dma.dst_addr = dad;
        }
    }
    else
    {
        disable_channel(gba, channel_num);
    }
}

} // namespace

auto on_hblank(Gba& gba) -> void
{
    for (auto i = 0; i < 4; i++)
    {
        if (gba.dma[i].enabled && gba.dma[i].mode == Mode::hblank)
        {
            log::print_info(gba, LOG_TYPE[i], "firing hdma: %u len: 0x%08X dst: 0x%08X src: 0x%08X dst_inc: %d src_inc: %d R: %u\n", i, gba.dma[i].len, gba.dma[i].dst_addr, gba.dma[i].src_addr, gba.dma[i].dst_increment, gba.dma[i].src_increment, gba.dma[i].repeat);
            start_dma(gba, gba.dma[i], i); // i think we only handle 1 dma at a time?
        }
    }
}

auto on_vblank(Gba& gba) -> void
{
    for (auto i = 0; i < 4; i++)
    {
        if (gba.dma[i].enabled && gba.dma[i].mode == Mode::vblank)
        {
            log::print_info(gba, LOG_TYPE[i], "firing vdma: %u len: 0x%08X dst: 0x%08X src: 0x%08X dst_inc: %d src_inc: %d R: %u\n", i, gba.dma[i].len, gba.dma[i].dst_addr, gba.dma[i].src_addr, gba.dma[i].dst_increment, gba.dma[i].src_increment, gba.dma[i].repeat);
            start_dma(gba, gba.dma[i], i); // i think we only handle 1 dma at a time?
        }
    }
}

auto on_dma3_special(Gba& gba) -> void
{
    constexpr auto dma3 = 3;

    if (gba.dma[dma3].enabled && gba.dma[dma3].mode == Mode::special)
    {
        if (REG_VCOUNT == 162)
        {
            disable_channel(gba, dma3);
        }
        else
        {
            log::print_info(gba, LOG_TYPE[dma3], "firing dma3-special len: 0x%08X dst: 0x%08X src: 0x%08X dst_inc: %d src_inc: %d R: %u\n", gba.dma[dma3].len, gba.dma[dma3].dst_addr, gba.dma[dma3].src_addr, gba.dma[dma3].dst_increment, gba.dma[dma3].src_increment, gba.dma[dma3].repeat);
            start_dma(gba, gba.dma[dma3], dma3);
        }
    }
}

auto on_fifo_empty(Gba& gba, u8 num) -> void
{
    num++;

    if (num == 1 && gba.dma[num].dst_addr != mem::IO_FIFO_A_L && gba.dma[num].mode == Mode::special)
    {
        log::print_warn(gba, log::Type::DMA1, "bad fifo addr: 0x%08X\n", gba.dma[num].dst_addr);
        assert(0);
        return;
    }
    if (num == 2 && gba.dma[num].dst_addr != mem::IO_FIFO_B_L && gba.dma[num].mode == Mode::special)
    {
        log::print_warn(gba, log::Type::DMA2, "bad fifo addr: 0x%08X\n", gba.dma[num].dst_addr);
        assert(0);
        return;
    }

    if (gba.dma[num].enabled && gba.dma[num].mode == Mode::special)
    {
        start_dma<true>(gba, gba.dma[num], num); // i think we only handle 1 dma at a time?
    }
}

auto on_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);

    for (auto i = 0; i < 4; i++)
    {
        if (gba.dma[i].enabled && gba.dma[i].mode == Mode::immediate)
        {
            start_dma(gba, gba.dma[i], i);
        }
    }
}

auto on_cnt_write(Gba& gba, const u8 channel_num) -> void
{
    assert(channel_num <= 3);
    const auto [sad, dad, cnt_h, cnt_l] = get_channel_registers(gba, channel_num);

    const auto dst_increment_type = static_cast<IncrementType>(bit::get_range<5, 6>(cnt_h)); // dst
    const auto src_increment_type = static_cast<IncrementType>(bit::get_range<7, 8>(cnt_h)); // src
    const auto repeat = bit::is_set<9>(cnt_h); // repeat
    const auto size_type = static_cast<SizeType>(bit::is_set<10>(cnt_h));
    // const auto U = bit::is_set<11>(cnt_h); // unk
    const auto mode = static_cast<Mode>(bit::get_range<12, 13>(cnt_h));
    const auto irq_enable = bit::is_set<14>(cnt_h); // irq
    const auto dma_enable = bit::is_set<15>(cnt_h); // enable flag

    const auto src = sad; // address is masked on r/w
    const auto dst = dad; // address is masked on r/w
    const auto len = cnt_l;

    // load data into registers
    auto& dma = gba.dma[channel_num];

    const auto was_enabled = dma.enabled;

    // update the dma enabled flag
    dma.enabled = dma_enable;

    if (!was_enabled && dma.enabled)
    {
        log::print_info(gba, LOG_TYPE[channel_num], "enabling dma\n");
    }
    else if (was_enabled && !dma.enabled)
    {
        log::print_info(gba, LOG_TYPE[channel_num], "disabling dma\n");
    }

    // dma only updates internal registers on enable bit 0->1 transition(?)
    // i think immediate dmas are only fired on 0->1 as well(?)
    // TODO: verify this
    if (!dma_enable || was_enabled)
    {
        return;
    }

    // load data into registers
    dma.dst_increment_type = dst_increment_type;
    dma.src_increment_type = src_increment_type;
    dma.repeat = repeat;
    dma.size_type = size_type;
    dma.mode = mode;
    dma.irq = irq_enable;

    dma.dst_addr = dst;
    dma.src_addr = src;
    dma.len = len;

    // handle len=0, set len to max
    if (dma.len == 0)
    {
        if (channel_num == 3)
        {
            dma.len = 0x10000;
        }
        else
        {
            dma.len = 0x4000;
        }
    }

    assert(dma.enabled && "shouldnt get here if dma is disabled");

    if (dma.mode == Mode::special)
    {
        switch (channel_num)
        {
            case 0:
                break;

            // fifo dma
            case 1: case 2:
                dma.len = 4;
                // forced to word, openlara needs this
                dma.size_type = SizeType::word;
                dma.dst_increment_type = IncrementType::special;
                dma.dst_increment = 0;
                break;

            // video transfer dma
            case 3:
                // assert(!"DMA3 special transfer not implemented!");
                assert(dma.repeat && "repeat bit not set for DMA3 special");
                break;
        }
    }

    // sort increments and force alignment of src/dst addr
    switch (dma.size_type)
    {
        case SizeType::half:
            dma.src_increment = 2;
            dma.dst_increment = 2;

            // assert(!(dma.src_addr & 0x1) && "dma addr not aligned");
            dma.src_addr = mem::align<u16>(dma.src_addr);
            // assert(!(dma.dst_addr & 0x1) && "dma addr not aligned");
            dma.dst_addr = mem::align<u16>(dma.dst_addr);
            break;

        case SizeType::word:
            dma.src_increment = 4;
            dma.dst_increment = 4;

            // assert(!(dma.src_addr & 0x3) && "dma addr not aligned");
            dma.src_addr = mem::align<u32>(dma.src_addr);
            // assert(!(dma.dst_addr & 0x3) && "dma addr not aligned");
            dma.dst_addr = mem::align<u32>(dma.dst_addr);
            break;
    }

    // update increment based on type
    const auto func = [](IncrementType type, auto& inc)
    {
        switch (type)
        {
            // already handled
            case IncrementType::inc:
             // same as increment, only that it reloads dst if R is set.
            case IncrementType::special:
                inc = +inc;
                break;

            // goes down
            case IncrementType::dec:
                inc = -inc;
                break;

            // don't increment
            case IncrementType::unchanged:
                inc = 0;
                break;
        }
    };

    func(dma.src_increment_type, dma.src_increment);
    func(dma.dst_increment_type, dma.dst_increment);

    // check if we should start transfer now
    if (dma.mode == Mode::immediate)
    {
        // dmas are delayed
        // start_dma(gba, dma, channel_num);
        gba.scheduler.add(scheduler::ID::DMA, 3, on_event, &gba);
        // gba.scheduler.add(scheduler::ID::DMA, 2, on_event, &gba);
    }
}

} // namespace gba::dma
