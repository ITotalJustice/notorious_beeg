// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::ppu {

auto render(Gba& gba) -> void;

// memset line. max value is 159
void clear_line(Gba& gba, u16 line);
// memset entire screen 0...159
void clear_screen(Gba& gba);

} // namespace gba::ppu
