// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "apu/apu.hpp"
#include "gba.hpp"
#include "scheduler.hpp"
#include "timer.hpp"
#include "bit.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "log.hpp"
#include <utility>

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#Timer%20registers
namespace gba::timer {
namespace {

constexpr log::Type LOG_TYPE[4] =
{
    log::Type::TIMER0,
    log::Type::TIMER1,
    log::Type::TIMER2,
    log::Type::TIMER3,
};

constexpr arm7tdmi::Interrupt INTERRUPT[4] =
{
    arm7tdmi::Interrupt::Timer0,
    arm7tdmi::Interrupt::Timer1,
    arm7tdmi::Interrupt::Timer2,
    arm7tdmi::Interrupt::Timer3,
};

constexpr scheduler::ID EVENTS[4] =
{
    scheduler::ID::TIMER0,
    scheduler::ID::TIMER1,
    scheduler::ID::TIMER2,
    scheduler::ID::TIMER3,
};

constexpr auto get_timer_num_from_event(s32 id) -> u8
{
    switch (id)
    {
        case scheduler::ID::TIMER0: return 0;
        case scheduler::ID::TIMER1: return 1;
        case scheduler::ID::TIMER2: return 2;
        case scheduler::ID::TIMER3: return 3;
    }

    std::unreachable();
}

auto add_timer_event(Gba& gba, Timer& timer, u8 num, u8 offset) -> void
{
    // don't add timer if cascade is enabled (and not timer0)
    if (num != 0 && timer.cascade)
    {
        return;
    }

    const auto value = (0x10000 - timer.counter) * timer.freq;

    assert(value >= 1);
    assert(timer.counter <= 0xFFFF);

    gba.scheduler.add(EVENTS[num], gba.delta.get(EVENTS[num], value + offset), on_timer_event, &gba);
    timer.event_time = gba.scheduler.get_event_cycles(EVENTS[num]);
}

auto on_overflow(Gba& gba, u8 number) -> void
{
    auto& timer = gba.timer[number];
    timer.counter = timer.reload;

    // these are audio fifo timers
    if (number == 0 || number == 1)
    {
        apu::on_timer_overflow(gba, number);
    }

    // tick cascade timer when the timer above overflows
    // eg, if timer2 overflows, cascade timer1 would be ticked
    // because of this, timer0 cascade is ignored due to
    // there not being a timer above it!
    if (number < 3)
    {
        auto& cascade_timer = gba.timer[number+1];

        if (cascade_timer.cascade)
        {
            cascade_timer.counter++;
            if (cascade_timer.counter == 0)
            {
                on_overflow(gba, number+1);
            }
        }
    }

    // check if we should fire an irq
    if (timer.irq)
    {
        arm7tdmi::fire_interrupt(gba, INTERRUPT[number]);
    }

    add_timer_event(gba, timer, number, 0);
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

    const auto was_enabled = timer.enable;

    // can these be updated whilst the timer is enabled?
    timer.freq = freq_table[F];
    timer.cascade = C;
    timer.irq = I;
    timer.enable = E;

    // if timer is now enabled, reload
    if (!was_enabled && E)
    {
        log::print_info(gba, LOG_TYPE[num], "enabling timer\n");
        timer.counter = timer.reload;
    }
    else if (was_enabled && !E)
    {
        log::print_info(gba, LOG_TYPE[num], "disabling timer\n");
        gba.delta.remove(EVENTS[num]);
        gba.scheduler.remove(EVENTS[num]);
        return;
    }

    // searching on emudev discord about timers mention that they
    // have a 2 cycle delay on startup (but not on overflow)
    gba.delta.remove(EVENTS[num]);
    add_timer_event(gba, timer, num, 2);
}

void on_timer_event(void* user, s32 id, s32 late)
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    on_overflow(gba, get_timer_num_from_event(id));
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
        const auto delta = (gba.scheduler.get_ticks() - gba.scheduler.get_event_cycles(EVENTS[num])) / timer.freq;
        return timer.counter + delta;
    }
}

} // namespace gba::timer
