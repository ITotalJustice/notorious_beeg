// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <array>

namespace gba::scheduler
{

enum class Event
{
    PPU,
    APU_SQUARE0,
    APU_SQUARE1,
    APU_WAVE,
    APU_NOISE,
    APU_FRAME_SEQUENCER,
    APU_SAMPLE,
    TIMER0,
    TIMER1,
    TIMER2,
    TIMER3,
    DMA,
    INTERRUPT,
    HALT,

    // special event which adjusts all cycles
    // to prevent the counter from overflowing
    RESET,
};

using callback = void(*)(Gba& gba);

struct Entry
{
    callback cb;
    u32 cycles;
    bool enabled;
};

struct Scheduler
{
    std::array<Entry, static_cast<u8>(Event::RESET) + 1> entries;

    u32 cycles;
    u32 next_event_cycles;
    Event next_event;
};

auto on_loadstate(Gba& gba) -> void;
auto reset(Gba& gba) -> void;
auto fire(Gba& gba) -> void;
auto add(Gba& gba, Event e, callback cb, u32 cycles) -> void;
auto remove(Gba& gba, Event e) -> void;

} // namespace gba::scheduler
