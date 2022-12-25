// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

// SOURCE:
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_scsd.c
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_sd_common.c
namespace gba::fat::scsd {

struct Scsd
{
    u8 REG_CMD;
    u8 REG_DATAWRITE;
    u8 REG_DATAREAD;
    u8 REG_LITE_ENABLE;
    u8 REG_LOCK;
};

} // namespace gba::fat::scsd
