// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fat/ezflash.hpp"
#include "fat/m3cf.hpp"
#include "fat/mpcf.hpp"
#include "fat/sccf.hpp"
#include "fwd.hpp"
#include <span>

// SOURCE:
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_mpcf.c
// https://github.com/devkitPro/libgba/blob/master/src/disc_io/io_cf_common.h
namespace gba::fat {

enum class Type : u8
{
    NONE,
    MPCF,
    M3CF,
    SCCF,
    EZFLASH,
    EZFLASH_DE,
};

enum : u32 { SECTOR_SIZE = 512 };

struct Device
{
    mpcf::Mpcf* mpcf{};
    m3cf::M3cf* m3cf{};
    sccf::Sccf* sccf{};
    ezflash::Ezflash* ezflash{};

    Type type{Type::NONE};
};

void init(Gba& gba, Type new_type);
void reset(Gba& gba);

// image must be 512MB (for now)
auto create_image(std::span<u8> data) -> bool;

auto flush(Gba& gba, u64 offset, u64 size) -> bool;

auto read16(Gba& gba, u64 addr) -> u16;
void write16(Gba& gba, u64 addr, u16 value);

auto get_type_str() -> std::span<const char*>;

} // namespace gba::fat
