// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "fat/fat.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include "mpcf.hpp"
#include "log.hpp"

#include <cstdio>
#include <cassert>
#include <cstring>

namespace gba::fat::mpcf {
namespace {

enum RegAddr
{
    STS = 0x098C0000, // Status of the CF Card / Device control
    CMD = 0x090E0000, // Commands sent to control chip and status return
    ERR = 0x09020000, // Errors / Features

    SEC = 0x09040000, // Number of sector to transfer
    LBA1 = 0x09060000, // 1st byte of sector address
    LBA2 = 0x09080000, // 2nd byte of sector address
    LBA3 = 0x090A0000, // 3rd byte of sector address
    LBA4 = 0x090C0000, // last nibble of sector address | 0xE0

    DATA = 0x09000000, // Pointer to buffer of CF data transered from card
};

enum Commands
{
    // CF Card status
    CF_STS_INSERTED = 0x50,
    CF_STS_REMOVED = 0x00,
    CF_STS_READY = 0x58,

    CF_STS_DRQ = 0x08,
    CF_STS_BUSY = 0x80,

    // CF Card commands
    CF_CMD_LBA = 0xE0,
    CF_CMD_READ = 0x20,
    CF_CMD_WRITE = 0x30,

    CF_CARD_TIMEOUT = 10000000,
};

} // namespace

// call tree:
// https://github.com/devkitPro/libfat/blob/69543f058c10c90cddf3b3243819fa1953e3ea9e/source/libfat.c#L160
// https://github.com/devkitPro/libfat/blob/69543f058c10c90cddf3b3243819fa1953e3ea9e/source/libfat.c#L100
// https://github.com/devkitPro/libfat/blob/69543f058c10c90cddf3b3243819fa1953e3ea9e/source/partition.c#L187
// https://github.com/devkitPro/libgba/blob/a7b03d614aa5d31b1dd36f4f21fb8d4286017f94/src/disc_io/io_cf_common.c#L127

auto Mpcf::get_sector_offset() const -> u64
{
    const u64 offset = ((REG_LBA4 & 0xF) << 24) | (REG_LBA3 << 16) | (REG_LBA2 << 8) | (REG_LBA1 << 0);
    return offset * fat::SECTOR_SIZE;
}

void Mpcf::init(Gba& gba)
{
    reset(gba);
}

void Mpcf::reset(Gba& gba)
{
    sector_offset = 0;
    REG_DATA = 0;
    REG_STS = 0;
    REG_CMD = 0;
    REG_ERR = 0;
    REG_SEC = 0;
    REG_LBA1 = 0;
    REG_LBA2 = 0;
    REG_LBA3 = 0;
    REG_LBA4 = 0;
}

auto Mpcf::read(Gba& gba, u32 addr, bool& handled) -> u16
{
    handled = true;

    switch (addr)
    {
        case RegAddr::STS:
            return REG_STS;

        case RegAddr::CMD:
            return REG_CMD;

        case RegAddr::ERR:
            assert(!"unhandled read from REG_MPCF_ERR");
            break;

        case RegAddr::SEC:
            assert(!"unhandled read from REG_MPCF_SEC");
            return REG_STS;

        case RegAddr::LBA1:
            return REG_LBA1;

        case RegAddr::LBA2:
            return REG_LBA2;

        case RegAddr::LBA3:
            return REG_LBA3;

        case RegAddr::LBA4:
            return REG_LBA4;

        case RegAddr::DATA:
        {
            assert(REG_CMD == CF_CMD_READ);
            const auto result = fat::read16(gba, sector_offset);
            sector_offset += 2;
            return result;
        }

        default:
            handled = false;
            return 0x0;
    }
}

void Mpcf::write(Gba& gba, u32 addr, u16 value, bool& handled)
{
    handled = true;

    switch (addr)
    {
        case RegAddr::STS:
            switch (value)
            {
                case CF_STS_INSERTED:
                    REG_STS = CF_STS_INSERTED;
                    break;

                case CF_STS_REMOVED:
                    assert(!"unhandeld CF_STS_REMOVED");
                    break;

                case CF_STS_READY:
                    assert(!"unhandeld CF_STS_READY");
                    REG_STS = CF_STS_READY;
                    break;

                case CF_STS_DRQ:
                    assert(!"unhandeld CF_STS_DRQ");
                    break;

                case CF_STS_BUSY:
                    assert(!"unhandeld CF_STS_BUSY");
                    REG_STS = CF_STS_READY;
                    break;

                default:
                    log::print_error(gba, log::Type::MPCF, "[MPCF] invalid status command: 0x%02X\n", value);
                    assert(0);
                    break;
            }
            break;

        case RegAddr::CMD:
            REG_CMD = value;

            switch (REG_CMD)
            {
                case CF_CMD_LBA:
                    assert(!"unhandled CF_CMD_LBA");
                    REG_STS = CF_STS_READY;
                    break;

                case CF_CMD_READ:
                    REG_STS = CF_STS_READY;
                    sector_offset = get_sector_offset();
                    break;

                case CF_CMD_WRITE:
                    REG_STS = CF_STS_READY;
                    sector_offset = get_sector_offset();
                    break;

                default:
                    log::print_error(gba, log::Type::MPCF, "[MPCF] invalid CF command: 0x%02X\n", value);
                    assert(0);
                    break;
            }
            break;

        case RegAddr::ERR:
            assert(!"unhandled write to REG_MPCF_ERR");
            break;

        case RegAddr::SEC:
            log::print_info(gba, log::Type::MPCF, "[MPCF] number of sectors: %u %u\n", value, REG_SEC * fat::SECTOR_SIZE);
            REG_SEC = value;
            assert(REG_SEC > 0 && "impossible value, 0 should be set to 256!!!");
            break;

        case RegAddr::LBA1:
            REG_LBA1 = value;
            break;

        case RegAddr::LBA2:
            REG_LBA2 = value;
            break;

        case RegAddr::LBA3:
            REG_LBA3 = value;
            break;

        case RegAddr::LBA4:
            REG_LBA4 = value;
            break;

        case RegAddr::DATA:
            assert(REG_CMD == CF_CMD_WRITE);
            fat::write16(gba, sector_offset, value);
            sector_offset += 2;

            // flush on every sector write
            if (sector_offset == get_sector_offset() + REG_SEC * fat::SECTOR_SIZE)
            {
                fat::flush(gba, get_sector_offset(), REG_SEC * fat::SECTOR_SIZE);
                log::print_info(gba, log::Type::MPCF, "[MPCF] dumping file offset: %zu size: %u\n", get_sector_offset(), REG_SEC * fat::SECTOR_SIZE);
            }
            break;

        default:
            log::print_error(gba, log::Type::MPCF, "invalid write to 0x%08X value: 0x%04X\n", addr, value);
            assert(0);
            handled = false;
            break;
    }
}

} // namespace gba::fat::mpcf
