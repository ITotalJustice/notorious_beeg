// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>
#include <array>
#include <utility>

namespace gba::scheduler {

enum class Event : u8
{
    PPU,
    // todo: profile removing the apu channel events
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

    // special event to indicate the end of a frame.
    // the cycles is set by the user in run();
    FRAME,

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
    u32 cycles;
    s32 delta; // todo: make this unsigned instead
    bool enabled;
};

struct Scheduler
{
    std::array<Entry, std::to_underlying(Event::END)> entries;

    u32 cycles;
    u32 next_event_cycles;
    u32 elapsed;
    Event next_event;
    bool frame_end; // have this here because there 2+bytes of padding in this struct

    // todo: fix this bug by ticking scheduler directly
    // try openlara menu and listen to the audio pop
    // auto tick(u32 cycle_amount) { this->cycles += cycle_amount; }
    auto tick(u32 cycle_amount) { this->elapsed += cycle_amount; }

    [[nodiscard]] auto get_cycles() const
    {
        return cycles;
    }

    [[nodiscard]] auto get_event(Event e) -> Entry&
    {
        return entries[std::to_underlying(e)];
    }

    [[nodiscard]] auto is_event_enabled(Event e) const
    {
        return entries[std::to_underlying(e)].enabled;
    }

    [[nodiscard]] auto get_event_cycles(Event e) const
    {
        return entries[std::to_underlying(e)].cycles;
    }

    [[nodiscard]] auto get_event_delta(Event e) const
    {
        return entries[std::to_underlying(e)].delta;
    }
};

auto on_loadstate(Gba& gba) -> void;
auto reset(Gba& gba) -> void;
auto fire(Gba& gba) -> void;
auto add(Gba& gba, Event e, callback cb, u32 cycles) -> void;
auto remove(Gba& gba, Event e) -> void;

} // namespace gba::scheduler
