// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include "rtc.hpp"

namespace gba::gpio {

struct Gpio
{
    rtc::Rtc rtc;

    u8 data; // 4 bit
    u8 read_mask;
    u8 write_mask;
    bool rw;
};

auto reset(Gba& gba, bool skip_bios) -> void;

} // namespace gba::gpio
