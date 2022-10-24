/*
simple-ish, fast-ish, generic-ish scheduler implementation.
based on discussion <https://github.com/dolphin-emu/dolphin/pull/4168>

NOTE: scheduler struct is not suitable for savestates
there is is example code below showing how save/load state.
*/
// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>

// set this to 0 if the scheduler can be empty, 1 by default
// due to the reset event always being set!
#ifndef SCHEDULER_NEVER_EMPTY
    #define SCHEDULER_NEVER_EMPTY 1
#endif

namespace scheduler {

using s32 = std::int32_t;
// id is the id of the event
// cycles_late will always be 0 if on time or negative if late
using Callback = void(*)(void* user, s32 id, s32 cycles_late);

enum { RESERVED_ID = 0x7FFFFFFF };

struct Event {
    s32 time; // time until event expires (scheduler.cycles + event.cycle)
    s32 id; // event id
    Callback callback; // function to call on event expire
    void* user; // user data passed to the callback

    // used for std::_heap functions
    constexpr auto operator>(const Event& rhs) const -> bool { return time > rhs.time; }
};

struct Scheduler {
public:
    // calls reset()
    Scheduler() { reset(); }
    // resets queue and cycles, adds reset event, optional custom callback
    constexpr void reset(s32 starting_cycles = 0, Callback reset_cb = nullptr);
    // fires all expired events
    constexpr void fire();
    // adds relative new / existing event. updates time,cb,user if existing
    constexpr void add(s32 id, s32 event_time, Callback cb, void* user);
    // adds new / existing event. updates time,cb,user if existing
    constexpr void add_absolute(s32 id, s32 event_time, Callback cb, void* user);
    // removes an event, does nothing if event not enabled.
    constexpr void remove(s32 id);
    // advances scheduler so that get_ticks() == get_next_event_cycles() if event has greater cycles
    constexpr void advance_to_next_event();
    // returns if an event is found with matching id
    [[nodiscard]] constexpr auto has_event(s32 id) const -> bool;
    // returns event cycles or 0 if not found
    [[nodiscard]] constexpr auto get_event_cycles(s32 id) const -> s32;
    // advance scheduler by number of ticks
    constexpr void tick(s32 ticks);
    // returns current time of the scheduler
    [[nodiscard]] constexpr auto get_ticks() const -> s32;
    // returns true if there are no events
    [[nodiscard]] constexpr auto empty() const -> bool;
    // return true if fire() should be called
    [[nodiscard]] constexpr auto should_fire() const -> bool;
    // return cycles of next event or 0 if no events
    [[nodiscard]] constexpr auto get_next_event_cycles() const -> s32;

private:
    // s32 overflows at 0x7FFFFFFF, just over 100 million gap
    static constexpr s32 TIMEOUT_VALUE = 0x70000000;

    static void reset_event(void* user, s32 id, [[maybe_unused]] s32 _)
    {
        auto s = static_cast<Scheduler*>(user);

        // no sort because order remains the same.
        std::for_each(s->queue.begin(), s->queue.end(), [](auto& e){ e.time -= TIMEOUT_VALUE; });

        s->cycles -= TIMEOUT_VALUE;
        s->add_absolute(id, TIMEOUT_VALUE, reset_event, user);
    }

    std::vector<Event> queue; // don't manually edit this!
    s32 cycles; // remember to tick this!
};

constexpr void Scheduler::reset(s32 starting_cycles, Callback reset_cb)
{
    queue.clear();
    cycles = std::min(starting_cycles, TIMEOUT_VALUE);
    add_absolute(RESERVED_ID, TIMEOUT_VALUE, reset_cb ? reset_cb : reset_event, this);
}

constexpr void Scheduler::fire()
{
    while (!empty())
    {
        const auto event = queue.front();
        // if event hasnt expired, we break early as we know no other
        // events have expired because the queue is sorted.
        if (event.time > cycles)
        {
            break;
        }
        std::pop_heap(queue.begin(), queue.end(), std::greater<>());
        queue.pop_back();
        event.callback(event.user, event.id, event.time - cycles);
    }
}

constexpr void Scheduler::add(s32 id, s32 event_time, Callback cb, void* user)
{
    add_absolute(id, cycles + event_time, cb, user);
}

constexpr void Scheduler::add_absolute(s32 id, s32 event_time, Callback cb, void* user)
{
    const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });

    // if event if already in queue then update time, cb and user.
    if (itr != queue.end())
    {
        itr->time = event_time;
        itr->callback = cb;
        itr->user = user;
        std::push_heap(queue.begin(), queue.end(), std::greater<>());
    }
    // otherwise create new event
    else
    {
        queue.emplace_back(Event{event_time, id, cb, user});
        std::push_heap(queue.begin(), queue.end(), std::greater<>());
    }
}

