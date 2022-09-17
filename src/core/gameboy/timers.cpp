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

// todo: proper timer impl
// use: https://hacktixme.ga/GBEDG/timers/

inline auto is_timer_enabled(const Gba& gba) -> bool
{
    return bit::is_set<2>(IO_TAC);
}

inline auto check_div_clocks_fs(const Gba& gba, u8 old_div, u8 new_div) -> bool
{
    // either bit 4 or 5 falling edge is checked
    const u8 bit = 1 << (4 + gba.gameboy.cpu.double_speed);

    return (old_div & bit) && !(new_div & bit);
}

// todo: check if div still drives fs on gba!
inline void div_clock_fs(Gba& gba, u8 old_div, u8 new_div)
{
    if (check_div_clocks_fs(gba, old_div, new_div)) [[unlikely]]
    {
        // step_frame_sequencer(gba);
    }
}

} // namespace

void on_timer_event(Gba& gba)
{
    #if USE_SCHED
    if (IO_TIMA == 0xFF) [[unlikely]]
    {
        IO_TIMA = IO_TMA;
        enable_interrupt(gba, INTERRUPT_TIMER);
    }
    else
    {
        IO_TIMA++;
    }

    scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, TAC_FREQ[IO_TAC & 0x03] >> (gba.gameboy.cpu.double_speed));
    #endif
}

void on_div_event(Gba& gba)
{
    #if USE_SCHED
    IO_DIV++;
    scheduler::add(gba, scheduler::Event::TIMER1, on_div_event, 256 >> (gba.gameboy.cpu.double_speed));
    #endif
}

void div_write(Gba& gba, u8 value)
{
    // writes to DIV resets it to zero
    UNUSED(value);

    if (check_div_clocks_fs(gba, IO_DIV, 0))
    {
        gb_log("div write caused early fs tick!\n");
    }

    gb_log("div write!\n");

    div_clock_fs(gba, IO_DIV, 0);

    IO_DIV = IO_DIV_LOWER = 0;
    #if USE_SCHED
    scheduler::add(gba, scheduler::Event::TIMER1, on_div_event, 256 >> (gba.gameboy.cpu.double_speed));
    #endif
}

void tima_write(Gba& gba, u8 value)
{
    IO_TIMA = value;
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

    #if USE_SCHED
    // check if the timer was just enabled
    if (now_enabled && !was_enabled)
    {
        scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, TAC_FREQ[IO_TAC & 0x03] >> (gba.gameboy.cpu.double_speed));
    }
    // check if the timer was just disabled
    else if (!now_enabled && was_enabled)
    {
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
    }
    #endif
}

void timer_run(Gba& gba, u8 cycles)
{
    #if !USE_SCHED
    // DIV is a 16-bit register
    // reads return the upper byte, but the lower byte
    // gets ticked every 4-T cycles (1M)

    // check if we overflow
    if (IO_DIV_LOWER + cycles > 0xFF) [[unlikely]]
    {
        div_clock_fs(gba, IO_DIV, IO_DIV + 1);

        // increase the upper byte
        ++IO_DIV;
    }

    // always add the cycles to lower byte
    // this will wrap round if overflow
    IO_DIV_LOWER = (IO_DIV_LOWER + cycles) & 0xFF;

    if (is_timer_enabled(gba))
    {
        gba.gameboy.timer.next_cycles += cycles;

        while ((gba.gameboy.timer.next_cycles) >= TAC_FREQ[IO_TAC & 0x03]) [[unlikely]]
        {
            gba.gameboy.timer.next_cycles -= TAC_FREQ[IO_TAC & 0x03];

            if (IO_TIMA == 0xFF) [[unlikely]]
            {
                IO_TIMA = IO_TMA;
                enable_interrupt(gba, INTERRUPT_TIMER);
            }
            else
            {
                ++IO_TIMA;
            }
        }
    }
    #endif
}

} // namespace gba::gb
