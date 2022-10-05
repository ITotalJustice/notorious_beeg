// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

// SOURCE:
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_m3cf.c
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_cf_common.c
namespace gba::fat::m3cf {

struct M3cf
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

    u32 mode;
    u8 mode_counter;

    void init(Gba& gba);
    auto read(Gba& gba, u32 addr) -> u32;
    void write(Gba& gba, u32 addr, u16 value);

private:
    [[nodiscard]] auto get_sector_offset() const -> u64;
    void set_mode(u32 new_mode);
};

} // namespace gba::fat::m3cf
