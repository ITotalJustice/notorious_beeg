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
    const u8 diff = 0x100 - gba.scheduler.get_event_cycles(scheduler::ID::TIMER1);

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

void on_timer_reload_event(void* user, s32 id, s32 late)
{
    auto& gba = *static_cast<Gba*>(user);
    IO_TIMA = IO_TMA;
}

void on_timer_event(void* user, s32 id, s32 late)
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);

    if (gba.scheduler.has_event(scheduler::ID::TIMER2))
    {
        on_timer_reload_event(user);
        gba.scheduler.remove(scheduler::ID::TIMER2);
    }

    if (IO_TIMA == 0xFF) [[unlikely]]
    {
        IO_TIMA = 0x00;
        enable_interrupt(gba, INTERRUPT_TIMER); // interrupt isn't delayed, see numism.gb
        const auto reload_event_cycles = 4 >> gba.gameboy.cpu.double_speed;
        gba.scheduler.add(scheduler::ID::TIMER2, reload_event_cycles, on_timer_reload_event, &gba);
        gba.gameboy.timer.tima_reload_timestamp = gba.scheduler.get_event_cycles_absolute(scheduler::ID::TIMER2);
    }
    else
    {
        IO_TIMA++;
    }

    gba.scheduler.add(scheduler::ID::TIMER0, gba.delta.get(id, TAC_FREQ[IO_TAC & 0x03] >> gba.gameboy.cpu.double_speed), on_timer_event, &gba);
}

void on_div_event(void* user, s32 id, s32 late)
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);

    if (check_if_div_clocks_fs(gba, IO_DIV, IO_DIV + 1))
    {
        apu::on_frame_sequencer_event(user, 0, 0);
    }

    IO_DIV++;
    gba.scheduler.add(scheduler::ID::TIMER1, gba.delta.get(id, 256 >> gba.gameboy.cpu.double_speed), on_div_event, &gba);
}

void div_write(Gba& gba, [[maybe_unused]] u8 value)
{
    if (check_if_div_clocks_fs(gba, IO_DIV, 0))
    {
        apu::on_frame_sequencer_event(&gba, 0, 0);
    }

    // "The timer uses the same internal counter as the DIV register, so resetting DIV also resets the timer."
    if (is_timer_enabled(gba))
    {
        // check if a falling edge will happen (1 -> 0)
        if (is_div16_bit_set(gba, IO_TAC & 0x3))
        {
            // tick tima, this will internally schedule an event
            on_timer_event(&gba, 0, 0);
        }
        else
        {
            // otherwise, re-schedule event
            gba.scheduler.add(scheduler::ID::TIMER0, TAC_FREQ[IO_TAC & 0x03] >> gba.gameboy.cpu.double_speed, on_timer_event, &gba);
        }
    }

    gba.scheduler.add(scheduler::ID::TIMER1, 256 >> gba.gameboy.cpu.double_speed, on_div_event, &gba);

    IO_DIV = 0;
}

void tima_write(Gba& gba, u8 value)
{
    // if a tima write happens on the same cycle as it being reloaded
    // then the reload takes priority
    if (gba.scheduler.get_ticks() == gba.gameboy.timer.tima_reload_timestamp)
    {
        return;
    }

    IO_TIMA = value;
    gba.scheduler.remove(scheduler::ID::TIMER2);
}

void tma_write(Gba& gba, u8 value)
{
    IO_TMA = value;

    // if a tma write happens on the same cycle as tima being reloaded
    // then the new value of tma gets loaded into tima instead of the old one
    if (gba.scheduler.get_ticks() == gba.gameboy.timer.tima_reload_timestamp)
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
        gba.scheduler.add(scheduler::ID::TIMER0, TAC_FREQ[new_freq] >> gba.gameboy.cpu.double_speed, on_timer_event, &gba);
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
            on_timer_event(&gba, 0, 0);
        }

        gba.scheduler.remove(scheduler::ID::TIMER0);
        gba.delta.remove(scheduler::ID::TIMER0);
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
            on_timer_event(&gba, 0, 0);
        }
    }
}

} // namespace gba::gb
