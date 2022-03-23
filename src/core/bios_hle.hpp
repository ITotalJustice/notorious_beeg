// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "fwd.hpp"
#include <cstdint>

namespace gba::bios {

// returns true if handled
auto hle(Gba& gba, std::uint8_t comment_field) -> bool;

} // namespace gba::bios
