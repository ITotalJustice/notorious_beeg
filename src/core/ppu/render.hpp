// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::ppu {

auto render(Gba& gba) -> void;
auto start_thread(Gba& gba) -> void;

} // namespace gba::ppu
