// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>
#include <array>
#include <utility>

namespace gba::scheduler
{

enum class Event : std:: uint8_t
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

    // not an actual event, it just keeps track of the size
    END,
};

using callback = void(*)(Gba& gba);

struct Entry
{
    callback cb;
    std::uint32_t cycles;
    std::int16_t delta; // todo: make this unsigned instead
    bool enabled;
};

struct Scheduler
{
    std::array<Entry, std::to_underlying(Event::END)> entries;

    std::uint32_t cycles;
    std::uint32_t next_event_cycles;
    Event next_event;
};

auto on_loadstate(Gba& gba) -> void;
auto reset(Gba& gba) -> void;
auto fire(Gba& gba) -> void;
auto add(Gba& gba, Event e, callback cb, std::uint32_t cycles) -> void;
auto remove(Gba& gba, Event e) -> void;

} // namespace gba::scheduler
