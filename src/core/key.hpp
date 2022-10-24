// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

namespace gba::key {

// this should be called whenever REG_KEY REG_KEYCNT is modified!
STATIC void check_key_interrupt(Gba& gba);
// should be called whenever the frontend wants to change REG_KEY
STATIC void set_key(Gba& gba, u16 buttons, bool down);

} // namespace gba::key
