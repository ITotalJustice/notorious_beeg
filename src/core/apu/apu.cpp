// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "apu.hpp"
#include "gameboy/types.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "dma.hpp"
#include "mem.hpp"
#include "scheduler.hpp"
#include "log.hpp"
#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utility>

// bad code lives here
// so i copy / pasted my gb apu code from my gb emu
// c++-ifyed it used classes and inheritence
// gcc did not virtualise all the functions, even with final
// decided on templates instead, this worked nicely
// but still it is a mix of class / template / legacy c code
#define APU gba.apu

namespace gba::apu {
namespace {

constexpr log::Type LOG_TYPE[4] =
{
    log::Type::SQUARE0,
    log::Type::SQUARE1,
    log::Type::WAVE,
    log::Type::NOISE,
};

constexpr scheduler::ID EVENTS[4] =
{
    scheduler::ID::APU_SQUARE0,
    scheduler::ID::APU_SQUARE1,
    scheduler::ID::APU_WAVE,
    scheduler::ID::APU_NOISE,
};

constexpr scheduler::Callback CALLBACKS[4] =
{
    on_square0_event,
    on_square1_event,
    on_wave_event,
    on_noise_event,
};

constexpr u8 PERIOD_TABLE[8] = { 8, 1, 2, 3, 4, 5, 6, 7 };

constexpr u8 SQUARE_DUTY_CYCLES[4][8] =
{
    /*[0] = */{ 0, 0, 0, 0, 0, 0, 0, 1 },
    /*[1] = */{ 1, 0, 0, 0, 0, 0, 0, 1 },
    /*[2] = */{ 0, 0, 0, 0, 0, 1, 1, 1 },
    /*[3] = */{ 0, 1, 1, 1, 1, 1, 1, 0 },
};

// this is used when a channel is triggered
[[nodiscard]]
constexpr auto is_next_frame_sequencer_step_not_len(const Gba& gba) -> bool
{
    // check if the current counter is the len clock, the next one won't be!
    return APU.frame_sequencer.index & 0x1;
}

// this is used when channels 1,2,4 are triggered
[[nodiscard]]
constexpr auto is_next_frame_sequencer_step_vol(const Gba& gba) -> bool
{
    // check if the current counter is the len clock, the next one won't be!
    return APU.frame_sequencer.index == 7;
}

[[nodiscard]]
auto get_frame_sequencer_cycles(const Gba& gba) -> u16
{
    assert(gba.is_gba() && "gb div clocks fs");
    // return 280896*60/512; // 32917
    return 8192 * 4; // 32768
}

template<typename T> [[nodiscard]]
constexpr auto get_channel_from_type(Gba& gba) -> auto&
{
    if constexpr(std::is_same<T, Square0>())
    {
        return APU.square0;
    }
    if constexpr(std::is_same<T, Square1>())
    {
        return APU.square1;
    }
    if constexpr(std::is_same<T, Wave>())
    {
        return APU.wave;
    }
    if constexpr(std::is_same<T, Noise>())
    {
        return APU.noise;
    }
}

// these should be methods, however
// it bloats up the header file a lot
// i also prefer to keep functions in .cpp files
// that way, fixing a function doesnt cause the whole
// codebase to recompile.
namespace sweep {

auto clock(Gba& gba, auto& channel);
auto trigger(Gba& gba, auto& channel);
auto get_new_freq(auto& channel) -> u16;
auto do_freq_calc(Gba& gba, auto& channel, bool update_value) -> void;
auto update_enabled_flag(auto& channel) -> void;

} // namespace sweep

namespace len {

auto clock(Gba& gba, auto& channel);
auto trigger(Gba& gba, auto& channel, u16 reload);
auto on_nrx4_edge_case_write(Gba& gba, auto& channel, u8 value);

} // namespace len

namespace env {

auto clock(Gba& gba, auto& channel);
auto trigger(Gba& gba, auto& channel);
auto write(Gba& gba, auto& channel, u8 value);

} // namespace env

[[nodiscard]]
auto psg_left_volume(Gba& gba)
{
    return 1 + bit::get_range<0, 2>(REG_SOUNDCNT_L);
}

[[nodiscard]]
auto psg_right_volume(Gba& gba)
{
    return 1 + bit::get_range<4, 6>(REG_SOUNDCNT_L);
}

[[nodiscard]]
auto psg_master_volume(Gba& gba)
{
    static constexpr u8 vols[4] = { 4, 2, 1, 1 };
    const auto index = bit::get_range<0, 1>(REG_SOUNDCNT_H);

    return vols[index];
}

auto sweep::clock(Gba& gba, auto& channel)
{
    auto& sweep = channel.sweep;

    // decrement the counter, reload after
    sweep.timer--;

    if (sweep.timer <= 0)
    {
        // period is counted as 8 if 0...
        sweep.timer = PERIOD_TABLE[sweep.period];

        if (sweep.enabled && sweep.period)
        {
            // first time updates the value
            sweep::do_freq_calc(gba, channel, true);
            // second time does not, but still checks for overflow
            sweep::do_freq_calc(gba, channel, false);
        }
    }
}

auto sweep::trigger(Gba &gba, auto &channel)
{
    auto& sweep = channel.sweep;

    sweep.did_negate = false;

    // reload sweep timer with period
    sweep.timer = PERIOD_TABLE[sweep.period];
    // the freq is loaded into the shadow_freq_reg
    sweep.freq_shadow_register = (channel.freq_msb << 8) | channel.freq_lsb;

    // sweep is enabled flag if period or shift is non zero
    sweep::update_enabled_flag(channel);

    // sweep calc is performed, but the value isn't updated
    // this means that it only really checks if the value overflows
    // if it does, then ch1 is disabled.
    if (sweep.shift)
    {
        sweep::do_freq_calc(gba, channel, false);
    }
}

[[nodiscard]]
auto sweep::get_new_freq(auto& channel) -> u16
{
    auto& sweep = channel.sweep;
    const auto new_freq = sweep.freq_shadow_register >> sweep.shift;

    if (sweep.negate)
    {
        sweep.did_negate = true;
        return sweep.freq_shadow_register - new_freq;
    }
    else
    {
        return sweep.freq_shadow_register + new_freq;
    }
}

auto sweep::do_freq_calc(Gba& gba, auto& channel, const bool update_value) -> void
{
    auto& sweep = channel.sweep;
    const auto new_freq = get_new_freq(channel);

    if (new_freq > 2047)
    {
        channel.disable(gba);
    }
    else if (sweep.shift && update_value)
    {
        sweep.freq_shadow_register = new_freq;
        channel.freq_lsb = new_freq & 0xFF;
        channel.freq_msb = new_freq >> 8;
    }
}

auto sweep::update_enabled_flag(auto& channel) -> void
{
    auto& sweep = channel.sweep;

    // sweep is enabled flag if period or shift is non zero
    sweep.enabled = (sweep.period != 0) || (sweep.shift != 0);
}

auto len::clock(Gba& gba, auto& channel)
{
    auto& len = channel.len;

    if (len.enable && len.counter > 0)
    {
        len.counter--;
        // disable channel if we hit zero...
        if (len.counter == 0)
        {
            channel.disable(gba);
        }
    }
}

auto len::trigger(Gba& gba, auto& channel, const u16 reload)
{
    auto& len = channel.len;

    if (len.counter == 0)
    {
        len.counter = reload;

        if (len.enable && is_next_frame_sequencer_step_not_len(gba))
        {
            len.counter--;
        }
    }
}

auto len::on_nrx4_edge_case_write(Gba& gba, auto& channel, u8 value)
{
    auto& len = channel.len;

    // if next is not len and len is NOW enabled, it is clocked
    if (is_next_frame_sequencer_step_not_len(gba) && len.counter && !len.enable && bit::is_set<6>(value))
    {
        --len.counter;

        log::print_info(gba, LOG_TYPE[channel.num], "edge case: extra len clock!");

        // if this makes the result 0, and trigger is clear, disable channel
        if (!len.counter && !bit::is_set<7>(value))
        {
            channel.disable(gba);
        }
    }
}

auto env::clock([[maybe_unused]] Gba& gba, auto& channel)
{
    auto& env = channel.env;

    if (!env.disable)
    {
        env.timer--;

        if (env.timer <= 0)
        {
            env.timer = PERIOD_TABLE[env.period];

            if (env.period != 0)
            {
                const auto modifier = env.mode == 1 ? +1 : -1;
                const auto new_volume = env.volume + modifier;

                if (new_volume >= 0 && new_volume <= 15)
                {
                    env.volume = new_volume;
                }
                else
                {
                    env.disable = true;
                }
            }
        }
    }
}

auto env::trigger(Gba& gba, auto& channel)
{
    auto& env = channel.env;

    env.disable = false;
    env.timer = PERIOD_TABLE[env.period];
    // if the next frame sequence clocks the vol, then
    // the timer is reloaded + 1.
    if (is_next_frame_sequencer_step_vol(gba))
    {
        env.timer++;
    }

    // reload the volume
    env.volume = env.starting_vol;
}

auto env::write(Gba& gba, auto& channel, u8 value)
{
    auto& env = channel.env;

    const auto starting_vol = bit::get_range<4, 7>(value);
    const auto mode = bit::is_set<3>(value);
    const auto period = bit::get_range<0, 2>(value);

    // todo: not sure if this bug is on gba
    #if 1
    if (channel.is_enabled(gba))
    {
        if (env.period == 0 && !env.disable)
        {
            env.volume += 1;
        }
        else if (!env.mode)
        {
            env.volume += 2;
        }

        if (env.mode != mode)
        {
            env.volume = 16 - env.volume;
        }

        env.volume &= 0xF;
    }
    #endif

    env.starting_vol = starting_vol;
    env.mode = mode;
    env.period = period;

    if (!channel.is_dac_enabled())
    {
        channel.disable(gba);
    }
}

auto clock_len(Gba& gba)
{
    len::clock(gba, APU.square0);
    len::clock(gba, APU.square1);
    len::clock(gba, APU.wave);
    len::clock(gba, APU.noise);
}

auto clock_sweep(Gba& gba)
{
    sweep::clock(gba, APU.square0);
}

auto clock_env(Gba& gba)
{
    env::clock(gba, APU.square0);
    env::clock(gba, APU.square1);
    env::clock(gba, APU.noise);
}

auto apu_on_enabled(Gba& gba) -> void
{
    log::print_info(gba, log::Type::FRAME_SEQUENCER, "enabling...\n");

    // todo: reset more regs
    APU.square0.duty = 0;
    APU.square1.duty = 0;
    APU.square0.duty_index = 0;
    APU.square1.duty_index = 0;
    APU.frame_sequencer.index = 0;

    // only add back scheduler event.
    // channels are only re-enabled on trigger
    if (gba.is_gba())
    {
        gba.scheduler.add(scheduler::ID::APU_FRAME_SEQUENCER, get_frame_sequencer_cycles(gba), on_frame_sequencer_event, &gba);
    }
}

auto apu_on_disabled(Gba& gba) -> void
{
    log::print_info(gba, log::Type::FRAME_SEQUENCER, "disabling...\n");

    REG_SOUND1CNT_L = 0;
    REG_SOUND1CNT_H = 0;
    REG_SOUND1CNT_X = 0;
    REG_SOUND2CNT_L = 0;
    REG_SOUND2CNT_H = 0;
    REG_SOUND3CNT_L = 0;
    REG_SOUND3CNT_H = 0;
    REG_SOUND3CNT_X = 0;
    REG_SOUND4CNT_L = 0;
    REG_SOUND4CNT_H = 0;
    REG_SOUNDCNT_L = 0;
    REG_SOUNDCNT_H = 0;
    gba.apu.square0.disable(gba);
    gba.apu.square1.disable(gba);
    gba.apu.wave.disable(gba);
    gba.apu.noise.disable(gba);

    REG_FIFO_A_L = 0;
    REG_FIFO_A_H = 0;
    REG_FIFO_B_L = 0;
    REG_FIFO_B_H = 0;

    gba.apu.square0 = {};
    gba.apu.square1 = {};
    // gba.apu.wave = {};
    gba.apu.noise = {};

    // these events are no longer ticked
    gba.delta.remove(scheduler::ID::APU_SQUARE0);
    gba.scheduler.remove(scheduler::ID::APU_SQUARE0);
    gba.delta.remove(scheduler::ID::APU_SQUARE1);
    gba.scheduler.remove(scheduler::ID::APU_SQUARE1);
    gba.delta.remove(scheduler::ID::APU_WAVE);
    gba.scheduler.remove(scheduler::ID::APU_WAVE);
    gba.delta.remove(scheduler::ID::APU_NOISE);
    gba.scheduler.remove(scheduler::ID::APU_NOISE);
    gba.delta.remove(scheduler::ID::APU_FRAME_SEQUENCER);
    gba.scheduler.remove(scheduler::ID::APU_FRAME_SEQUENCER);
}

template<typename T>
auto clock2(Gba& gba, T& channel)
{
    if constexpr(std::is_same<T, Wave>())
    {
        channel.advance_position_counter(gba);
    }
    else if constexpr(std::is_same<T, Noise>())
    {
        if (channel.clock_shift != 14 && channel.clock_shift != 15)
        {
            channel.clock_lfsr(gba);
        }
    }
    else if constexpr(std::is_same<T, Square0>() || std::is_same<T, Square1>())
    {
        channel.duty_index = (channel.duty_index + 1) % 8;
    }
}

template<typename T>
auto on_channel_event(Gba& gba) -> void
{
    auto& channel = get_channel_from_type<T>(gba);
    const auto freq = channel.get_freq(gba);

    // set to 1 to enable skipping of very high freq
    #if 0
    if constexpr(std::is_same<T, Square0>() || std::is_same<T, Square1>())
    {
        if (freq >= 8)
        {
            clock2<T>(gba, channel);
        }
    }
    else
    #endif
    {
        clock2<T>(gba, channel);
    }

    if (freq > 0)
    {
        gba.scheduler.add(EVENTS[channel.num], gba.delta.get(EVENTS[channel.num], freq), CALLBACKS[channel.num], &gba);
    }
}

template<typename T>
auto trigger(Gba& gba, T& channel)
{
    channel.enable(gba);

    static constexpr u16 len_reload[4] = { 64, 64, 256, 64 };
    len::trigger(gba, channel, len_reload[channel.num]);

    if constexpr(std::is_same_v<T, Wave>)
    {
        // reset position counter
        channel.position_counter = 0;
        channel.timer = channel.get_freq(gba) + (3 * 2);
    }
    else
    {
        env::trigger(gba, channel);

        if constexpr(std::is_same_v<T, Noise>)
        {
            // set all bits of the lfsr to 1
            channel.lfsr = 0x7FFF;
            channel.timer = channel.get_freq(gba);
        }
        else // square0 | square1
        {
            // when a square channel is triggered, it's lower 2-bits are not modified!
            // SOURCE: https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
            channel.timer = (channel.timer & 0x3) | (channel.get_freq(gba) & ~0x3);

            if constexpr(std::is_same_v<T, Square0>)
            {
                sweep::trigger(gba, channel);
            }
        }
    }

    if (!channel.is_dac_enabled()) [[unlikely]]
    {
        channel.disable(gba);
    }

    if (channel.is_enabled(gba) && channel.timer > 0) [[likely]]
    {
        gba.scheduler.add(EVENTS[channel.num], channel.timer, CALLBACKS[channel.num], &gba);
    }
}

template<typename T>
auto on_nrx0_write(Gba& gba, T& channel, u8 value)
{
    log::print_info(gba, LOG_TYPE[channel.num], "NR%u0: 0x%02X\n", channel.num, value);

    if constexpr(std::is_same<T, Square0>())
    {
        const bool sweep_negate = bit::is_set<3>(value);

        // if at least 1 sweep negate has happened since last trigger,
        // and negate is now cleared, then disable ch1.
        if (channel.sweep.negate && !sweep_negate && channel.sweep.did_negate)
        {
            log::print_info(gba, LOG_TYPE[channel.num], "NRX0 sweep negate cleared, disabling channel!\n");
            channel.disable(gba);
        }

        channel.sweep.period = bit::get_range<4, 6>(value);
        channel.sweep.negate = sweep_negate;
        channel.sweep.shift = bit::get_range<0, 2>(value);
    }
    else if constexpr(std::is_same<T, Wave>())
    {
        if (gba.is_gba())
        {
            channel.bank_mode = bit::is_set<5>(value);
            channel.bank_select = bit::is_set<6>(value);
        }

        channel.dac_power = bit::is_set<7>(value);

        if (!channel.is_dac_enabled())
        {
            channel.disable(gba);
        }
    }
}

template<typename T>
auto on_nrx1_write([[maybe_unused]] Gba& gba, T& channel, u8 value)
{
    log::print_info(gba, LOG_TYPE[channel.num], "NR%u1: 0x%02X\n", channel.num, value);

    if constexpr(std::is_same<T, Wave>())
    {
        channel.len.counter = 256 - value;
    }
    else
    {
        channel.len.counter = 64 - bit::get_range<0, 5>(value);

        if constexpr(std::is_same<T, Square0>() || std::is_same<T, Square1>())
        {
            channel.duty = bit::get_range<6, 7>(value);
        }
    }
}

template<typename T>
auto on_nrx2_write(Gba& gba, T& channel, u8 value)
{
    log::print_info(gba, LOG_TYPE[channel.num], "NR%u2: 0x%02X\n", channel.num, value);

    if constexpr(std::is_same<T, Wave>())
    {
        channel.vol_code = bit::get_range<5, 6>(value);

        if (gba.is_gba())
        {
            channel.force_volume = bit::is_set<7>(value);
        }
    }
    else
    {
        env::write(gba, channel, value);
    }
}

template<typename T>
auto on_nrx3_write([[maybe_unused]] Gba& gba, T& channel, u8 value)
{
    log::print_info(gba, LOG_TYPE[channel.num], "NR%u3: 0x%02X\n", channel.num, value);

    if constexpr(std::is_same<T, Noise>())
    {
        channel.clock_shift = bit::get_range<4, 7>(value);
        channel.half_width_mode = bit::is_set<3>(value);
        channel.divisor_code = bit::get_range<0, 2>(value);
    }
    else
    {
        channel.freq_lsb = value;
    }
}

template<typename T>
auto on_nrx4_write(Gba& gba, T& channel, u8 value)
{
    log::print_info(gba, LOG_TYPE[channel.num], "NR%u4: 0x%02X\n", channel.num, value);

    len::on_nrx4_edge_case_write(gba, channel, value);

    if constexpr(!std::is_same<T, Noise>())
    {
        channel.freq_msb = bit::get_range<0, 2>(value);
    }

    channel.len.enable = bit::is_set<6>(value);

    if (bit::is_set<7>(value))
    {
        trigger(gba, channel);
    }
}

auto on_nr52_write(Gba& gba, u8 value)
{
    const auto master_enable = bit::is_set<7>(value);

    if (APU.enabled)
    {
        if (!master_enable)
        {
            gba_log("\tapu disabled\n");
            apu_on_disabled(gba);
        }
    }
    else
    {
        if (master_enable)
        {
            gba_log("\tapu enabled\n");
            apu_on_enabled(gba);
        }
    }

    APU.enabled = master_enable;
    REG_SOUNDCNT_X = bit::set<7>(REG_SOUNDCNT_X, master_enable);
}

auto on_wave_mem_write(Gba& gba, u8 addr, u8 value)
{
    log::print_info(gba, log::Type::WAVE, "ram write: 0x%02X value: 0x%02X\n", addr, value);

    if (gba.is_gb())
    {
        if (APU.wave.is_enabled(gba))
        {
            APU.wave.ram[APU.wave.position_counter >> 1] = value;
        }
        else
        {
            const auto wave_addr = addr &= 0xF;
            APU.wave.ram[wave_addr] = value;
        }
        return;
    }

    // treated as 2 banks
    if (APU.wave.bank_mode == 0)
    {
        const auto wave_addr = addr &= 0xF;

        // writes happen to the opposite bank
        const auto bank_select = APU.wave.bank_select ^ 1;
        const auto offset = bank_select ? 0 : 16;
        APU.wave.ram[wave_addr + offset] = value;
    }
    else
    {
        // todo: not sure if this is correct.
        // im copying the behviour of the cgb apu
        if (APU.wave.is_enabled(gba))
        {
            APU.wave.ram[APU.wave.position_counter >> 1] = value;
        }
        else
        {
            const auto wave_addr = addr &= 0x1F;
            APU.wave.ram[wave_addr] = value;
        }
    }
}

} // namespace

auto is_apu_enabled(Gba& gba) -> bool
{
    assert(APU.enabled == bit::is_set<7>(REG_SOUNDCNT_X) && "apu enabled missmatch");
    return APU.enabled;
}

template<u8 Number>
auto Base<Number>::enable(Gba& gba) -> void
{
    if (!is_enabled(gba))
    {
        log::print_info(gba, LOG_TYPE[Number], "enabling channel\n");
    }
    REG_SOUNDCNT_X = bit::set<num>(REG_SOUNDCNT_X);
}

template<u8 Number>
auto Base<Number>::disable(Gba& gba) -> void
{
    if (is_enabled(gba))
    {
        log::print_info(gba, LOG_TYPE[Number], "disabling channel\n");
    }
    REG_SOUNDCNT_X = bit::unset<num>(REG_SOUNDCNT_X);
    gba.delta.remove(EVENTS[Number]);
    gba.scheduler.remove(EVENTS[Number]);
}

template<u8 Number>
auto Base<Number>::is_enabled(Gba& gba) const -> bool
{
    return bit::is_set<num>(REG_SOUNDCNT_X);
}

template<u8 Number>
auto Base<Number>::left_enabled(Gba& gba) const -> bool
{
    return bit::is_set<8 + num>(REG_SOUNDCNT_L);
}

template<u8 Number>
auto Base<Number>::right_enabled(Gba& gba) const -> bool
{
    return bit::is_set<12 + num>(REG_SOUNDCNT_L);
}

auto Fifo::sample() const -> s8
{
    constexpr u8 VOL_SHIFT[2] = { 1, 0 };
    return this->current_sample >> VOL_SHIFT[this->volume_code];
}

auto Fifo::reset() -> void
{
    this->count = 0;
    this->r_index = 0;
    this->w_index = 0;
}

auto Fifo::size() const -> u8
{
    return count;
}

#if 0
auto Fifo::push(u8 value) -> void
{
    buf[w_index] = value;
    w_index = (w_index + 1) % capacity;

    if (count < capacity)
    {
        count++;
    }
}
#else
auto Fifo::push(u8 value) -> void
{
    if (count < capacity)
    {
        buf[w_index] = value;
        w_index = (w_index + 1) % capacity;
        count++;
    }
    // else
    {
        // printf("fifo overlow\n");
        // assert(0);
    }
}
#endif

auto Fifo::pop() -> s8
{
    const auto v = buf[r_index];

    if (count > 0)
    {
        r_index = (r_index + 1) % capacity;
        count--;
    }
    else
    {
        // assert(0);
    }

    return v;
}

auto Fifo::update_current_sample(Gba& gba, u8 num) -> void
{
    current_sample = pop();

    if (size() <= 16)
    {
        dma::on_fifo_empty(gba, num);
    }
}

auto on_fifo_write8(Gba& gba, u8 value, u8 num) -> void
{
    APU.fifo[num].push(value);
}

auto on_fifo_write16(Gba& gba, u16 value, u8 num) -> void
{
    on_fifo_write8(gba, static_cast<u8>(value >> 0x00), num);
    on_fifo_write8(gba, static_cast<u8>(value >> 0x08), num);
}

auto on_fifo_write32(Gba& gba, u32 value, u8 num) -> void
{
    on_fifo_write8(gba, static_cast<u8>(value >> 0x00), num);
    on_fifo_write8(gba, static_cast<u8>(value >> 0x08), num);
    on_fifo_write8(gba, static_cast<u8>(value >> 0x10), num);
    on_fifo_write8(gba, static_cast<u8>(value >> 0x18), num);
}

auto on_timer_overflow(Gba& gba, u8 timer_num) -> void
{
    assert(timer_num == 0 || timer_num == 1);

    for (auto i = 0; i < 2; i++)
    {
        if (APU.fifo[i].timer_select == static_cast<bool>(timer_num))
        {
            APU.fifo[i].update_current_sample(gba, i);
        }
    }
}

auto on_soundcnt_write(Gba& gba) -> void
{
    APU.fifo[0].volume_code = bit::is_set<2>(REG_SOUNDCNT_H);
    APU.fifo[0].enable_right = bit::is_set<8>(REG_SOUNDCNT_H);
    APU.fifo[0].enable_left = bit::is_set<9>(REG_SOUNDCNT_H);
    APU.fifo[0].timer_select = bit::is_set<10>(REG_SOUNDCNT_H);

    APU.fifo[1].volume_code = bit::is_set<3>(REG_SOUNDCNT_H);
    APU.fifo[1].enable_right = bit::is_set<12>(REG_SOUNDCNT_H);
    APU.fifo[1].enable_left = bit::is_set<13>(REG_SOUNDCNT_H);
    APU.fifo[1].timer_select = bit::is_set<14>(REG_SOUNDCNT_H);

    if (bit::is_set<11>(REG_SOUNDCNT_H))
    {
        APU.fifo[0].reset();
    }
    if (bit::is_set<15>(REG_SOUNDCNT_H))
    {
        APU.fifo[1].reset();
    }
}

template<u8 Number>
auto SquareBase<Number>::sample(Gba& gba) const -> u8
{
    const auto dcycle = SQUARE_DUTY_CYCLES[duty][duty_index];
    return env.volume * dcycle * this->is_enabled(gba);
}

template<u8 Number>
auto SquareBase<Number>::get_freq(Gba& gba) const -> u32
{
    const auto mult = gba.is_gba() ? 16 : 4;
    return (2048 - ((static_cast<u32>(freq_msb) << 8) | static_cast<u32>(freq_lsb))) * mult;
}

template<u8 Number>
auto SquareBase<Number>::is_dac_enabled() const -> bool
{
    return env.starting_vol != 0 || env.mode != 0;
}

auto on_square0_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    on_channel_event<Square0>(gba);
}

