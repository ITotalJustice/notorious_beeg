#include "bit.hpp"
#include "gb.hpp"
#include "internal.hpp"
// #include "apu/apu.hpp"

#include "gba.hpp"
#include "scheduler.hpp"
#include <cstdio>

namespace gba::gb {
namespace {

constexpr u16 TAC_FREQ[4] = { 1024, 16, 64, 256 };

// tima is actually incremented on the falling edge of certain
// div bits based on the frequency of tac!
constexpr u8 TAC_DIV_FALL_BIT[4] = { 9, 3, 5, 7 };

// todo: proper timer impl
// use: https://hacktixme.ga/GBEDG/timers/

inline auto is_timer_enabled(const Gba& gba) -> bool
{
    return bit::is_set<2>(IO_TAC);
}

} // namespace

void on_timer_reload_event(Gba& gba)
{
    if (gba.gameboy.timer.reloading)
    {
        gba.gameboy.timer.reloading = false;
        IO_TIMA = IO_TMA;
        enable_interrupt(gba, INTERRUPT_TIMER);
    }
}

void on_timer_event(Gba& gba)
{
    if (gba.gameboy.timer.reloading)
    {
        on_timer_reload_event(gba);
        scheduler::remove(gba, scheduler::Event::TIMER2);
    }

    if (IO_TIMA == 0xFF) [[unlikely]]
    {
        IO_TIMA = 0x00;
        gba.gameboy.timer.reloading = true;
        scheduler::remove(gba, scheduler::Event::TIMER2);
        scheduler::add(gba, scheduler::Event::TIMER2, on_timer_reload_event, 4 >> (gba.gameboy.cpu.double_speed));
    }
    else
    {
        IO_TIMA++;
    }

    scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, TAC_FREQ[IO_TAC & 0x03] >> (gba.gameboy.cpu.double_speed));
}

void on_div_event(Gba& gba)
{
    IO_DIV++;
    scheduler::add(gba, scheduler::Event::TIMER1, on_div_event, 256 >> (gba.gameboy.cpu.double_speed));
}

void div_write(Gba& gba, [[maybe_unused]] u8 value)
{
    if (is_timer_enabled(gba))
    {
        const auto event_cycles = gba.scheduler.get_event_cycles(scheduler::Event::TIMER1);
        const u8 diff = 0x100 - (event_cycles - gba.scheduler.cycles);

        const auto bit_to_check = TAC_DIV_FALL_BIT[IO_TAC & 0x3];
        const u16 div_16 = (IO_DIV << 8) | diff;

        // "The timer uses the same internal counter as the DIV register, so resetting DIV also resets the timer."
        // remove event in order to reset delta
        scheduler::remove(gba, scheduler::Event::TIMER0);

        // check if a falling edge will happen (1 -> 0)
        if (bit::is_set(div_16, bit_to_check))
        {
            // tick tima, this will internally schedule an event
            on_timer_event(gba);
        }
        else
        {
            // otherwise, re-schedule event
            scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, TAC_FREQ[IO_TAC & 0x03] >> (gba.gameboy.cpu.double_speed));
        }
    }

    // remove both timers as to reset the delta
    scheduler::remove(gba, scheduler::Event::TIMER1);
    scheduler::add(gba, scheduler::Event::TIMER1, on_div_event, 256 >> (gba.gameboy.cpu.double_speed));

    IO_DIV = 0;
}

void tima_write(Gba& gba, u8 value)
{
    // printf("tima write: 0x%02X tma: 0x%02X gba.gameboy.timer.reloading: %u cycles: %u\n", value, IO_TMA, gba.gameboy.timer.reloading, gba.gameboy.cycles);
    IO_TIMA = value;
    gba.gameboy.timer.reloading = false;
}

void tma_write(Gba& gba, u8 value)
{
    IO_TMA = value;
}

void tac_write(Gba& gba, u8 value)
{
    const auto was_enabled = bit::is_set<2>(IO_TAC);
    const auto old_freq = IO_TAC & 0x03;

    IO_TAC = value;

    const auto now_enabled = bit::is_set<2>(IO_TAC);
    const auto new_freq = IO_TAC & 0x03;

    // check if the timer was just enabled
    if (now_enabled && !was_enabled)
    {
        if (gba.gameboy.timer.elapsed)
        {
            // check if the timer has already expired by this point
            // NOTE: there is a bug(?) here if double speed was changed between
            // enable / disabling of timers.
            if (gba.gameboy.timer.elapsed <= TAC_FREQ[IO_TAC & 0x03])
            {
                // this internally adds the event to the scheduler
                on_timer_event(gba);
            }
            else
            {
                scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, gba.gameboy.timer.elapsed);
            }
        }
        else
        {
            scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, TAC_FREQ[IO_TAC & 0x03] >> (gba.gameboy.cpu.double_speed));
        }
    }
    // check if the timer was just disabled
    else if (!now_enabled && was_enabled)
    {
        // disabled timers keep track of their cuurent position
        // SEE: mts rapid_toggle test
        gba.gameboy.timer.elapsed = (gba.scheduler.get_event_cycles(scheduler::Event::TIMER0) - gba.scheduler.cycles);
        // printf("elapsed: %u\n", gba.gameboy.timer.elapsed);
        // todo: it's possible that the timer could fire here.
        // eg, an instruction taking 8 cycles that does the write to
        // disable timer.
        // in the middle of the instruction, the timer would fire!
        scheduler::remove(gba, scheduler::Event::TIMER0);
    }
    // check if the timer frequency changed
    else if (old_freq != new_freq)
    {
        // does this only apply on reload?
        // todo: investigate this!
        printf("timer freq changed, old: %u new: %u\n", TAC_FREQ[old_freq], TAC_FREQ[new_freq]);
        // assert(!"tima freq changed whilst enabled!");
    }
}

} // namespace gba::gb
