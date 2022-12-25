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

enum : s32 { RESERVED_ID = 0x7FFFFFFF };
enum : s32 { TIMEOUT_VALUE = 0x70000000 };

struct Event {
    s32 time; // time until event expires (scheduler.cycles + event.cycle)
    s32 id; // event id
    Callback callback; // function to call on event expire
    void* user; // user data passed to the callback

    // used for std::_heap functions
    auto operator>(const Event& rhs) const -> bool { return time > rhs.time; }
};

struct Scheduler {
public:
    // calls reset()
    Scheduler(s32 starting_cycles = 0, Callback reset_cb = nullptr)
    {
        reset(starting_cycles, reset_cb);
    }

    // resets queue and cycles, adds reset event, optional custom callback
    void reset(s32 starting_cycles = 0, Callback reset_cb = nullptr)
    {
        queue.clear();
        cycles = std::min<s32>(starting_cycles, TIMEOUT_VALUE);
        add_absolute(RESERVED_ID, TIMEOUT_VALUE, reset_cb ? reset_cb : reset_event, this);
    }

    // fires all expired events
    void fire()
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

    // adds relative new / existing event. updates time,cb,user if existing
    void add(s32 id, s32 event_time, Callback cb, void* user)
    {
        add_absolute(id, cycles + event_time, cb, user);
    }

    // adds new / existing event. updates time,cb,user if existing
    void add_absolute(s32 id, s32 event_time, Callback cb, void* user)
    {
        const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });

        // if event if already in queue then update time, cb and user.
        if (itr != queue.end())
        {
            // fast path if front of queue
            if (itr == queue.begin())
            {
                std::pop_heap(queue.begin(), queue.end(), std::greater<>());
                queue.back().time = event_time;
                queue.back().callback = cb;
                queue.back().user = user;
                std::push_heap(queue.begin(), queue.end(), std::greater<>());
            }
            // otherwise, sadly we have to sort the queue :/
            else
            {
                itr->time = event_time;
                itr->callback = cb;
                itr->user = user;
                std::make_heap(queue.begin(), queue.end(), std::greater<>());
            }
        }
        // otherwise create new event
        else
        {
            queue.emplace_back(Event{event_time, id, cb, user});
            std::push_heap(queue.begin(), queue.end(), std::greater<>());
        }
    }

    // removes an event, does nothing if event not enabled.
    void remove(s32 id)
    {
        const auto itr = std::remove_if(queue.begin(), queue.end(), [id](auto& e) { return id == e.id; });

        if (itr != queue.end())
        {
            queue.erase(itr, queue.end());
            std::make_heap(queue.begin(), queue.end(), std::greater<>()); // optimise this
        }
    }

    // advance scheduler by number of ticks
    void tick(s32 ticks)
    {
        cycles += ticks;
    }

    // returns current time of the scheduler
    [[nodiscard]] auto get_ticks() const -> s32
    {
        return cycles;
    }

    // returns true if there are no events
    [[nodiscard]] auto empty() const -> bool
    {
        #if SCHEDULER_NEVER_EMPTY
        return false;
        #else
        return queue.empty();
        #endif
    }

    // return true if fire() should be called
    [[nodiscard]] auto should_fire() const -> bool
    {
        if (empty())
        {
            return false;
        }
        return queue.front().time <= cycles;
    }

    // returns if an event is found with matching id
    [[nodiscard]] auto has_event(s32 id) const -> bool
    {
        const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });
        return itr != queue.end();
    }

    // returns event cycles - get_ticks() or 0 if not found
    [[nodiscard]] auto get_event_cycles(s32 id) const -> s32
    {
        const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });
        if (itr == queue.end())
        {
            return 0;
        }
        return itr->time - get_ticks();
    }

    // returns event cycles or 0 if not found
    [[nodiscard]] auto get_event_cycles_absolute(s32 id) const -> s32
    {
        const auto itr = std::find_if(queue.begin(), queue.end(), [id](auto& e){ return id == e.id; });
        if (itr == queue.end())
        {
            return 0;
        }
        return itr->time;
    }

    // return cycles - get_ticks() of next event or 0 if no events
    [[nodiscard]] auto get_next_event_cycles() const -> s32
    {
        if (empty())
        {
            return 0;
        }
        return queue.front().time - get_ticks();
    }

    // return cycles of next event or 0 if no events
    [[nodiscard]] auto get_next_event_cycles_absolute() const -> s32
    {
        if (empty())
        {
            return 0;
        }
        return queue.front().time;
    }

    // advances scheduler so that get_ticks() == get_next_event_cycles() if event has greater cycles
    void advance_to_next_event()
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

private:
    // default reset event
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

} // namespace scheduler