auto on_square1_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    on_channel_event<Square1>(gba);
}

auto on_wave_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    on_channel_event<Wave>(gba);
}

auto on_noise_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    on_channel_event<Noise>(gba);
}

auto Wave::sample(Gba& gba) const -> u8
{
    if (!this->is_enabled(gba))
    {
        return 0;
    }

    // check if we need the access the low of high nibble
    return (position_counter & 1) ? sample_buffer & 0xF : sample_buffer >> 4;
}

auto Wave::get_freq(Gba& gba) const -> u32
{
    const auto mult = gba.is_gba() ? 8 : 2;
    return (2048 - ((static_cast<u32>(freq_msb) << 8) | static_cast<u32>(freq_lsb))) * mult;
}

auto Wave::is_dac_enabled() const -> bool
{
    return dac_power;
}

auto Wave::get_volume_divider(Gba& gba) const -> float
{
    // if = 0, use the above table
    if (!force_volume || gba.is_gb())
    {
        switch (vol_code)
        {
            case 0b001: // 100%
                return  1.0;

            case 0b010: // 50%
                return 0.5;

            case 0b011: // 25%
                return  0.25;

            case 0b000: // 0%
                return 0.0;

            default:
                std::unreachable();
        }
    }
    else
    {
        return 0.75;
    }
}

