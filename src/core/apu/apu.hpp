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
    static constexpr inline auto tick_rate = 280896*60/512;
    u16 cycles;
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
    enum Mode
    {
        SUB = 0,
        ADD = 1
    };

    u8 starting_vol;
    u8 volume;
    u8 period;
    s8 timer;
    bool mode;
    bool disable;
};

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

    [[nodiscard]] auto sample(Gba& gba) const -> s8;
    [[nodiscard]] auto get_freq() const -> u32;
    [[nodiscard]] auto is_dac_enabled() const -> bool;
};

struct Square0 : SquareBase<0>
{
    Sweep sweep;
};

struct Square1 : SquareBase<1>
{
};

struct Wave : Base<2>
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

    [[nodiscard]] auto sample(Gba& gba) const -> s8;
    [[nodiscard]] auto get_freq() const -> u32;
    [[nodiscard]] auto is_dac_enabled() const -> bool;
};

struct Noise : Base<3>
{
    Envelope env;

    u16 lfsr;
    u8 clock_shift;
    u8 divisor_code;

    bool half_width_mode;

    auto clock_lfsr(Gba& gba) -> void;

    [[nodiscard]] auto sample(Gba& gba) const -> s8;
    [[nodiscard]] auto get_freq() const -> u32;
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
    std::size_t cycles; // todo: remove once scheduler version is complete
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

STATIC auto write_legacy8(Gba& gba, u32 addr, u8 value) -> void;
STATIC auto write_legacy(Gba& gba, u32 addr, u16 value) -> void;

STATIC auto on_square0_event(Gba& gba) -> void;
STATIC auto on_square1_event(Gba& gba) -> void;
STATIC auto on_wave_event(Gba& gba) -> void;
STATIC auto on_noise_event(Gba& gba) -> void;
STATIC auto on_frame_sequencer_event(Gba& gba) -> void;
STATIC auto on_sample_event(Gba& gba) -> void;

STATIC auto is_apu_enabled(Gba& gba) -> bool;

STATIC auto reset(Gba& gba, bool skip_bios) -> void;

} // namespace gba::apu
