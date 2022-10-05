// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "fat/fat.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include "scsd.hpp"

#include <cstdio>
#include <cassert>
#include <cstring>

namespace gba::fat::scsd {
namespace {

enum RegAddr
{
    CMD = 0x09800000,
    DATAWRITE = 0x09000000,
    DATAREAD = 0x09100000,
    LITE_ENABLE = 0x09440000,
    LOCK = 0x09FFFFFE,
};

enum Response
{
    STS_BUSY = 0x100,
    STS_INSERTED = 0x300,
};

} // namespace

} // namespace gba::fat::scsd