auto Noise::sample(Gba& gba) const -> u8
{
    // docs say that it's bit-0 INVERTED
    const auto bit = !(lfsr & 0x1);
    return env.volume * bit * this->is_enabled(gba);
}

auto Noise::get_freq(Gba& gba) const -> u32
{
    // indexed using the noise divisor code
    static constexpr u32 NOISE_DIVISOR[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

    const auto mult = gba.is_gba() ? 4 : 1;
    return (NOISE_DIVISOR[divisor_code] << clock_shift) * mult;
}

auto Noise::is_dac_enabled() const -> bool
{
    return env.starting_vol != 0 || env.mode != 0;
}


auto write_NR10(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND1CNT_L = value;
        on_nrx0_write(gba, APU.square0, value);
    }
}

auto write_NR11(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND1CNT_H &= 0xFF00;
        REG_SOUND1CNT_H |= value;
        on_nrx1_write(gba, APU.square0, value);
    }
}

auto write_NR12(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND1CNT_H &= 0xFF;
        REG_SOUND1CNT_H |= value << 8;
        on_nrx2_write(gba, APU.square0, value);
    }
}

auto write_NR13(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND1CNT_X &= 0xFF00;
        REG_SOUND1CNT_X |= value;
        on_nrx3_write(gba, APU.square0, value);
    }
}

