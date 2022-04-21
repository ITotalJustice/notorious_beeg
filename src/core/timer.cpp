// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "apu/apu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "timer.hpp"
#include "bit.hpp"
#include "arm7tdmi/arm7tdmi.hpp"

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#Timer%20registers
namespace gba::timer
{

static constexpr arm7tdmi::Interrupt INTERRUPT[4] =
{
    arm7tdmi::Interrupt::Timer0,
    arm7tdmi::Interrupt::Timer1,
    arm7tdmi::Interrupt::Timer2,
    arm7tdmi::Interrupt::Timer3,
};

static constexpr scheduler::Event EVENTS[4] =
{
    scheduler::Event::TIMER0,
    scheduler::Event::TIMER1,
    scheduler::Event::TIMER2,
    scheduler::Event::TIMER3,
};

static constexpr scheduler::callback CALLBACKS[4] =
{
    on_timer0_event,
    on_timer1_event,
    on_timer2_event,
    on_timer3_event,
};

template<u8 Number>
static auto add_timer_event(Gba& gba, Timer& timer, auto offset) -> void
{
    // don't add timer if cascade is enabled (and not timer0)
    if (Number != 0 && timer.cascade)
    {
        return;
    }

    const auto value = (0x10000 - timer.counter) * timer.freq;
    assert(value >= 1);
    assert(timer.counter <= 0xFFFF);
    timer.event_time = gba.scheduler.cycles;
    scheduler::add(gba, EVENTS[Number], CALLBACKS[Number], value + offset);
}

template<u8 Number>
static auto on_overflow(Gba& gba) -> void
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

    add_timer_event<Number>(gba, timer, 0);
}

template<u8 Number>
static auto on_cnt_write(Gba& gba, u16 cnt) -> void
{
    constexpr u16 freq_table[4] = {1, 64, 256, 1024};

    const auto F = bit::get_range<0, 1>(cnt); // freq
    const auto C = bit::is_set<2>(cnt); // cascade
    const auto I = bit::is_set<6>(cnt); // irq
    const auto E = bit::is_set<7>(cnt); // enable

    auto& timer = gba.timer[Number];

    // if timer is enabled, reload
    if (!timer.enable && E)
    {
        #if ENABLE_SCHEDULER == 0
        switch (Number)
        {
            case 0: REG_TM0D = timer.reload; break;
            case 1: REG_TM1D = timer.reload; break;
            case 2: REG_TM2D = timer.reload; break;
            case 3: REG_TM3D = timer.reload; break;
        }
        #endif
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
        add_timer_event<Number>(gba, timer, 2);
    }
    else
    {
        scheduler::remove(gba, EVENTS[Number]);
    }
}

auto on_cnt0_write(Gba& gba, u16 cnt) -> void
{
    on_cnt_write<0>(gba, cnt);
}

auto on_cnt1_write(Gba& gba, u16 cnt) -> void
{
    on_cnt_write<1>(gba, cnt);
}

auto on_cnt2_write(Gba& gba, u16 cnt) -> void
{
    on_cnt_write<2>(gba, cnt);
}

auto on_cnt3_write(Gba& gba, u16 cnt) -> void
{
    on_cnt_write<3>(gba, cnt);
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

    const auto delta = (gba.scheduler.cycles - timer.event_time) / timer.freq;
    // update counter
    timer.counter += delta;
    // update timestamp (needed for Event::Reset)
    timer.event_time += delta * timer.freq;//gba.scheduler.cycles;
}

auto read_timer(Gba& gba, u8 num) -> u16
{
    auto& timer = gba.timer[num];
    #if ENABLE_SCHEDULER == 0
    return timer.counter;
    #else
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
    #endif
}

#if ENABLE_SCHEDULER == 0
// returns true if overflowed
template<u8 Number>
static auto tick(Gba& gba, u16& data, u8 cycles)
{
    auto& timer = gba.timer[Number];

    const auto func = [&]()
    {
        data++;
        if (data == 0)
        {
            if constexpr(Number == 0)
            {
                // std::printf("[timer0] overflow at %u\n", gba.scheduler.cycles);
            }
            data = timer.reload;
            on_overflow<Number>(gba);
        }
    };

    // check if enabled, return early if disabled
    if (!timer.enable)
    {
        return;
    }

    if (timer.cascade)
    {
        return;
    }

    timer.cycles += cycles;

    if (timer.cycles >= timer.freq)
    {
        while (timer.cycles >= timer.freq)
        {
            timer.cycles -= timer.freq;
            func();
        }
        timer.cycles = 0;
    }

    return;
}

auto run(Gba& gba, u8 cycles) -> void
{
    tick<0>(gba, REG_TM0D, cycles);
    tick<1>(gba, REG_TM1D, cycles);
    tick<2>(gba, REG_TM2D, cycles);
    tick<3>(gba, REG_TM3D, cycles);
}
#endif

} // namespace gba::timer
