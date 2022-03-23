// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::backup::sram
{

struct Sram
{
    uint8_t data[0x8000];
};

} // namespace gba::backup::sram