auto write_NR14(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND1CNT_X &= 0xFF;
        REG_SOUND1CNT_X |= value << 8;
        on_nrx4_write(gba, APU.square0, value);
    }
}

auto write_NR21(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND2CNT_L &= 0xFF00;
        REG_SOUND2CNT_L |= value;
        on_nrx1_write(gba, APU.square1, value);
    }
}

auto write_NR22(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND2CNT_L &= 0xFF;
        REG_SOUND2CNT_L |= value << 8;
        on_nrx2_write(gba, APU.square1, value);
    }
}

auto write_NR23(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND2CNT_H &= 0xFF00;
        REG_SOUND2CNT_H |= value;
        on_nrx3_write(gba, APU.square1, value);
    }
}

auto write_NR24(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND2CNT_H &= 0xFF;
        REG_SOUND2CNT_H |= value << 8;
        on_nrx4_write(gba, APU.square1, value);
    }
}

auto write_NR30(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND3CNT_L = value;
        on_nrx0_write(gba, APU.wave, value);
    }
}

auto write_NR31(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND3CNT_H &= 0xFF00;
        REG_SOUND3CNT_H |= value;
        on_nrx1_write(gba, APU.wave, value);
    }
}

auto write_NR32(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND3CNT_H &= 0xFF;
        REG_SOUND3CNT_H |= value << 8;
        on_nrx2_write(gba, APU.wave, value);
    }
}

