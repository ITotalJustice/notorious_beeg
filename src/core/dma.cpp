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
#include <utility> // for std::unreachable c++23

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#DMA%20Source%20Registers
// https://problemkaputt.de/gbatek.htm#gbadmatransfers
namespace gba::dma {
namespace {

constexpr auto INTERNAL_MEMORY_RANGE = 0x07FFFFFF;
constexpr auto ANY_MEMORY_RANGE = 0x0FFFFFFF;

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
        case 0: return { REG_DMA0SAD, REG_DMA0DAD, REG_DMA0CNT_H, REG_DMA0CNT_L };
        case 1: return { REG_DMA1SAD, REG_DMA1DAD, REG_DMA1CNT_H, REG_DMA1CNT_L };
        case 2: return { REG_DMA2SAD, REG_DMA2DAD, REG_DMA2CNT_H, REG_DMA2CNT_L };
        case 3: return { REG_DMA3SAD, REG_DMA3DAD, REG_DMA3CNT_H, REG_DMA3CNT_L };
    }

    std::unreachable();
}

template<bool Special = false>
auto start_dma(Gba& gba, Channel& dma, const u8 channel_num) -> void
{
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
            gba.scheduler.tick(1); // for fifo write
            apu::on_fifo_write32(gba, value, channel_num-1);
            dma.src_addr += dma.src_increment;
        }
    }
    else
    {
        // eeprom size detection
        if (channel_num == 3) // todo: make constexpr
        {
            // order is important here because we dont want
            // to read from union if not eeprom as thats UB in C / C++.
            if (gba.backup.type == backup::Type::EEPROM && dma.dst_addr >= 0x0D000000 && dma.dst_addr <= 0x0DFFFFFFF)
            {
                auto width = backup::eeprom::Width::unknown;

                if (dma.len > 9)
                {
                    width = backup::eeprom::Width::beeg;
                }
                else
                {
                    width = backup::eeprom::Width::small;
                }

                gba.backup.eeprom.set_width(width);
            }
        }

        switch (dma.size_type)
        {
            case SizeType::half:
                dma.src_addr = mem::align<u16>(dma.src_addr);
                dma.dst_addr = mem::align<u16>(dma.dst_addr);

                while (dma.len--)
                {
                    dma.src_addr &= SRC_MASK[channel_num];
                    dma.dst_addr &= DST_MASK[channel_num];

                    const auto value = mem::read16(gba, dma.src_addr);
                    mem::write16(gba, dma.dst_addr, value);

                    dma.src_addr += dma.src_increment;
                    dma.dst_addr += dma.dst_increment;
                }
                break;

            case SizeType::word:
                dma.src_addr = mem::align<u32>(dma.src_addr);
                dma.dst_addr = mem::align<u32>(dma.dst_addr);

                while (dma.len--)
                {
                    dma.src_addr &= SRC_MASK[channel_num];
                    dma.dst_addr &= DST_MASK[channel_num];

                    const auto value = mem::read32(gba, dma.src_addr);
                    mem::write32(gba, dma.dst_addr, value);

                    dma.src_addr += dma.src_increment;
                    dma.dst_addr += dma.dst_increment;
                }
                break;
        }
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
        switch (channel_num)
        {
            case 0: REG_DMA0CNT_H = bit::unset<15>(REG_DMA0CNT_H); break;
            case 1: REG_DMA1CNT_H = bit::unset<15>(REG_DMA1CNT_H); break;
            case 2: REG_DMA2CNT_H = bit::unset<15>(REG_DMA2CNT_H); break;
            case 3: REG_DMA3CNT_H = bit::unset<15>(REG_DMA3CNT_H); break;
        }
        dma.enabled = false;
    }
}

} // namespace

auto on_hblank(Gba& gba) -> void
{
    for (auto i = 0; i < 4; i++)
    {
        if (gba.dma[i].enabled && gba.dma[i].mode == Mode::hblank)
        {
            // std::printf("firing hdma: %u len: %08X dst: %08X src: 0x%08X dst_inc: %d src_inc: %d R: %u\n", i, gba.dma[i].len, gba.dma[i].dst_addr, gba.dma[i].src_addr, gba.dma[i].dst_increment, gba.dma[i].src_increment, gba.dma[i].repeat);
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
            // std::printf("firing vdma: %u len: %08X dst: %08X src: 0x%08X dst_inc: %d src_inc: %d R: %u\n", i, gba.dma[i].len, gba.dma[i].dst_addr, gba.dma[i].src_addr, gba.dma[i].dst_increment, gba.dma[i].src_increment, gba.dma[i].repeat);
            start_dma(gba, gba.dma[i], i); // i think we only handle 1 dma at a time?
        }
    }
}

