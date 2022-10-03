#include "bit.hpp"
#include "gb.hpp"
#include "internal.hpp"

#include "gba.hpp"
#include "scheduler.hpp"
#include "apu/apu.hpp"

// potential speedups:
// - only tick div on:
//  - div write
//  - div read
//  - tac write
// (often games do not write to div, though they do read)
// (tac writes dont happen that often either)
// - set tima event to fire on overflow (same as gba timer):
//  - cycles = (0x100 - tima) >> double_speed;
//  - will need to catchup on:
//   - tima read
//   - tac disable
//   - tac freq change(?)
// (this can save 255 scheduler fires which is more important)
// (when using very fast freq of 16 as that can fire every instruction!)

namespace gba::gb {
namespace {

// 4096 hz, 262144 hz, 65536 hz, 16384 hz
constexpr u16 TAC_FREQ[4] = { 1024, 16, 64, 256 };

// tima is actually incremented on the falling edge of certain
// div bits based on the frequency of tac!
constexpr u8 TAC_DIV_FALL_BIT[4] = { 9, 3, 5, 7 };

inline auto is_timer_enabled(const Gba& gba) -> bool
{
    return bit::is_set<2>(IO_TAC);
}

inline auto is_div16_bit_set(const Gba& gba, u8 freq_index) -> bool
{
    const auto event_cycles = gba.scheduler.get_event_cycles(scheduler::Event::TIMER1);
    const u8 diff = 0x100 - (event_cycles - gba.scheduler.get_cycles());

    const auto bit_to_check = TAC_DIV_FALL_BIT[freq_index];
    const u16 div_16 = (IO_DIV << 8) | diff;

    return bit::is_set(div_16, bit_to_check);
}

inline auto check_if_div_clocks_fs(Gba& gba, u8 old_div, u8 new_div) -> bool
{
    // either bit 4 or 5 falling edge is checked
    const auto bit = 1 << (4 + gba.gameboy.cpu.double_speed);
    return (old_div & bit) && !(new_div & bit) && apu::is_apu_enabled(gba);
}

} // namespace

void on_timer_reload_event(Gba& gba)
{
    if (gba.gameboy.timer.reloading)
    {
        gba.gameboy.timer.reloading = false;
        IO_TIMA = IO_TMA;
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
        enable_interrupt(gba, INTERRUPT_TIMER); // interrupt isn't delayed, see numism.gb
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
    if (check_if_div_clocks_fs(gba, IO_DIV, IO_DIV + 1))
    {
        apu::on_frame_sequencer_event(gba);
    }

    IO_DIV++;
    scheduler::add(gba, scheduler::Event::TIMER1, on_div_event, 256 >> (gba.gameboy.cpu.double_speed));
}

void div_write(Gba& gba, [[maybe_unused]] u8 value)
{
    if (check_if_div_clocks_fs(gba, IO_DIV, 0))
    {
        apu::on_frame_sequencer_event(gba);
    }

    if (is_timer_enabled(gba))
    {
        // "The timer uses the same internal counter as the DIV register, so resetting DIV also resets the timer."
        // remove event in order to reset delta
        scheduler::remove(gba, scheduler::Event::TIMER0);

        // check if a falling edge will happen (1 -> 0)
        if (is_div16_bit_set(gba, IO_TAC & 0x3))
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
    // if a tima write happens on the same cycle as it being reloaded
    // then the reload takes priority
    if (gba.scheduler.get_cycles() == gba.scheduler.get_event_cycles(scheduler::Event::TIMER2))
    {
        return;
    }

    IO_TIMA = value;
    gba.gameboy.timer.reloading = false;
}

void tma_write(Gba& gba, u8 value)
{
    IO_TMA = value;

    // if a tma write happens on the same cycle as tima being reloaded
    // then the new value of tma gets loaded into tima instead of the old one
    if (gba.scheduler.get_cycles() == gba.scheduler.get_event_cycles(scheduler::Event::TIMER2))
    {
        IO_TIMA = IO_TMA;
    }
}

void tac_write(Gba& gba, u8 value)
{
    const auto was_enabled = bit::is_set<2>(IO_TAC);
    const auto old_freq = bit::get_range<0, 1>(IO_TAC);

    IO_TAC = value;

    const auto now_enabled = bit::is_set<2>(IO_TAC);
    const auto new_freq = bit::get_range<0, 1>(IO_TAC);

    // check if the timer was just enabled
    if (now_enabled && !was_enabled)
    {
        scheduler::add(gba, scheduler::Event::TIMER0, on_timer_event, TAC_FREQ[new_freq] >> (gba.gameboy.cpu.double_speed));
    }
    // check if the timer was just disabled
    else if (!now_enabled && was_enabled)
    {
        // the div16 mask becomes 0 on disable, so if the bit was high
        // it now is falling to zero which causes tima to be ticked.
        // NOTE: i assume it old frequency is used here instead of
        // the new frequency set to tac, need to verify.
        if (is_div16_bit_set(gba, old_freq))
        {
            on_timer_event(gba);
        }

        scheduler::remove(gba, scheduler::Event::TIMER0);
    }
    // check if the timer frequency changed
    else if (now_enabled && old_freq != new_freq)
    {
        const auto was_high = is_div16_bit_set(gba, old_freq);
        const auto now_low = !is_div16_bit_set(gba, new_freq);

        // changing the freq can cause a the bit transition 1->0
        // this is similar to when tima is disabled.
        if (was_high && now_low)
        {
            on_timer_event(gba);
        }
    }
}

} // namespace gba::gb