auto write_NR33(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND3CNT_X &= 0xFF00;
        REG_SOUND3CNT_X |= value;
        on_nrx3_write(gba, APU.wave, value);
    }
}

auto write_NR34(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND3CNT_X &= 0xFF;
        REG_SOUND3CNT_X |= value << 8;
        on_nrx4_write(gba, APU.wave, value);
    }
}

auto write_NR41(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND4CNT_L &= 0xFF00;
        REG_SOUND4CNT_L |= value;
        on_nrx1_write(gba, APU.noise, value);
    }
}

auto write_NR42(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND4CNT_L &= 0xFF;
        REG_SOUND4CNT_L |= value << 8;
        on_nrx2_write(gba, APU.noise, value);
    }
}

auto write_NR43(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND4CNT_H &= 0xFF00;
        REG_SOUND4CNT_H |= value;
        on_nrx3_write(gba, APU.noise, value);
    }
}

auto write_NR44(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUND4CNT_H &= 0xFF;
        REG_SOUND4CNT_H |= value << 8;
        on_nrx4_write(gba, APU.noise, value);
    }
}

auto write_NR50(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUNDCNT_L &= 0xFF00;
        REG_SOUNDCNT_L |= value;
    }
}

auto write_NR51(Gba& gba, u8 value) -> void
{
    if (is_apu_enabled(gba)) {
        REG_SOUNDCNT_L &= 0xFF;
        REG_SOUNDCNT_L |= value << 8;
    }
}

