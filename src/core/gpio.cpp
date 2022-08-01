// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "gpio.hpp"
#include "gba.hpp"

namespace gba::gpio {

auto reset(Gba& gba, [[maybe_unused]] bool skip_bios) -> void
{
    gba.gpio.rtc.init();
    gba.gpio.data = 0;
    gba.gpio.read_mask = 0b0000;
    gba.gpio.write_mask = 0b1111;
    gba.gpio.rw = false;
}

// 1.4 MiB (1,500,920)
} // namespace gba::gpio
