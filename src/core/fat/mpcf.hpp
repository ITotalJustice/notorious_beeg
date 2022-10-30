// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

// SOURCE:
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_mpcf.c
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_cf_common.c
namespace gba::fat::mpcf {

struct Mpcf
{
    u64 sector_offset;

    u8 REG_DATA;
    u8 REG_STS;
    u8 REG_CMD;
    u8 REG_ERR;
    u8 REG_SEC;
    u8 REG_LBA1;
    u8 REG_LBA2;
    u8 REG_LBA3;
    u8 REG_LBA4;

    void init(Gba& gba);
    void reset(Gba& gba);

    auto read(Gba& gba, u32 addr, bool& handled) -> u16;
    void write(Gba& gba, u32 addr, u16 value, bool& handled);

private:
    [[nodiscard]] auto get_sector_offset() const -> u64;
};

} // namespace gba::fat::mpcf
