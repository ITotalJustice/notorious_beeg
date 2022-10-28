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

auto read_timer_from_scheduler(Gba& gba, Timer& timer, u8 num) -> u16
{
    const auto delta = (gba.scheduler.get_ticks() - gba.scheduler.get_event_cycles_absolute(EVENTS[num])) / timer.freq;

    // handles rare case where the timer is read after 0/1 cycle(s)
    if (delta < timer.counter - 0x10000) [[unlikely]]
    {
        return timer.counter;
    }

    return delta;
}

auto add_timer_event(Gba& gba, Timer& timer, u8 num, u8 offset) -> void
{
    // don't add timer if cascade is enabled (and not timer0)
    if (num != 0 && timer.cascade)
    {
        return;
    }

    const auto value = ((0x10000 - timer.counter) * timer.freq) + offset;
    gba.scheduler.add(EVENTS[num], gba.delta.get(EVENTS[num], value), on_timer_event, &gba);

    log::print_info(gba, LOG_TYPE[num], "timestamp: %d adding timer[%u] counter: 0x%04X value: 0x%04X delta %d\n", gba.scheduler.get_ticks(), num, timer.counter, value, gba.delta.get(EVENTS[num], 0));
}

auto on_overflow(Gba& gba, u8 num) -> void
{
    auto& timer = gba.timer[num];
    timer.counter = timer.reload;

    log::print_info(gba, LOG_TYPE[num], "timestamp: %d overflow, reloading: 0x%04X\n", gba.scheduler.get_ticks(), timer.counter);

    // these are audio fifo timers
    if (num == 0 || num == 1)
    {
        apu::on_timer_overflow(gba, num);
    }

    // tick cascade timer when the timer above overflows
    // eg, if timer2 overflows, cascade timer1 would be ticked
    // because of this, timer0 cascade is ignored due to
    // there not being a timer above it!
    if (num < 3)
    {
        auto& cascade_timer = gba.timer[num+1];

        if (cascade_timer.enable && cascade_timer.cascade)
        {
            cascade_timer.counter++;
            log::print_info(gba, LOG_TYPE[num+1], "clocking cascade timer: 0x%04X\n", gba.scheduler.get_ticks(), cascade_timer.counter);

            if (cascade_timer.counter == 0)
            {
                on_overflow(gba, num+1);
            }
        }
    }

    // check if we should fire an irq
    if (timer.irq)
    {
        log::print_info(gba, LOG_TYPE[num], "timestamp: %d firing timer irq\n", gba.scheduler.get_ticks());
        arm7tdmi::fire_interrupt(gba, INTERRUPT[num]);
    }

    add_timer_event(gba, timer, num, 0);
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
        log::print_info(gba, LOG_TYPE[num], "timestamp: %d enabling timer\n", gba.scheduler.get_ticks());
        timer.counter = timer.reload;
    }
    else if (was_enabled && !E)
    {
        log::print_info(gba, LOG_TYPE[num], "timestamp: %d disabling timer\n", gba.scheduler.get_ticks());

        if (num == 0 || !timer.cascade)
        {
            timer.counter = read_timer_from_scheduler(gba, timer, num);
        }

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
    u16 result;

    if (!timer.enable)
    {
        result = timer.counter;
        log::print_info(gba, LOG_TYPE[num], "timestamp: %d reading disabled timer: result: 0x%04X vcount: %u ppu_cycles: %d counter: 0x%04X\n", gba.scheduler.get_ticks(), result, REG_VCOUNT, gba.scheduler.get_event_cycles(scheduler::ID::PPU), timer.counter);
    }
    else if (timer.cascade)
    {
        gba.scheduler.fire(); // may break stuff but needed for ags countup
        result = timer.counter;
        log::print_info(gba, LOG_TYPE[num], "timestamp: %d reading cascade timer: result: 0x%04X vcount: %u ppu_cycles: %d counter: 0x%04X\n", gba.scheduler.get_ticks(), result, REG_VCOUNT, gba.scheduler.get_event_cycles(scheduler::ID::PPU), timer.counter);
    }
    else
    {
        result = read_timer_from_scheduler(gba, timer, num);
        log::print_info(gba, LOG_TYPE[num], "timestamp: %d reading normal timer: result: 0x%04X vcount: %u ppu_cycles: %d counter: 0x%04X\n", gba.scheduler.get_ticks(), result, REG_VCOUNT, gba.scheduler.get_event_cycles(scheduler::ID::PPU), timer.counter);
    }

    return result;
}

void write_timer(Gba& gba, u16 value, u8 num)
{
    auto& timer = gba.timer[num];

    timer.reload = value;
    if (!timer.enable)
    {
        timer.counter = timer.reload;
    }
    gba.timer[0].reload = value;
}

} // namespace gba::timer