auto on_fifo_empty(Gba& gba, u8 num) -> void
{
    num++;

    if (num == 1 && gba.dma[num].dst_addr != mem::IO_FIFO_A_L && gba.dma[num].mode == Mode::special)
    {
        gba_log("addr: 0x%08X\n", gba.dma[num].dst_addr);
        assert(0);
        return;
    }
    if (num == 2 && gba.dma[num].dst_addr != mem::IO_FIFO_B_L && gba.dma[num].mode == Mode::special)
    {
        gba_log("addr: 0x%08X\n", gba.dma[num].dst_addr);
        assert(0);
        return;
    }

    if (gba.dma[num].enabled && gba.dma[num].mode == Mode::special)
    {
        // std::printf("firing dma in fifo: %u\n", num-1);
        start_dma<true>(gba, gba.dma[num], num); // i think we only handle 1 dma at a time?
    }
}

auto on_event(Gba& gba) -> void
{
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

    const auto B = static_cast<IncrementType>(bit::get_range<5, 6>(cnt_h)); // dst
    const auto A = static_cast<IncrementType>(bit::get_range<7, 8>(cnt_h)); // src
    const auto R = bit::is_set<9>(cnt_h); // repeat
    const auto S = static_cast<SizeType>(bit::is_set<10>(cnt_h));
    // const auto U = bit::is_set<11>(cnt_h); // unk
    const auto M = static_cast<Mode>(bit::get_range<12, 13>(cnt_h));
    const auto I = bit::is_set<14>(cnt_h); // irq
    const auto N = bit::is_set<15>(cnt_h); // enable flag

    const auto src = sad; // address is masked on r/w
    const auto dst = dad; // address is masked on r/w
    const auto len = cnt_l;

    // std::printf("[dma%u] src: 0x%08X dst: 0x%08X len: 0x%04X B: %u A: %u R: %u S: %u M: %u I: %u N: %u\n", channel_num, src, dst, len, (u8)B, (u8)A, R, (u8)S, (u8)M, I, N);

    // load data into registers
    auto& dma = gba.dma[channel_num];

    // see if the channel is to be stopped
    if (!N)
    {
        dma.enabled = false;
        // std::printf("[dma%u] disabled\n", channel_num);
        return;
    }

    // load data into registers
    dma.dst_increment_type = B;
    dma.src_increment_type = A;
    dma.repeat = R;
    dma.size_type = S;
    dma.mode = M;
    dma.irq = I;

    // these can only be reloaded if going from 0-1 (off then on)
    if (N && !dma.enabled)
    {
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
    }
    else
    {
        if (dma.src_addr != src)
        {
            // std::printf("[dma%u] skipping reloading of src, old: 0x%08X new: 0x%08X\n", channel_num, dma.src_addr, src);
        }
    }
    dma.enabled = N;

    assert(dma.enabled && "shouldnt get here if dma is disabled");

    if (dma.mode == Mode::special)
    {
        dma.len = 4;
        // forced to word, openlara needs this
        dma.size_type = SizeType::word;
        dma.dst_increment_type = IncrementType::special;
        dma.dst_increment = 0;
    }

    // sort increments
    switch (dma.size_type)
    {
        case SizeType::half:
            dma.src_increment = 2;
            dma.dst_increment = 2;
            break;

        case SizeType::word:
            dma.src_increment = 4;
            dma.dst_increment = 4;
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
                break;

            // goes down
            case IncrementType::dec:
                inc *= -1;
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
        #if ENABLE_SCHEDULER
        // start_dma(gba, dma, channel_num);
        scheduler::add(gba, scheduler::Event::DMA, on_event, 0);
        #else
        start_dma(gba, dma, channel_num);
        #endif
    }
}

} // namespace gba::dma