auto write_NR52(Gba& gba, u8 value) -> void
{
    on_nr52_write(gba, value);
}

auto write_WAVE(Gba& gba, u8 addr, u8 value) -> void
{
    on_wave_mem_write(gba, addr, value);
}

auto read_WAVE(Gba& gba, u8 addr) -> u8
{
    log::print_info(gba, log::Type::WAVE, "ram read: 0x%02X\n", addr);

    if (gba.is_gb())
    {
        if (gba.apu.wave.is_enabled(gba))
        {
            return gba.apu.wave.ram[gba.apu.wave.position_counter >> 1];
        }
        else
        {
            const auto wave_addr = addr &= 0xF;
            return gba.apu.wave.ram[wave_addr];
        }
    }
    else
    {
        log::print_warn(gba, log::Type::WAVE, "wave reads not properly implemented!\n");

        if (APU.wave.bank_mode == 0)
        {
            const auto wave_addr = addr &= 0xF;

            // writes happen to the opposite bank
            const auto bank_select = APU.wave.bank_select ^ 1;
            const auto offset = bank_select ? 0 : 16;
            return APU.wave.ram[wave_addr + offset];
        }
        else
        {
            // todo: not sure if this is correct.
            // im copying the behviour of the cgb apu
            if (APU.wave.is_enabled(gba))
            {
                return APU.wave.ram[APU.wave.position_counter >> 1];
            }
            else
            {
                const auto wave_addr = addr &= 0x1F;
                return APU.wave.ram[wave_addr];
            }
        }
    }
}

auto reset(Gba& gba, bool skip_bios) -> void
{
    gba.apu = {};

    // on init, all bits of the lfsr are set
    APU.noise.lfsr = 0x7FFF;

    // todo: reset all apu regs properly if skipping bios
    APU.fifo[0].reset();
    APU.fifo[1].reset();

    if (gba.sample_rate)
    {
        if (gba.is_gb())
        {
            gba.sample_rate_calculated = gb::CPU_CYCLES / gba.sample_rate;
        }
        else
        {
            gba.sample_rate_calculated = 280896 * 60 / gba.sample_rate;
        }
    }

    if (gba.audio_callback && !gba.sample_data.empty() && gba.sample_rate_calculated)
    {
        gba.scheduler.add(scheduler::ID::APU_SAMPLE, gba.sample_rate_calculated, on_sample_event, &gba);
    }
    else
    {
        gba.scheduler.remove(scheduler::ID::APU_SAMPLE);
    }

    if (gba.is_gb())
    {
        gba.apu.wave.bank_mode = false;
    }

    if (skip_bios)
    {
        REG_SOUNDCNT_H = 0x880E;
        REG_SOUNDBIAS = 0x200; // by default bias is set to 512 and resample mode is 0
    }
}

// this is clocked by DIV
auto FrameSequencer::clock(Gba& gba) -> void
{
    // return;
    assert(is_apu_enabled(gba) && "clocking fs when apu is disabled");

    switch (this->index)
    {
        case 0: // len
        case 4:
            clock_len(gba);
            break;

        case 2: // len, sweep
        case 6:
            clock_len(gba);
            clock_sweep(gba);
            break;

        case 7: // vol
            clock_env(gba);
            break;
    }

    this->index = (this->index + 1) % 8;
}

auto Noise::clock_lfsr([[maybe_unused]] Gba& gba) -> void
{
    const auto bit0 = bit::is_set<0>(this->lfsr);
    const auto bit1 = bit::is_set<1>(this->lfsr);
    const auto result = bit1 ^ bit0;

    // now we shift the lfsr BEFORE setting the value!
    this->lfsr >>= 1;

    // NOTE: i am not 100% sure if this is correct
    if (result)
    {
        if (this->half_width_mode)
        {
            this->lfsr = bit::set<6>(this->lfsr); // sets 0x40
        }
        else
        {
            this->lfsr = bit::set<14>(this->lfsr); // sets 0x4000
        }
    }

    assert(this->lfsr != 0 && "noise lfsr should never be zero!");
}

