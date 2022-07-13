// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include "rtc.hpp"

namespace gba::gpio {

struct Gpio
{
    rtc::Rtc rtc;

    u8 data{}; // 4 bit
    u8 read_mask{0b0000};
    u8 write_mask{0b111};
    bool rw{false};
};

} // namespace gba::gpio
