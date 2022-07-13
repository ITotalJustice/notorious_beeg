// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::rtc {

enum class State : u8
{
    // waits until CS=0 and SCK=1
    init1,

    // waits until CS=1 and SCK=1 (CS rises)
    init2,

    // waits until 8 bits have been transfered
    command,

    // idk about these yet
    read,
    write,
};

enum class Command : u8
{
    reset = 0,
    control = 1,
    date = 2,
    time = 3,
    alarm1 = 4,
    alarm2 = 5,
    irq = 6,
    unused = 7,
};

struct Rtc
{
    u64 bits; // bits shifted in / out
    u8 bit_counter; // bit position
    bool pending_bit; // pending bit that's been written

    u8 control{0b0100'0000}; // enable 24h, thats it

    State state{State::init1};
    Command command;

    auto write(Gba& gba, u32 addr, u8 value) -> void;
};

} // namespace gba::rtc