void Wave::advance_position_counter([[maybe_unused]] Gba& gba)
{
    if (gba.is_gb())
    {
        this->position_counter = (this->position_counter + 1) % 32;
        this->sample_buffer = this->ram[this->position_counter >> 1];
        return;
    }

    if (this->bank_mode == 0)
    {
        const auto offset = this->bank_select ? 0 : 16;
        this->position_counter = (this->position_counter + 1) % 32;
        this->sample_buffer = this->ram[offset + (this->position_counter >> 1)];
    }
    else
    {
        this->position_counter = (this->position_counter + 1) % 64;
        this->sample_buffer = this->ram[this->position_counter >> 1];
    }
}

static auto push_sample(Gba& gba, s16 left, s16 right)
{
    gba.sample_data[gba.sample_count++] = left;
    gba.sample_data[gba.sample_count++] = right;

    if (gba.sample_count >= gba.sample_data.size())
    {
        gba.audio_callback(gba.userdata);
        gba.sample_count = 0;
    }
}

static constexpr auto scale_psg_u8_to_s16(u8 input) -> s16
{
    // -32768, +32767
    input *= 1.06;
    return bit::scale<7, 16, s16>(input) ^ 0x8000;
}

static void lowpass(Gba& gba, auto& sample_left, auto& sample_right, float d = 0.5, int filter_level = 1)
{
    static s16 sample[3][2]{};
    s16 output_left;
    s16 output_right;

    if (filter_level == 1)
    {
        output_left = (d*sample[0][0]) + ((1-d)*sample_left);
        output_right = (d*sample[0][1]) + ((1-d)*sample_right);

        sample_left = sample[0][0] = output_left;
        sample_right = sample[0][1] = output_right;
    }
    else if (filter_level == 2)
    {
        const auto e = 2*d;
        const auto f = -d*d;

        output_left = (e*sample[0][0]) + (f*sample[1][0]) + ((1-e-f)*sample_left);
        output_right = (e*sample[0][1]) + (f*sample[1][1]) + ((1-e-f)*sample_right);

        sample_left = output_left;
        sample_right = output_right;

        sample[1][0] = sample[0][0];
        sample[0][0] = output_left;

        sample[1][1] = sample[0][1];
        sample[0][1] = output_right;
    }
    else if (filter_level == 3)
    {
        const auto e = 3*d;
        const auto f = -3*d*d;
        const auto g = d*d*d;

        output_left = (e*sample[0][0]) + (f*sample[1][0]) + ((g*sample[2][0])) + ((1-e-f-g)*sample_left);
        output_right = (e*sample[0][1]) + (f*sample[1][1]) + ((g*sample[2][1])) + ((1-e-f-g)*sample_right);

        sample_left = output_left;
        sample_right = output_right;

        sample[2][0] = sample[1][0];
        sample[1][0] = sample[0][0];
        sample[0][0] = output_left;

        sample[2][1] = sample[1][1];
        sample[1][1] = sample[0][1];
        sample[0][1] = output_right;
    }
}

#if 1
static auto sample_gb(Gba& gba)
{
    s32 sample_left = 0;
    s32 sample_right = 0;

    const auto left_volume = psg_left_volume(gba);
    const auto right_volume = psg_right_volume(gba);
    const auto wave_volume_divider = APU.wave.get_volume_divider(gba);

    // sample channels and upscale to 16-bit
    sample_left += scale_psg_u8_to_s16(APU.square0.sample(gba) * APU.square0.left_enabled(gba) * left_volume);
    sample_left += scale_psg_u8_to_s16(APU.square1.sample(gba) * APU.square1.left_enabled(gba) * left_volume);
    sample_left += scale_psg_u8_to_s16(APU.wave.sample(gba) * APU.wave.left_enabled(gba) * left_volume) * wave_volume_divider;
    sample_left += scale_psg_u8_to_s16(APU.noise.sample(gba) * APU.noise.left_enabled(gba) * left_volume);

    sample_right += scale_psg_u8_to_s16(APU.square0.sample(gba) * APU.square0.right_enabled(gba) * right_volume);
    sample_right += scale_psg_u8_to_s16(APU.square1.sample(gba) * APU.square1.right_enabled(gba) * right_volume);
    sample_right += scale_psg_u8_to_s16(APU.wave.sample(gba) * APU.wave.right_enabled(gba) * right_volume) * wave_volume_divider;
    sample_right += scale_psg_u8_to_s16(APU.noise.sample(gba) * APU.noise.right_enabled(gba) * right_volume);

    sample_left /= 4;
    sample_right /= 4;

    lowpass(gba, sample_left, sample_right, 0.3, 3);
    push_sample(gba, sample_left, sample_right);
}
#else
static auto sample_gb(Gba& gba)
{
    s16 sample_left = 0;
    s16 sample_right = 0;

    const auto square0_sample = APU.square0.sample(gba);
    const auto square1_sample = APU.square1.sample(gba);
    const auto wave_sample = APU.wave.sample(gba);
    const auto noise_sample = APU.noise.sample(gba);

    // sample channels and upscale to 16-bit
    sample_left += square0_sample * APU.square0.left_enabled(gba);
    sample_left += square1_sample * APU.square1.left_enabled(gba);
    sample_left += wave_sample * APU.wave.left_enabled(gba);
    sample_left += noise_sample * APU.noise.left_enabled(gba);

    sample_right += square0_sample * APU.square0.right_enabled(gba);
    sample_right += square1_sample * APU.square1.right_enabled(gba);
    sample_right += wave_sample * APU.wave.right_enabled(gba);
    sample_right += noise_sample * APU.noise.right_enabled(gba);

    sample_left *= psg_left_volume(gba);
    sample_right *= psg_right_volume(gba);

    sample_left *= 1.06;
    sample_right *= 1.06;

    sample_left = bit::scale<10, 16, s16>(sample_left);
    sample_right = bit::scale<10, 16, s16>(sample_right);

    lowpass(gba, sample_left, sample_right, 0.3, 3);
    push_sample(gba, sample_left, sample_right);
}
#endif

