// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

// SOURCE:
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_m3sd.c
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_sd_common.c
namespace gba::fat::m3sd {

struct M3sd
{
    u16 REG_DIR;
    u16 REG_DAT;
    u16 REG_CMD;
    u16 REG_ARGH;
    u16 REG_ARGL;
    u16 REG_STS;

    void init(Gba& gba);
    void reset(Gba& gba);

    auto read(Gba& gba, u32 addr, bool& handled) -> u16;
    void write(Gba& gba, u32 addr, u16 value, bool& handled);

private:
    [[nodiscard]] auto get_sector_offset() const -> u64;
};

} // namespace gba::fat::m3sd
