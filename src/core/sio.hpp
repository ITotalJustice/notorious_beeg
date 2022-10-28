// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

namespace gba::sio {

enum class Mode
{
    Normal_8bit,
    Normal_32bit,
    MultiPlayer,
    UART,
    JOY_BUS,
    General,
};

void on_rcnt_write(Gba& gba, u16 value);
void on_siocnt_write(Gba& gba, u16 value);

auto get_mode(Gba& gba) -> Mode;
auto get_mode_str(Mode mode) -> const char*;

} // namespace gba::sio
