// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "scheduler.hpp"
#include "gba.hpp"
#include "timer.hpp"
#include "dma.hpp"
#include <cstddef>
#include <cstdint>

namespace gba::scheduler
{
#define SCHEDULER gba.scheduler

constexpr auto RESET_CYCLES = INT32_MAX;

auto on_reset_event(Gba& gba) -> void
{
    printf("panic mode\n");
    // assert(0 && "scheduler oveflow!");
    SCHEDULER.cycles -= RESET_CYCLES;
    if (SCHEDULER.next_event_cycles < RESET_CYCLES)
    {
        printf("next mode is bad\n");
    }
    SCHEDULER.next_event_cycles -= RESET_CYCLES;

    for (auto& entry : SCHEDULER.entries)
    {
        if (entry.enabled)
        {
            if (entry.cycles < RESET_CYCLES)
            {
                printf("cycles is bad\n");
            }
            entry.cycles -= RESET_CYCLES;
        }
    }

    add(gba, Event::RESET, on_reset_event, RESET_CYCLES);
}

auto reset(Gba& gba) -> void
{
    for (auto& entry : SCHEDULER.entries)
    {
        entry.enabled = false;
    }

    SCHEDULER.cycles = 0;
    SCHEDULER.next_event_cycles = RESET_CYCLES;
    add(gba, Event::RESET, on_reset_event, RESET_CYCLES);
}

// reload events
auto on_loadstate(Gba& gba) -> void
{
    for (std::size_t i = 0; i < SCHEDULER.entries.size(); i++)
    {
        auto& entry = SCHEDULER.entries[i];

        if (entry.enabled)
        {
            switch (static_cast<Event>(i))
            {
                case Event::PPU: entry.cb = ppu::on_event; break;
                case Event::APU_SQUARE0: entry.cb = apu::on_square0_event; break;
                case Event::APU_SQUARE1: entry.cb = apu::on_square1_event; break;
                case Event::APU_WAVE: entry.cb = apu::on_wave_event; break;
                case Event::APU_NOISE: entry.cb = apu::on_noise_event; break;
                case Event::APU_FRAME_SEQUENCER: entry.cb = apu::on_frame_sequencer_event; break;
                case Event::APU_SAMPLE: entry.cb = apu::on_sample_event; break;
                case Event::TIMER0: entry.cb = timer::on_timer0_event; break;
                case Event::TIMER1: entry.cb = timer::on_timer1_event; break;
                case Event::TIMER2: entry.cb = timer::on_timer2_event; break;
                case Event::TIMER3: entry.cb = timer::on_timer3_event; break;
                case Event::DMA: entry.cb = dma::on_event; break;
                case Event::INTERRUPT: entry.cb = arm7tdmi::on_interrupt_event; break;
                case Event::HALT: entry.cb = arm7tdmi::on_halt_event; break;
                case Event::RESET: entry.cb = scheduler::on_reset_event; break;
            }
        }
    }
}

auto find_next_event(Gba& gba)
{
    u32 next_cycles = UINT32_MAX;
    std::size_t index = 0;

    for (std::size_t i = 0; i < SCHEDULER.entries.size(); i++)
    {
        auto& entry = SCHEDULER.entries[i];

        if (entry.enabled && entry.cycles <= SCHEDULER.cycles)
        {
            entry.enabled = false;
            entry.cb(gba);
        }
    }

    for (std::size_t i = 0; i < SCHEDULER.entries.size(); i++)
    {
        const auto& entry = SCHEDULER.entries[i];
        if (entry.enabled)
        {
            if (entry.cycles < next_cycles)
            {
                next_cycles = entry.cycles;
                index = i;
            }
        }
    }

    SCHEDULER.next_event_cycles = next_cycles;
    SCHEDULER.next_event = static_cast<Event>(index);
}

// 1,018.8 KiB (1,043,208)
auto fire(Gba& gba) -> void
{
    const auto next_event = SCHEDULER.next_event;
    auto& entry = SCHEDULER.entries[static_cast<u8>(next_event)];
    entry.enabled = false;
    // printf("firing event: %u\n", static_cast<u8>(SCHEDULER.next_event));
    if (SCHEDULER.next_event == Event::HALT)
    {
        find_next_event(gba);
    }
    entry.cb(gba);
    // printf("did it\n");
    find_next_event(gba);
}

auto add(Gba& gba, Event e, callback cb, u32 cycles) -> void
{
    if (e != Event::PPU && e != Event::RESET && e != Event::INTERRUPT && e != Event::TIMER0 && e != Event::TIMER1 && e != Event::TIMER2  && e != Event::TIMER3 && e != Event::APU_SAMPLE && e != Event::DMA && e != Event::HALT)
    {
        // return;
        // find_next_event(gba);
    }

    auto& entry = SCHEDULER.entries[static_cast<u8>(e)];
    entry.cb = cb;
    entry.cycles = SCHEDULER.cycles + cycles;
    entry.enabled = true;

    // check if the new event if sooner than current event
    if (entry.cycles <= SCHEDULER.next_event_cycles)
    {
        SCHEDULER.next_event_cycles = entry.cycles;
        SCHEDULER.next_event = e;
        // printf("event added yay: %u\n", static_cast<u8>(SCHEDULER.next_event));
    }

}

auto remove(Gba& gba, Event e) -> void
{
    auto& entry = SCHEDULER.entries[static_cast<u8>(e)];
    entry.enabled = false;

    // check if we are removing our current event
    if (SCHEDULER.next_event == e)
    {
        find_next_event(gba);
    }
}

} // namespace gba::scheduler
