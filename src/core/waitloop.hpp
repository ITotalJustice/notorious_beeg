// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// SEE: https://github.com/ITotalJustice/notorious_beeg/issues/103
#pragma once

#include "fwd.hpp"

namespace gba::waitloop {

enum WaitloopEvent
{
    WAITLOOP_EVENT_IRQ,
    WAITLOOP_EVENT_DMA,
    WAITLOOP_EVENT_IO,
};

enum WaitloopStep
{
    // checks that the loop is read only and only
    WAITLOOP_STEP_1,
    // checks the registers that were saved in step1.
    // if the registers changed, then this loop cannot be skipped.
    WAITLOOP_STEP_2,
    // this loop cannot be skipped
    WAITLOOP_STEP_INVALID,
};

struct Waitloop
{
public:
    // call this on startup or to disable waitloop
    void reset(Gba& gba, bool enable);
    // call this on conditional branches
    void on_thumb_loop(Gba& gba, u32 current_pc, u32 new_jump_pc);
    // call this whenever an event changes
    void on_event_change(Gba& gba, WaitloopEvent event, u32 addr_start = 0, u32 addr_end = 0);
    // returns master enable flag
    [[nodiscard]] auto is_enabled() const -> bool { return enabled; }
    // returns true if currently within a waitloop
    [[nodiscard]] auto is_in_waitloop() const -> bool { return in_waitloop; }

private:
    // the new pc of thumb conditional branch
    u32 pc;
    // the address that is polled
    u32 poll_address;
    // saved registers need for step2
    u32 wait_loop_registers[15];
    // see WaitloopStep
    u8 step;
    // set to true whilst in a waitloop, cleared on_event_change().
    bool in_waitloop;
    // set to true on on_event_change(), cleared in evaluate_loop_step2().
    bool event_changed;
    // master enable flag, if false, no loop skipp will occur
    bool enabled;

    auto evaluate_loop_step1(Gba& gba, u32 current_pc, u32 new_jump_pc) -> bool;
    auto evaluate_loop_step2(Gba& gba, u32 current_pc, u32 new_jump_pc) -> bool;
    void evaluate_loop(Gba& gba, u32 current_pc, u32 new_jump_pc);
};

} // namespace gba::arm7tdmi::waitloop