static auto sample_gba(Gba& gba)
{
    #define USE_SCALE 1

    s16 sample_left = 0;
    s16 sample_right = 0;

    const u8 square0_sample = APU.square0.sample(gba);
    const u8 square1_sample = APU.square1.sample(gba);
    const u8 wave_sample = APU.wave.sample(gba);
    const u8 noise_sample = APU.noise.sample(gba);

    sample_left += square0_sample * APU.square0.left_enabled(gba);
    sample_left += square1_sample * APU.square1.left_enabled(gba);
    sample_left += wave_sample * APU.wave.left_enabled(gba) * APU.wave.get_volume_divider(gba);
    sample_left += noise_sample * APU.noise.left_enabled(gba);

    sample_right += square0_sample * APU.square0.right_enabled(gba);
    sample_right += square1_sample * APU.square1.right_enabled(gba);
    sample_right += wave_sample * APU.wave.right_enabled(gba) * APU.wave.get_volume_divider(gba);
    sample_right += noise_sample * APU.noise.right_enabled(gba);

    sample_left *= 1.06;
    sample_right *= 1.06;

    // scale to 8bit range from 7bit (0,127) to (0,254)
    #if USE_SCALE
    sample_left = bit::scale<7, 8, u8>(sample_left);
    sample_right = bit::scale<7, 8, u8>(sample_right);
    #else
    sample_left = sample_left << 1;
    sample_right = sample_right << 1;
    #endif

    sample_left *= psg_left_volume(gba);
    sample_right *= psg_right_volume(gba);

    sample_left /= psg_master_volume(gba);
    sample_right /= psg_master_volume(gba);

    // scale to 10bits
    #if USE_SCALE
    const s16 fifo0_sample = bit::scale<8, 10, s16>(APU.fifo[0].sample());
    const s16 fifo1_sample = bit::scale<8, 10, s16>(APU.fifo[1].sample());
    #else
    const auto fifo0_sample = APU.fifo[0].sample() << 2;
    const auto fifo1_sample = APU.fifo[1].sample() << 2;
    #endif

    sample_left += fifo0_sample * APU.fifo[0].enable_left;
    sample_left += fifo1_sample * APU.fifo[1].enable_left;

    sample_right += fifo0_sample * APU.fifo[0].enable_right;
    sample_right += fifo1_sample * APU.fifo[1].enable_right;

    const auto bias = bit::get_range<1, 9>(REG_SOUNDBIAS);
    sample_left += bias;
    sample_right += bias;

    constexpr s16 min = 0x000;
    constexpr s16 max = 0x3FF;
    sample_left = std::clamp(sample_left, min, max);
    sample_right = std::clamp(sample_right, min, max);

    const auto resample_mode = bit::get_range<0xE, 0xF>(REG_SOUNDBIAS);
    assert((resample_mode == 0 || resample_mode == 1) && "resample mode is not currently supported");

    // scale the 10bit value to 16bit
    // don't bit crush (as gba does) because it sounds very bad.
    if (!gba.bit_crushing)
    {
        #if USE_SCALE
        sample_left = bit::scale<10, 16, s16>(sample_left);
        sample_right = bit::scale<10, 16, s16>(sample_right);
        #else
        sample_left <<= 6;
        sample_right <<= 6;
        #endif
    }
    else
    {
        // the bit-depth differs based on the resample mode
        static constexpr s16 divs[4] = { 1, 2, 3, 4 }; // 9bit, 8bit, 7bit, 6bit
        sample_left >>= divs[resample_mode];
        sample_right >>= divs[resample_mode];

        #if USE_SCALE
        switch (resample_mode)
        {
            case 0:
                sample_left = bit::scale<9, 16, s16>(sample_left);
                sample_right = bit::scale<9, 16, s16>(sample_right);
                break;

            case 1:
                sample_left = bit::scale<8, 16, s16>(sample_left);
                sample_right = bit::scale<8, 16, s16>(sample_right);
                break;

            case 2:
                sample_left = bit::scale<7, 16, s16>(sample_left);
                sample_right = bit::scale<7, 16, s16>(sample_right);
                break;

            case 3:
                sample_left = bit::scale<6, 16, s16>(sample_left);
                sample_right = bit::scale<6, 16, s16>(sample_right);
                break;
        }
        #else
        // we now need to scale to to 16bit range
        static constexpr s16 scales[4] = { 6, 7, 8, 9 }; // 9bit, 8bit, 7bit, 6bit
        sample_left <<= scales[resample_mode];
        sample_right <<= scales[resample_mode];
        #endif
    }

    // make the output signed
    sample_left ^= 0x8000;
    sample_right ^= 0x8000;

    lowpass(gba, sample_left, sample_right, 0.3, 3);
    push_sample(gba, sample_left, sample_right);
}

auto sample(Gba& gba)
{
    if (gba.audio_callback == nullptr || gba.sample_data.empty()) [[unlikely]]
    {
        return;
    }

    if (!is_apu_enabled(gba)) [[unlikely]]
    {
        push_sample(gba, 0, 0);
        return;
    }

    if (gba.is_gb())
    {
        sample_gb(gba);
    }
    else
    {
        sample_gba(gba);
    }
}

auto on_sample_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    sample(gba);
    gba.scheduler.add(id, gba.delta.get(id, gba.sample_rate_calculated), on_sample_event, &gba);
}

auto on_frame_sequencer_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    APU.frame_sequencer.clock(gba);

    if (gba.is_gba())
    {
        gba.delta.add(id, late);
        gba.scheduler.add(id, gba.delta.get(id, get_frame_sequencer_cycles(gba)), on_frame_sequencer_event, &gba);
    }
}

#undef APU
} // namespace gba::apu