constexpr void Scheduler::remove(s32 id)
{
    const auto itr = std::remove_if(queue.begin(), queue.end(), [id](auto& e) { return id == e.id; });

    if (itr != queue.end())
    {
        queue.erase(itr, queue.end());
        std::make_heap(queue.begin(), queue.end(), std::greater<>()); // optimise this
    }
}

constexpr void Scheduler::advance_to_next_event()
{
    if (!empty())
    {
        // only advance if the next event time is greater than current time
        if (queue.front().time > cycles)
        {
            cycles = queue.front().time;
        }
    }
}

[[nodiscard]] constexpr auto Scheduler::has_event(s32 id) const -> bool
{
    const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });
    return itr != queue.end();
}

[[nodiscard]] constexpr auto Scheduler::get_event_cycles(s32 id) const -> s32
{
    const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });
    if (itr == queue.end())
    {
        return 0;
    }
    return itr->time;
}

constexpr void Scheduler::tick(s32 ticks)
{
    cycles += ticks;
}

// returns current time of the scheduler
[[nodiscard]] constexpr auto Scheduler::get_ticks() const -> s32
{
    return cycles;
}

// returns true if there are no events
[[nodiscard]] constexpr auto Scheduler::empty() const -> bool
{
    #if SCHEDULER_NEVER_EMPTY
    return false;
    #else
    return queue.empty();
    #endif
}

// return true if fire() should be called
[[nodiscard]] constexpr auto Scheduler::should_fire() const -> bool
{
    if (empty())
    {
        return false;
    }
    return queue[0].time <= cycles;
}

// return cycles of next event or 0 if no events
[[nodiscard]] constexpr auto Scheduler::get_next_event_cycles() const -> s32
{
    if (empty())
    {
        return 0;
    }
    return queue[0].time;
}

/*
EXAMPLE: how to keep track of delta

struct DeltaManager
{
    s32 deltas[ID::END]{};

    constexpr void reset()
    {
        for (auto& delta : deltas) { delta = 0; }
    }

    constexpr void add(s32 id, s32 delta)
    {
        deltas[id] = delta;
    }

    constexpr void remove(s32 id)
    {
        deltas[id] = 0;
    }

    [[nodiscard]] constexpr auto get(s32 id, s32 time) -> s32
    {
        return time + deltas[id];
    }
};

DeltaManager deltas;

void event_callback(void* user, s32 id, s32 late)
{
    deltas.add(id, late);
    // ... do stuff here ...
    scheduler.add(id, deltas.get(id, 100), event_callback, user);
}

void on_disable_apu()
{
    deltas.remove(ID::APU);
    scheduler.remove(ID::APU);
}

*/

/*
EXAMPLE: how to implement save/load state

enum ID {
    PPU,
    APU,
    TIMER,
    DMA,
    MAX,
};

static_assert(ID::MAX < scheduler::RESERVED_ID);

struct EventEntry {
    std::int32_t enabled; // don't use bool here because padding!
    std::int32_t time;
};

void savestate()
{
    std::array<EventEntry, ID::MAX> events{};
    s32 scheduler_cycles = scheduler.get_ticks();

    for (std::size_t i = 0; i < events.size(); i++)
    {
        // see if we have this event in queue, if we do, it's enabled
        if (scheduler.has_event(i))
        {
            events[i].enabled = true;
            events[i].cycles = scheduler.get_event_cycles(i);
        }
    }

    // write state data
    write(events.data(), events.size());
    write(&scheduler_cycles, sizeof(scheduler_cycles));
}

void loadstate()
{
    std::array<EventEntry, ID::MAX> events;
    s32 scheduler_cycles;

    // read state data
    read(&scheduler_cycles, sizeof(scheduler_cycles));
    read(events.data(), events.size());

    // need to reset scheduler to remove all events and reset
    // to the saved time.
    scheduler.reset(scheduler_cycles);

    for (std::size_t i = 0; i < events.size(); i++)
    {
        if (events[i].enabled)
        {
            scheduler.add_absolute(i, events[i].cycles, callback);
        }
    }
}
*/

} // namespace scheduler
