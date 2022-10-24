// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstddef>

namespace gba::apu {

struct Square0;
struct Square1;
struct Wave;
struct Noise;

struct FrameSequencer
{
    u8 index;
    auto clock(Gba& gba) -> void;
};

struct Len
{
    u16 counter;
    bool enable;
};

struct Sweep
{
    u16 freq_shadow_register;
    u8 period;
    u8 shift;
    s8 timer;

    bool enabled;
    bool negate;
    bool did_negate;
};

struct Envelope
{
    enum Mode : bool
    {
        SUB = false,
        ADD = true
    };

    u8 starting_vol;
    u8 volume;
    u8 period;
    s8 timer;
    bool mode;
    bool disable;
};

// todo: don't use template class, its bad
// just have a seperate class for every channel
// even if it means repeated code, its better than this.
// also, don't use inheritance either, vtables bad.
// potentially could have 1 big class that covers all channels
// and then have an array[4]
template<u8 Number>
struct Base
{
    static_assert(Number <= 3, "invalid channel number");

    static constexpr inline auto num = Number;
    s32 timer; // every channel has a timer
    Len len; // every channel has one

    auto enable(Gba& gba) -> void;
    auto disable(Gba& gba) -> void;
    [[nodiscard]] auto is_enabled(Gba& gba) const -> bool;
    [[nodiscard]] auto left_enabled(Gba& gba) const -> bool;
    [[nodiscard]] auto right_enabled(Gba& gba) const -> bool;
    // virtual auto sample() const -> s8 = 0;
    // virtual auto get_freq() const -> u32 = 0;
    // virtual auto trigger(Gba& gba) -> void = 0;
    // virtual auto is_dac_enabled() -> bool = 0;
};

template<u8 Number>
struct SquareBase : Base<Number>
{
    Envelope env;
    u8 duty;
    u8 freq_lsb;
    u8 freq_msb;
    u8 duty_index;

    [[nodiscard]] auto sample(Gba& gba) const -> u8;
    [[nodiscard]] auto get_freq(Gba& gba) const -> u32;
    [[nodiscard]] auto is_dac_enabled() const -> bool;
};

struct Square0 final : SquareBase<0>
{
    Sweep sweep;
};

struct Square1 final : SquareBase<1>
{
};

struct Wave final : Base<2>
{
    // each sample is 4bits, theres 32 samples
    // so 32 / 2 = 16.
    // gba has 2 banks of wave ram, so 16 * 2 = 32
    u8 ram[32 / 2 * 2];
    u8 vol_code;
    u8 freq_lsb;
    u8 freq_msb;
    u8 sample_buffer;
    u8 position_counter;

    bool bank_select;
    bool bank_mode;
    bool force_volume;
    bool dac_power;

    auto advance_position_counter(Gba& gba) -> void;

    [[nodiscard]] auto sample(Gba& gba) const -> u8;
    [[nodiscard]] auto get_freq(Gba& gba) const -> u32;
    [[nodiscard]] auto is_dac_enabled() const -> bool;
    [[nodiscard]] auto get_volume_divider(Gba& gba) const -> float;
};

struct Noise final : Base<3>
{
    Envelope env;

    u16 lfsr;
    u8 clock_shift;
    u8 divisor_code;

    bool half_width_mode;

    auto clock_lfsr(Gba& gba) -> void;

    [[nodiscard]] auto sample(Gba& gba) const -> u8;
    [[nodiscard]] auto get_freq(Gba& gba) const -> u32;
    [[nodiscard]] auto is_dac_enabled() const -> bool;
};

struct Fifo
{
    static constexpr inline auto capacity = 32;

    s8 buf[capacity];
    u8 r_index;
    u8 w_index;
    u8 count;

    s8 current_sample;
    bool volume_code;
    bool enable_right;
    bool enable_left;
    bool timer_select;

    auto update_current_sample(Gba& gba, u8 num) -> void;

    [[nodiscard]] auto sample() const -> s8;
    auto reset() -> void;
    [[nodiscard]] auto size() const -> u8;
    auto push(u8 value) -> void;
    [[nodiscard]] auto pop() -> s8;
};

struct Apu
{
    Fifo fifo[2];

    // legacy gb apu
    FrameSequencer frame_sequencer;
    Square0 square0;
    Square1 square1;
    Wave wave;
    Noise noise;

    bool enabled;
};

STATIC auto on_fifo_write8(Gba& gba, u8 value, u8 num) -> void;
STATIC auto on_fifo_write16(Gba& gba, u16 value, u8 num) -> void;
STATIC auto on_fifo_write32(Gba& gba, u32 value, u8 num) -> void;
STATIC auto on_timer_overflow(Gba& gba, u8 timer_num) -> void;
STATIC auto on_soundcnt_write(Gba& gba) -> void;

STATIC auto write_NR10(Gba& gba, u8 value) -> void;
STATIC auto write_NR11(Gba& gba, u8 value) -> void;
STATIC auto write_NR12(Gba& gba, u8 value) -> void;
STATIC auto write_NR13(Gba& gba, u8 value) -> void;
STATIC auto write_NR14(Gba& gba, u8 value) -> void;
STATIC auto write_NR21(Gba& gba, u8 value) -> void;
STATIC auto write_NR22(Gba& gba, u8 value) -> void;
STATIC auto write_NR23(Gba& gba, u8 value) -> void;
STATIC auto write_NR24(Gba& gba, u8 value) -> void;
STATIC auto write_NR30(Gba& gba, u8 value) -> void;
STATIC auto write_NR31(Gba& gba, u8 value) -> void;
STATIC auto write_NR32(Gba& gba, u8 value) -> void;
STATIC auto write_NR33(Gba& gba, u8 value) -> void;
STATIC auto write_NR34(Gba& gba, u8 value) -> void;
STATIC auto write_NR41(Gba& gba, u8 value) -> void;
STATIC auto write_NR42(Gba& gba, u8 value) -> void;
STATIC auto write_NR43(Gba& gba, u8 value) -> void;
STATIC auto write_NR44(Gba& gba, u8 value) -> void;
STATIC auto write_NR50(Gba& gba, u8 value) -> void;
STATIC auto write_NR51(Gba& gba, u8 value) -> void;
STATIC auto write_NR52(Gba& gba, u8 value) -> void;
STATIC auto write_WAVE(Gba& gba, u8 addr, u8 value) -> void;
STATIC auto read_WAVE(Gba& gba, u8 addr) -> u8;

STATIC auto on_square0_event(void* user, s32 id, s32 late) -> void;
STATIC auto on_square1_event(void* user, s32 id, s32 late) -> void;
STATIC auto on_wave_event(void* user, s32 id, s32 late) -> void;
STATIC auto on_noise_event(void* user, s32 id, s32 late) -> void;
STATIC auto on_frame_sequencer_event(void* user, s32 id = 0, s32 late = 0) -> void;
STATIC auto on_sample_event(void* user, s32 id, s32 late) -> void;

STATIC auto is_apu_enabled(Gba& gba) -> bool;

STATIC auto reset(Gba& gba, bool skip_bios) -> void;

} // namespace gba::apu
