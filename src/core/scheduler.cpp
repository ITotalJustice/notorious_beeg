// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "scheduler.hpp"
#include "gba.hpp"
#include "timer.hpp"
#include "dma.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace gba::scheduler {
namespace {

// we start at the highest possible value for delta so that
// should delta ever be maxed (it wont), then we can ensure that
// once RESET event fires, all enabled events will be >= RESET_CYCLES
// then all events are -= RESET_CYCLES, so they will start between
// 0-START_CYCLES.
// we could also just use an int32 instead and start at 0,
// however the int32 is measureably slower than an unsigned value.
constexpr auto START_CYCLES = INT16_MAX;
constexpr auto RESET_CYCLES = INT32_MAX;

auto fire_all_expired_events(Gba& gba) -> bool
{
    bool fire_halt = false;

    for (std::size_t i = 0; i < gba.scheduler.entries.size(); i++)
    {
        auto& entry = gba.scheduler.entries[i];
        assert(entry.delta <= 0);

        if (entry.enabled && entry.cycles <= gba.scheduler.cycles)
        {
            if (static_cast<Event>(i) == Event::HALT) [[unlikely]]
            {
                fire_halt = true;
            }
            else
            {
                assert(static_cast<Event>(i) != Event::HALT);
                entry.enabled = false;

                if (static_cast<Event>(i) != Event::HALT && static_cast<Event>(i) != Event::DMA)
                {
                    entry.delta = entry.cycles - gba.scheduler.cycles;
                    assert(entry.delta <= 0);
                }
                entry.cb(gba);
            }
        }
    }

    return fire_halt;
}

auto on_reset_event(Gba& gba) -> void
{
    // first thing to do is to flush all events which also
    // expire at the same time!
    fire_all_expired_events(gba);

    assert(gba.scheduler.next_event_cycles >= RESET_CYCLES);
    gba.scheduler.next_event_cycles -= RESET_CYCLES;

    for (auto& entry : gba.scheduler.entries)
    {
        if (entry.enabled)
        {
            assert(entry.cycles >= RESET_CYCLES);
            entry.cycles -= RESET_CYCLES;
        }
    }

    // also update timers otherwise reads will be very broken
    for (auto& timer : gba.timer)
    {
        if (timer.enable)
        {
            timer::update_timer(gba, timer);
            assert(timer.event_time >= RESET_CYCLES);
            timer.event_time -= RESET_CYCLES;
        }
    }

    // very important to reset this last as update_timer needs it
    assert(gba.scheduler.cycles >= RESET_CYCLES);
    gba.scheduler.cycles -= RESET_CYCLES;

    add(gba, Event::RESET, on_reset_event, RESET_CYCLES);
}

auto find_next_event(Gba& gba, bool fire)
{
    u32 next_cycles = UINT32_MAX;
    u8 index = 0;
    bool want_halt = false;

    if (fire)
    {
        want_halt = fire_all_expired_events(gba);
    }

    for (std::size_t i = 0; i < gba.scheduler.entries.size(); i++)
    {
        const auto& entry = gba.scheduler.entries[i];
        if (entry.enabled)
        {
            if (entry.cycles <= next_cycles)
            {
                next_cycles = entry.cycles;
                index = i;
            }
        }
    }

    gba.scheduler.next_event_cycles = next_cycles;
    gba.scheduler.next_event = static_cast<Event>(index);

    if (want_halt) [[unlikely]]
    {
        scheduler::fire(gba);
    }
}

} // namespace

auto reset(Gba& gba) -> void
{
    gba.scheduler = {};

    gba.scheduler.cycles = START_CYCLES;
    gba.scheduler.next_event_cycles = RESET_CYCLES;
    add(gba, Event::RESET, on_reset_event, RESET_CYCLES);
}

// reload events
auto on_loadstate(Gba& gba) -> void
{
    for (std::size_t i = 0; i < gba.scheduler.entries.size(); i++)
    {
        auto& entry = gba.scheduler.entries[i];

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
                case Event::FRAME: /*this will get set on run() anyway*/ break;
                case Event::RESET: entry.cb = scheduler::on_reset_event; break;
                case Event::END: assert(!"Event::END somehow in array"); break;
            }
        }
    }
}

auto fire(Gba& gba) -> void
{
    const auto next_event = gba.scheduler.next_event;
    auto& entry = gba.scheduler.entries[std::to_underlying(next_event)];
    entry.enabled = false;
    gba.scheduler.next_event = Event::END;

    // calculate delta so that we don't drift
    if (next_event != Event::HALT && next_event != Event::DMA)
    {
        assert(entry.cycles <= gba.scheduler.cycles);
        entry.delta = entry.cycles - gba.scheduler.cycles;
        assert(entry.cycles <= gba.scheduler.cycles);
        assert(entry.delta <= 0); // todo: debug this with emerald
    }

    if (next_event == Event::HALT)
    {
        // halt event loops over all events until halt is exited
        // so we need to change events and fire any expired events
        find_next_event(gba, true);
        assert(gba.scheduler.next_event != Event::HALT);
    }

    entry.cb(gba);
    // we need to see what our next event is going to be
    // we should also fire all events that have expired
    // this can include events that expire at the exact same time.
    find_next_event(gba, true);
}

auto add(Gba& gba, Event e, callback cb, u32 cycles) -> void
{
    // used for filtering out events when debugging
    // if (e != Event::PPU && e != Event::RESET && e != Event::INTERRUPT && e != Event::TIMER0 && e != Event::TIMER2 && e != Event::TIMER3)
    // {
    //     return;
    // }

    auto& entry = gba.scheduler.entries[std::to_underlying(e)];
    entry.cb = cb;
    entry.cycles = (gba.scheduler.cycles + cycles) + entry.delta;
    entry.enabled = true;
    entry.delta = 0;

    // check if the new event if sooner than current event
    if (entry.cycles < gba.scheduler.next_event_cycles)
    {
        gba.scheduler.next_event_cycles = entry.cycles;
        gba.scheduler.next_event = e;
    }
    // check if we are adding the same event but the cycles have increased
    // at which point, we have to slowly find the next event again
    else if (e == gba.scheduler.next_event && entry.cycles > gba.scheduler.next_event_cycles)
    {
        find_next_event(gba, false);
    }
}

auto remove(Gba& gba, Event e) -> void
{
    auto& entry = gba.scheduler.entries[std::to_underlying(e)];
    entry.enabled = false;
    entry.delta = 0;

    // check if we are removing our current event
    if (gba.scheduler.next_event == e)
    {
        find_next_event(gba, false);
    }
}

} // namespace gba::scheduler
