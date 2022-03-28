// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "apu.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "dma.hpp"
#include "mem.hpp"
#include "scheduler.hpp"
#include <cassert>
#include <type_traits>

// bad code lives here
// so i copy / pasted my gb apu code from my gb emu
// c++-ifyed it used classes and inheritence
// gcc did not virtualise all the functions, even with final
// decided on templates instead, this worked nicely
// but still it is a mix of class / template / legacy c code

namespace gba::apu
{

#define APU gba.apu

constexpr scheduler::Event EVENTS[4] =
{
    scheduler::Event::APU_SQUARE0,
    scheduler::Event::APU_SQUARE1,
    scheduler::Event::APU_WAVE,
    scheduler::Event::APU_NOISE,
};

constexpr scheduler::callback CALLBACKS[4] =
{
    on_square0_event,
    on_square1_event,
    on_wave_event,
    on_noise_event,
};

// gba has 4 sample modes [32768, 65536, 131072, 262144]
// however almost all games use either mode0 or mode1
// because of this (and to save pointless sampling)
// we sample at 65536hz
constexpr auto SAMPLE_RATE = 65536;
constexpr auto SAMPLE_TICKS = 280896*60/SAMPLE_RATE;

constexpr uint8_t PERIOD_TABLE[8] = { 8, 1, 2, 3, 4, 5, 6, 7 };

constexpr s8 SQUARE_DUTY_CYCLES[4][8] =
{
    /*[0] = */{ -1, -1, -1, -1, -1, -1, -1, +1 },
    /*[1] = */{ +1, -1, -1, -1, -1, -1, -1, +1 },
    /*[2] = */{ -1, -1, -1, -1, -1, +1, +1, +1 },
    /*[3] = */{ -1, +1, +1, +1, +1, +1, +1, -1 },
};

bool is_apu_enabled(Gba& gba);
auto apu_on_enabled(Gba& gba) -> void;
auto apu_on_disabled(Gba& gba) -> void;

void on_wave_mem_write(Gba& gba, u32 addr, uint8_t value);
void on_nr52_write(Gba& gba, uint8_t value);
bool is_next_frame_sequencer_step_not_len(const Gba& gba);
bool is_next_frame_sequencer_step_vol(const Gba& gba);

template<typename T>
auto& get_channel_from_type(Gba& gba)
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
namespace sweep
{
    auto clock(Gba& gba, auto& channel);
    auto trigger(Gba& gba, auto& channel);
    auto get_new_freq(auto& channel) -> uint16_t;
    auto do_freq_calc(Gba& gba, auto& channel, bool update_value) -> void;
    auto update_enabled_flag(auto& channel) -> void;
}

namespace len
{
    auto clock(Gba& gba, auto& channel);
    auto trigger(Gba& gba, auto& channel, auto reload);
    void on_nrx4_edge_case_write(Gba& gba, auto& channel, u8 value);
}

namespace env
{
    auto clock(Gba& gba, auto& channel);
    auto trigger(Gba& gba, auto& channel);
    void write(Gba& gba, auto& channel, u8 value);
}

auto left_volume(Gba& gba)
{
    return 1 + bit::get_range<0, 2>(REG_SOUNDCNT_L);
}

auto right_volume(Gba& gba)
{
    return 1 + bit::get_range<4, 6>(REG_SOUNDCNT_L);
}

auto master_volume(Gba& gba)
{
    constexpr u8 VOL_SHIFT[4] = { 2, 1, 0, /*invalid*/0 };
    const auto index = bit::get_range<0, 1>(REG_SOUNDCNT_H);
    assert(index != 3 && "invalid master volume");

    return VOL_SHIFT[index];
}

template<u8 Number>
auto Base<Number>::enable(Gba& gba) -> void
{
    REG_SOUNDCNT_X = bit::set<num>(REG_SOUNDCNT_X, true);
}

template<u8 Number>
auto Base<Number>::disable(Gba& gba) -> void
{
    REG_SOUNDCNT_X = bit::set<num>(REG_SOUNDCNT_X, false);
    scheduler::remove(gba, EVENTS[Number]);
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

auto sweep::get_new_freq(auto& channel) -> uint16_t
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

auto sweep::do_freq_calc(Gba& gba, auto& channel, bool update_value) -> void
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

auto len::trigger(Gba& gba, auto& channel, auto reload)
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

void len::on_nrx4_edge_case_write(Gba& gba, auto& channel, u8 value)
{
    auto& len = channel.len;

    // if next is not len and len is NOW enabled, it is clocked
    if (is_next_frame_sequencer_step_not_len(gba) && len.counter && !len.counter && bit::is_set<6>(value))
    {
        --len.counter;

        gba_log("[APU] ch1 edge case: extra len clock!\n");
        // if this makes the result 0, and trigger is clear, disable channel
        if (!len.counter && !bit::is_set<7>(value))
        {
            channel.disable(gba);
        }
    }
}

auto env::clock(Gba& gba, auto& channel)
{
    auto& env = channel.env;

    if (env.disable == false)
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

void env::write(Gba& gba, auto& channel, u8 value)
{
    auto& env = channel.env;

    const uint8_t starting_vol = bit::get_range<4, 7>(value);
    const bool mode = bit::is_set<3>(value);
    const uint8_t period = bit::get_range<0, 2>(value);

    if (channel.is_enabled(gba))
    {
        if (env.period == 0 && env.disable == false)
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

    env.starting_vol = starting_vol;
    env.mode = mode;
    env.period = period;

    if (channel.is_dac_enabled() == false)
    {
        channel.disable(gba);
    }
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

auto Fifo::push(std::uint8_t value) -> void
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
        if (APU.fifo[i].timer_select == timer_num)
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
auto SquareBase<Number>::sample(Gba& gba) const -> s8
{
    const auto dcycle = SQUARE_DUTY_CYCLES[duty][duty_index];
    return env.volume * dcycle * this->is_enabled(gba);
}

template<u8 Number>
auto SquareBase<Number>::get_freq() const -> u32
{
    return (2048 - ((static_cast<u32>(freq_msb) << 8) | static_cast<u32>(freq_lsb))) * 16;
}

template<u8 Number>
auto SquareBase<Number>::is_dac_enabled() -> bool
{
    return env.starting_vol > 0 || env.mode > 0;
}

template<typename T>
constexpr auto clock2(Gba& gba, T& channel)
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
    else // square0 | square1
    {
        channel.duty_index = (channel.duty_index + 1) % 8;
    }
}

template<typename T>
constexpr auto clock(Gba& gba, T& channel, u8 cycles)
{
    if (channel.is_enabled(gba))
    {
        const auto freq = channel.get_freq();

        if (channel.timer > 0 || freq)
        {
            channel.timer -= cycles;
            while (channel.timer <= 0)
            {
                assert(freq != 0 && "endless loop in apu");
                channel.timer += freq;
                clock2<T>(gba, channel);
            }
        }
    }
}

template<typename T>
auto on_channel_event(Gba& gba) -> void
{
    auto& channel = get_channel_from_type<T>(gba);

    clock2<T>(gba, channel);

    const auto freq = channel.get_freq();
    if (freq > 0)
    {
        scheduler::add(gba, EVENTS[channel.num], CALLBACKS[channel.num], freq);
    }
}

auto on_square0_event(Gba& gba) -> void
{
    on_channel_event<Square0>(gba);
}

auto on_square1_event(Gba& gba) -> void
{
    on_channel_event<Square1>(gba);
}

auto on_wave_event(Gba& gba) -> void
{
    on_channel_event<Wave>(gba);
}

auto on_noise_event(Gba& gba) -> void
{
    on_channel_event<Noise>(gba);
}

// auto on_frame_sequencer_event(Gba& gba) -> void;
// auto on_sample_event(Gba& gba) -> void;

template<typename T>
constexpr auto trigger(Gba& gba, T& channel)
{
    channel.enable(gba);

    constexpr u16 len_reload[4] = { 64, 64, 256, 64 };
    len::trigger(gba, channel, len_reload[channel.num]);

    if constexpr(std::is_same_v<T, Wave>)
    {
        // reset position counter
        channel.position_counter = 0;
        channel.timer = channel.get_freq() + (3 * 2);
    }
    else
    {
        env::trigger(gba, channel);

        if constexpr(std::is_same_v<T, Noise>)
        {
            // set all bits of the lfsr to 1
            channel.lfsr = 0x7FFF;
            channel.timer = channel.get_freq();
        }
        else // square0 | square1
        {
            // when a square channel is triggered, it's lower 2-bits are not modified!
            // SOURCE: https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
            channel.timer = (channel.timer & 0x3) | (channel.get_freq() & ~0x3);

            if constexpr(std::is_same_v<T, Square0>)
            {
                sweep::trigger(gba, channel);
            }
        }
    }

    if (channel.is_dac_enabled() == false) [[unlikely]]
    {
        channel.disable(gba);
    }

    if (channel.is_enabled(gba) && channel.timer > 0) [[likely]]
    {
        scheduler::add(gba, EVENTS[channel.num], CALLBACKS[channel.num], channel.timer);
    }
}

auto Wave::sample(Gba& gba) const -> s8
{
    if (!this->is_enabled(gba))
    {
        return 0;
    }

    // indexed using the ch3 shift.
    // the values in this table are used to right-shift the sample
    constexpr u8 CH3_SHIFT[4] = { 4, 0, 1, 2 };

    // check if we need the access the low of high nibble
    const auto sample = (position_counter & 1) ? sample_buffer & 0xF : sample_buffer >> 4;

    // if = 0, use the above table
    if (force_volume == 0)
    {
        return (sample >> CH3_SHIFT[vol_code]);
    }
    else
    {
        const auto half = sample >> 1;
        return half + (half >> 1);
    }
}

auto Wave::get_freq() const -> u32
{
    return (2048 - ((static_cast<u32>(freq_msb) << 8) | static_cast<u32>(freq_lsb))) * 8;
}

auto Wave::is_dac_enabled() -> bool
{
    return dac_power;
}

auto Noise::sample(Gba& gba) const -> s8
{
    // docs say that it's bit-0 INVERTED
    const auto bit = !(lfsr & 0x1);
    return env.volume * bit * this->is_enabled(gba);
}

auto Noise::get_freq() const -> u32
{
    // indexed using the noise divisor code
    constexpr u32 NOISE_DIVISOR[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

    return (NOISE_DIVISOR[divisor_code] << clock_shift) * 8;
}

auto Noise::is_dac_enabled() -> bool
{
    return env.starting_vol > 0 || env.mode > 0;
}

template<typename T>
constexpr auto on_nrx0_write(Gba& gba, T& channel, uint8_t value)
{
    if constexpr(std::is_same<T, Square0>())
    {
        const bool sweep_negate = bit::is_set<3>(value);

        // if at least 1 sweep negate has happened since last trigger,
        // and negate is now cleared, then disable ch1.
        if (channel.sweep.negate && !sweep_negate && channel.sweep.did_negate)
        {
            gba_log("[APU] NR10 sweep negate cleared, disabling channel!\n");
            channel.disable(gba);
        }

        channel.sweep.period = bit::get_range<4, 6>(value);
        channel.sweep.negate = sweep_negate;
        channel.sweep.shift = bit::get_range<0, 2>(value);
    }
    else if constexpr(std::is_same<T, Wave>())
    {
        channel.bank_mode = bit::is_set<5>(value);
        channel.bank_select = bit::is_set<6>(value);
        channel.dac_power = bit::is_set<7>(value);

        if (channel.is_dac_enabled() == false)
        {
            channel.disable(gba);
        }
    }
}

template<typename T>
constexpr auto on_nrx1_write(Gba& gba, T& channel, uint8_t value)
{
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
constexpr auto on_nrx2_write(Gba& gba, T& channel, uint8_t value)
{
    if constexpr(std::is_same<T, Wave>())
    {
        channel.vol_code = bit::get_range<5, 6>(value);
        channel.force_volume = bit::is_set<7>(value);
    }
    else
    {
        env::write(gba, channel, value);
    }
}

template<typename T>
constexpr auto on_nrx3_write(Gba& gba, T& channel, uint8_t value)
{
    if constexpr(std::is_same<T, Noise>())
    {
        channel.clock_shift = bit::get_range<4, 7>(value);
        channel.width_mode = bit::is_set<3>(value);
        channel.divisor_code = bit::get_range<0, 2>(value);
    }
    else
    {
        channel.freq_lsb = value;
    }
}

template<typename T>
constexpr auto on_nrx4_write(Gba& gba, T& channel, uint8_t value)
{
    len::on_nrx4_edge_case_write(gba, channel, value);

    if constexpr(std::is_same<T, Noise>() == false)
    {
        channel.freq_msb = bit::get_range<0, 2>(value);
    }

    channel.len.enable = bit::is_set<6>(value);

    if (bit::is_set<7>(value))
    {
        trigger(gba, channel);
    }
}

auto write_legacy8(Gba& gba, u32 addr, u8 value) -> void
{
    if (addr == mem::IO_SOUNDCNT_X)
    {
        on_nr52_write(gba, value);
    }
    // otherwise ignore writes if apu is disabled
    else if (!is_apu_enabled(gba))
    {
        return;
    }

    switch (addr)
    {
        case mem::IO_SOUND1CNT_L + 0: on_nrx0_write(gba, APU.square0, value); break;
        case mem::IO_SOUND1CNT_H + 0: on_nrx1_write(gba, APU.square0, value); break;
        case mem::IO_SOUND1CNT_H + 1: on_nrx2_write(gba, APU.square0, value); break;
        case mem::IO_SOUND1CNT_X + 0: on_nrx3_write(gba, APU.square0, value); break;
        case mem::IO_SOUND1CNT_X + 1: on_nrx4_write(gba, APU.square0, value); break;

        case mem::IO_SOUND2CNT_L + 0: on_nrx1_write(gba, APU.square1, value); break;
        case mem::IO_SOUND2CNT_L + 1: on_nrx2_write(gba, APU.square1, value); break;
        case mem::IO_SOUND2CNT_H + 0: on_nrx3_write(gba, APU.square1, value); break;
        case mem::IO_SOUND2CNT_H + 1: on_nrx4_write(gba, APU.square1, value); break;

        case mem::IO_SOUND3CNT_L + 0: on_nrx0_write(gba, APU.wave, value); break;
        case mem::IO_SOUND3CNT_H + 0: on_nrx1_write(gba, APU.wave, value); break;
        case mem::IO_SOUND3CNT_H + 1: on_nrx2_write(gba, APU.wave, value); break;
        case mem::IO_SOUND3CNT_X + 0: on_nrx3_write(gba, APU.wave, value); break;
        case mem::IO_SOUND3CNT_X + 1: on_nrx4_write(gba, APU.wave, value); break;

        case mem::IO_SOUND4CNT_L + 0: on_nrx1_write(gba, APU.noise, value); break;
        case mem::IO_SOUND4CNT_L + 1: on_nrx2_write(gba, APU.noise, value); break;
        case mem::IO_SOUND4CNT_H + 0: on_nrx3_write(gba, APU.noise, value); break;
        case mem::IO_SOUND4CNT_H + 1: on_nrx4_write(gba, APU.noise, value); break;

        // nr5X are already handled
        case 0x24: case 0x25: case 0x26: break;

        case mem::IO_WAVE_RAM0_L + 0:
        case mem::IO_WAVE_RAM0_L + 1:
        case mem::IO_WAVE_RAM0_H + 0:
        case mem::IO_WAVE_RAM0_H + 1:
        case mem::IO_WAVE_RAM1_L + 0:
        case mem::IO_WAVE_RAM1_L + 1:
        case mem::IO_WAVE_RAM1_H + 0:
        case mem::IO_WAVE_RAM1_H + 1:
        case mem::IO_WAVE_RAM2_L + 0:
        case mem::IO_WAVE_RAM2_L + 1:
        case mem::IO_WAVE_RAM2_H + 0:
        case mem::IO_WAVE_RAM2_H + 1:
        case mem::IO_WAVE_RAM3_L + 0:
        case mem::IO_WAVE_RAM3_L + 1:
        case mem::IO_WAVE_RAM3_H + 0:
        case mem::IO_WAVE_RAM3_H + 1:
            on_wave_mem_write(gba, addr, value);
            break;
    }
}

// todo: rewrite the nrxx functions so accept 16bit values
auto write_legacy(Gba& gba, u32 addr, u16 value) -> void
{
    // nr52 is always writeable
    if (addr == mem::IO_SOUNDCNT_X)
    {
        on_nr52_write(gba, value);
    }
    // otherwise ignore writes if apu is disabled
    else if (!is_apu_enabled(gba))
    {
        return;
    }

    switch (addr)
    {
        case mem::IO_SOUND1CNT_L:
            on_nrx0_write(gba, APU.square0, value);
            break;
        case mem::IO_SOUND1CNT_H:
            on_nrx1_write(gba, APU.square0, value);
            on_nrx2_write(gba, APU.square0, value>>8);
            break;
        case mem::IO_SOUND1CNT_X:
            on_nrx3_write(gba, APU.square0, value);
            on_nrx4_write(gba, APU.square0, value>>8);
            break;

        case mem::IO_SOUND2CNT_L:
            on_nrx1_write(gba, APU.square1, value);
            on_nrx2_write(gba, APU.square1, value>>8);
            break;
        case mem::IO_SOUND2CNT_H:
            on_nrx3_write(gba, APU.square1, value);
            on_nrx4_write(gba, APU.square1, value>>8);
            break;

        case mem::IO_SOUND3CNT_L:
            on_nrx0_write(gba, APU.wave, value);
            break;
        case mem::IO_SOUND3CNT_H:
            on_nrx1_write(gba, APU.wave, value);
            on_nrx2_write(gba, APU.wave, value>>8);
            break;
        case mem::IO_SOUND3CNT_X:
            on_nrx3_write(gba, APU.wave, value);
            on_nrx4_write(gba, APU.wave, value>>8);
            break;

        case mem::IO_SOUND4CNT_L:
            on_nrx1_write(gba, APU.noise, value);
            on_nrx2_write(gba, APU.noise, value>>8);
            break;
        case mem::IO_SOUND4CNT_H:
            on_nrx3_write(gba, APU.noise, value);
            on_nrx4_write(gba, APU.noise, value>>8);
            break;

        // nr5X are already handled
        case 0x24: case 0x25: case 0x26:
            break;

        case mem::IO_WAVE_RAM0_L:
        case mem::IO_WAVE_RAM0_H:
        case mem::IO_WAVE_RAM1_L:
        case mem::IO_WAVE_RAM1_H:
        case mem::IO_WAVE_RAM2_L:
        case mem::IO_WAVE_RAM2_H:
        case mem::IO_WAVE_RAM3_L:
        case mem::IO_WAVE_RAM3_H:
            on_wave_mem_write(gba, addr+0, value>>0);
            on_wave_mem_write(gba, addr+1, value>>8);
            break;
    }
}

void on_nr52_write(Gba& gba, uint8_t value)
{
    const auto master_enable = bit::is_set<7>(value);

    if (APU.enabled)
    {
        if (master_enable == 0)
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

void on_wave_mem_write(Gba& gba, u32 addr, uint8_t value)
{
    // treated as 2 banks
    if (APU.wave.bank_mode == 0)
    {
        const auto wave_addr = addr &= 0xF;

        // writes happen to the opposite bank
        const auto bank_select = APU.wave.bank_select^1;
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

auto reset(Gba& gba) -> void
{
    // by default bias is set to 0x100 and resample mode is 0
    REG_SOUNDBIAS = 0x100 << 1;
    // enable sound on startup
    REG_SOUNDCNT_X = bit::set<7>(REG_SOUNDCNT_X, true);
    APU.enabled = true;
    // todo: reset al apu regs properly if skipping bios
    APU.fifo[0].reset();
    APU.fifo[1].reset();
    scheduler::add(gba, scheduler::Event::APU_SAMPLE, on_sample_event, SAMPLE_TICKS);
    scheduler::add(gba, scheduler::Event::APU_FRAME_SEQUENCER, on_frame_sequencer_event, APU.frame_sequencer.tick_rate);
}

void clock_len(Gba& gba)
{
    len::clock(gba, APU.square0);
    len::clock(gba, APU.square1);
    len::clock(gba, APU.wave);
    len::clock(gba, APU.noise);
}

void clock_sweep(Gba& gba)
{
    sweep::clock(gba, APU.square0);
}

void clock_env(Gba& gba)
{
    env::clock(gba, APU.square0);
    env::clock(gba, APU.square1);
    env::clock(gba, APU.noise);
}

bool is_apu_enabled(Gba& gba)
{
    assert(APU.enabled == bit::is_set<7>(REG_SOUNDCNT_X) && "apu enabled missmatch");
    return APU.enabled;
}

auto apu_on_enabled(Gba& gba) -> void
{
    gba_log("[APU] enabling...\n");

    // todo: reset more regs
    APU.square0.duty = 0;
    APU.square1.duty = 0;
    APU.square0.duty_index = 0;
    APU.square1.duty_index = 0;
    APU.frame_sequencer.index = 0;

    // only add back scheduler event.
    // channels are only re-enabled on trigger
    scheduler::add(gba, scheduler::Event::APU_FRAME_SEQUENCER, on_frame_sequencer_event, APU.frame_sequencer.tick_rate);
}

auto apu_on_disabled(Gba& gba) -> void
{
    gba_log("[APU] disabling...\n");

    // these events are no longer ticked
    scheduler::remove(gba, scheduler::Event::APU_SQUARE0);
    scheduler::remove(gba, scheduler::Event::APU_SQUARE1);
    scheduler::remove(gba, scheduler::Event::APU_WAVE);
    scheduler::remove(gba, scheduler::Event::APU_NOISE);
    scheduler::remove(gba, scheduler::Event::APU_FRAME_SEQUENCER);
}

// this is used when a channel is triggered
bool is_next_frame_sequencer_step_not_len(const Gba& gba)
{
    // check if the current counter is the len clock, the next one won't be!
    return APU.frame_sequencer.index & 0x1; // same as below code
}

// this is used when channels 1,2,4 are triggered
bool is_next_frame_sequencer_step_vol(const Gba& gba)
{
    // check if the current counter is the len clock, the next one won't be!
    return APU.frame_sequencer.index == 7;
}

// this is clocked by DIV
auto FrameSequencer::clock(Gba &gba) -> void
{
    // return;
    assert(is_apu_enabled(gba) && "clocking fs when apu is disabled");

    switch (index)
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

    index = (index + 1) % 8;
}

auto Noise::clock_lfsr(Gba& gba) -> void
{
    const auto bit0 = this->lfsr & 0x1;
    const auto bit1 = (this->lfsr >> 1) & 0x1;
    const auto result = bit1 ^ bit0;

    // now we shift the lfsr BEFORE setting the value!
    this->lfsr >>= 1;

    // now set bit 15
    this->lfsr |= (result << 14);

    // set bit-6 if the width is half-mode
    if (this->width_mode == 1)
    {
        // unset it first!
        this->lfsr &= ~(1 << 6);
        this->lfsr |= (result << 6);
    }
}

void Wave::advance_position_counter(Gba& gba)
{
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

auto Fifo::sample() -> int8_t
{
    constexpr u8 VOL_SHIFT[2] = { 1, 0 };
    return this->current_sample >> VOL_SHIFT[this->volume_code];
}

auto sample(Gba& gba)
{
    if (!gba.audio_callback) [[unlikely]]
    {
        return;
    }

    if (!is_apu_enabled(gba)) [[unlikely]]
    {
        gba.audio_callback(gba.userdata, 0, 0);
        return;
    }

    const auto resample_mode = bit::get_range<0xE, 0xF>(REG_SOUNDBIAS);
    assert(resample_mode == 0 || resample_mode == 1);

    const auto fifo_a = APU.fifo[0].sample();
    const auto fifo_b = APU.fifo[1].sample();
    const auto square0 = APU.square0.sample(gba);
    const auto square1 = APU.square1.sample(gba);
    const auto wave = APU.wave.sample(gba);
    const auto noise = APU.noise.sample(gba);

    const auto fifo_a_left = fifo_a * APU.fifo[0].enable_left;
    const auto fifo_b_left = fifo_b * APU.fifo[1].enable_left;
    const auto square0_left = square0 * APU.square0.left_enabled(gba);
    const auto square1_left = square1 * APU.square1.left_enabled(gba);
    const auto wave_left = wave * APU.wave.left_enabled(gba);
    const auto noise_left = noise * APU.noise.left_enabled(gba);

    const auto fifo_a_right = fifo_a * APU.fifo[0].enable_right;
    const auto fifo_b_right = fifo_b * APU.fifo[1].enable_right;
    const auto square0_right = square0 * APU.square0.right_enabled(gba);
    const auto square1_right = square1 * APU.square1.right_enabled(gba);
    const auto wave_right = wave * APU.wave.right_enabled(gba);
    const auto noise_right = noise * APU.noise.right_enabled(gba);

    const auto master_vol = master_volume(gba);
    const auto left_vol = left_volume(gba);
    const auto right_vol = right_volume(gba);

    const auto final_fifo_left = (fifo_a_left + fifo_b_left) / 2;
    const auto final_gb_left = (((square0_left + square1_left + wave_left + noise_left) * left_vol) >> master_vol) / 4;
    const auto final_fifo_right = (fifo_a_right + fifo_b_right) / 2;
    const auto final_gb_right = (((square0_right + square1_right + wave_right + noise_right) * right_vol) >> master_vol) / 4;

    const auto left = final_fifo_left + final_gb_left;
    const auto right = final_fifo_right + final_gb_right;

    gba.audio_callback(gba.userdata, left<<8, right<<8);
}

auto on_sample_event(Gba& gba) -> void
{
    sample(gba);
    scheduler::add(gba, scheduler::Event::APU_SAMPLE, on_sample_event, SAMPLE_TICKS);
}

auto on_frame_sequencer_event(Gba& gba) -> void
{
    APU.frame_sequencer.clock(gba);
    scheduler::add(gba, scheduler::Event::APU_FRAME_SEQUENCER, on_frame_sequencer_event, APU.frame_sequencer.tick_rate);
}

#if ENABLE_SCHEDULER == 0
auto run(Gba& gba, uint8_t cycles) -> void
{
    if (is_apu_enabled(gba))
    {
        clock(gba, APU.square0, cycles);
        clock(gba, APU.square1, cycles);
        clock(gba, APU.wave, cycles);
        clock(gba, APU.noise, cycles);

        // check if we need to tick the frame sequencer!
        APU.frame_sequencer.cycles += cycles;
        while (APU.frame_sequencer.cycles >= APU.frame_sequencer.tick_rate)
        {
            APU.frame_sequencer.cycles -= APU.frame_sequencer.tick_rate;
            APU.frame_sequencer.clock(gba);
        }
    }

    APU.cycles += cycles;
    if (APU.cycles >= SAMPLE_TICKS)
    {
        APU.cycles -= SAMPLE_TICKS;
        sample(gba);
    }
}
#endif

#undef APU
} // namespace gba::apu
