// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "apu/apu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "timer.hpp"
#include "bit.hpp"
#include "arm7tdmi/arm7tdmi.hpp"

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#Timer%20registers
namespace gba::timer {
namespace {

constexpr arm7tdmi::Interrupt INTERRUPT[4] =
{
    arm7tdmi::Interrupt::Timer0,
    arm7tdmi::Interrupt::Timer1,
    arm7tdmi::Interrupt::Timer2,
    arm7tdmi::Interrupt::Timer3,
};

constexpr scheduler::Event EVENTS[4] =
{
    scheduler::Event::TIMER0,
    scheduler::Event::TIMER1,
    scheduler::Event::TIMER2,
    scheduler::Event::TIMER3,
};

constexpr scheduler::callback CALLBACKS[4] =
{
    on_timer0_event,
    on_timer1_event,
    on_timer2_event,
    on_timer3_event,
};

auto add_timer_event(Gba& gba, Timer& timer, u8 num, auto offset) -> void
{
    // don't add timer if cascade is enabled (and not timer0)
    if (num != 0 && timer.cascade)
    {
        return;
    }

    const auto value = (0x10000 - timer.counter) * timer.freq;
    assert(value >= 1);
    assert(timer.counter <= 0xFFFF);
    timer.event_time = gba.scheduler.cycles;
    scheduler::add(gba, EVENTS[num], CALLBACKS[num], value + offset);
}

template<u8 Number>
auto on_overflow(Gba& gba) -> void
{
    auto& timer = gba.timer[Number];
    timer.counter = timer.reload;

    // these are audio fifo timers
    if constexpr(Number == 0 || Number == 1)
    {
        apu::on_timer_overflow(gba, Number);
    }

    // tick cascade timer when the timer above overflows
    // eg, if timer2 overflows, cascade timer1 would be ticked
    // because of this, timer0 cascade is ignored due to
    // there not being a timer above it!
    if constexpr(Number != 3)
    {
        auto& cascade_timer = gba.timer[Number+1];

        if (cascade_timer.cascade)
        {
            cascade_timer.counter++;
            if (cascade_timer.counter == 0)
            {
                on_overflow<Number+1>(gba);
            }
        }
    }

    // check if we should fire an irq
    if (timer.irq)
    {
        arm7tdmi::fire_interrupt(gba, INTERRUPT[Number]);
    }

    add_timer_event(gba, timer, Number, 0);
}

auto get_tmxcnt(Gba& gba, const u8 num)
{
    switch (num)
    {
        case 0: return REG_TM0CNT;
        case 1: return REG_TM1CNT;
        case 2: return REG_TM2CNT;
        case 3: return REG_TM3CNT;
    }

    std::unreachable();
}

} // namespace

auto on_cnt_write(Gba& gba, const u8 num) -> void
{
    assert(num <= 3 && "invalid timer");

    static constexpr u16 freq_table[4] = {1, 64, 256, 1024};

    const auto cnt = get_tmxcnt(gba, num);
    const auto F = bit::get_range<0, 1>(cnt); // freq
    const auto C = bit::is_set<2>(cnt); // cascade
    const auto I = bit::is_set<6>(cnt); // irq
    const auto E = bit::is_set<7>(cnt); // enable

    auto& timer = gba.timer[num];

    // if timer is enabled, reload
    if (!timer.enable && E)
    {
        timer.counter = timer.reload;
    }

    timer.freq = freq_table[F];
    timer.cascade = C;
    timer.irq = I;
    timer.enable = E;

    if (timer.enable)
    {
        // searching on emudev discord about timers mention that they
        // have a 2 cycle delay on startup (but not on overflow)
        add_timer_event(gba, timer, num, 2);
    }
    else
    {
        scheduler::remove(gba, EVENTS[num]);
    }
}

auto on_timer0_event(Gba& gba) -> void
{
    on_overflow<0>(gba);
}

auto on_timer1_event(Gba& gba) -> void
{
    on_overflow<1>(gba);
}

auto on_timer2_event(Gba& gba) -> void
{
    on_overflow<2>(gba);
}

auto on_timer3_event(Gba& gba) -> void
{
    on_overflow<3>(gba);
}

auto update_timer(Gba& gba, Timer& timer) -> void
{
    if (!timer.enable)
    {
        return;
    }

    const auto delta = (gba.scheduler.cycles + gba.scheduler.elapsed - timer.event_time) / timer.freq;
    // update counter
    timer.counter += delta;
    // update timestamp (needed for Event::Reset)
    timer.event_time += delta * timer.freq;//gba.scheduler.cycles;
}

auto read_timer(Gba& gba, u8 num) -> u16
{
    auto& timer = gba.timer[num];

    if (!timer.enable)
    {
        return timer.reload;
    }
    else if (timer.cascade)
    {
        return timer.counter;
    }
    else
    {
        update_timer(gba, timer);
        return timer.counter;
    }
}

} // namespace gba::timer
