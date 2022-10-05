// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "gba.hpp"
#include "mem.hpp"
#include "m3cf.hpp"

#include <cstdio>
#include <cassert>
#include <cstring>

namespace gba::fat::m3cf {
namespace {

enum RegAddr
{
    STS = 0x080C0000, // Status of the CF Card / Device control
    CMD = 0x088E0000, // Commands sent to control chip and status return
    ERR = 0x08820000, // Errors / Features

    SEC = 0x08840000, // Number of sector to transfer
    LBA1 = 0x08860000, // 1st byte of sector address
    LBA2 = 0x08880000, // 2nd byte of sector address
    LBA3 = 0x088A0000, // 3rd byte of sector address
    LBA4 = 0x088C0000, // last nibble of sector address | 0xE0

    DATA = 0x08800000, // Pointer to buffer of CF data transered from card
};

enum Mode
{
    MODE_ROM = 0x00400004,
    MODE_MEDIA = 0x00400003,
};

enum ModeChangeAddr
{
    SEQ_0 = 0x08e00002,
    SEQ_1 = 0x0800000e,
    SEQ_2 = 0x08801ffc,
    SEQ_3 = 0x0800104a,
    SEQ_4 = 0x08800612,
    SEQ_5 = 0x08000000,
    SEQ_6 = 0x08801b66,

    SEQ_7_MODE_MEDIA = 0x08000000 + (MODE_MEDIA << 1),
    SEQ_7_MODE_ROM = 0x08000000 + (MODE_ROM << 1),

    SEQ_8 = 0x0800080e,
    SEQ_9 = 0x08000000,

    SEQ_10_MEDIA = 0x09000000,

    SEQ_10_ROM = 0x080001e4,
    SEQ_11_ROM = 0x080001e4,
    SEQ_12_ROM = 0x08000188,
    SEQ_13_ROM = 0x08000188,
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

auto M3cf::get_sector_offset() const -> u64
{
    const u64 offset = ((REG_LBA4 & 0xF) << 24) | (REG_LBA3 << 16) | (REG_LBA2 << 8) | (REG_LBA1 << 0);
    return offset * SECTOR_SIZE;
}

void M3cf::set_mode(u32 new_mode)
{
    mode_counter = 0;
    mode = new_mode;
    std::printf("[M3CF] new mode: 0x%04X\n", mode);
}

void M3cf::init(Gba& gba)
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

auto M3cf::read(Gba& gba, u32 addr) -> u32
{
    switch (addr)
    {
        case RegAddr::STS:
            return REG_STS;

        case RegAddr::CMD:
            return REG_CMD;

        case RegAddr::ERR:
            assert(!"unhandled read from REG_M3CF_ERR");
            break;

        case RegAddr::SEC:
            assert(!"unhandled read from REG_M3CF_SEC");
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
            const auto result = read16(gba, sector_offset);
            sector_offset += 2;
            return result;
        }

        default:
            // mode switching code, not important to emulate but might as well
            // SOURCE: https://github.com/devkitPro/libgba/blob/db9b8e6dd3b257b3cf3841d71c332f3a262e6cd5/src/disc_io/io_m3_common.c#L39
            if (addr == SEQ_0 && mode_counter == 0) { mode_counter++; }
            else if (addr == SEQ_1 && mode_counter == 1) { mode_counter++; }
            else if (addr == SEQ_2 && mode_counter == 2) { mode_counter++; }
            else if (addr == SEQ_3 && mode_counter == 3) { mode_counter++; }
            else if (addr == SEQ_4 && mode_counter == 4) { mode_counter++; }
            else if (addr == SEQ_5 && mode_counter == 5) { mode_counter++; }
            else if (addr == SEQ_6 && mode_counter == 6) { mode_counter++; }
            else if (addr == SEQ_7_MODE_MEDIA && mode_counter == 7) { mode_counter++; }
            else if (addr == SEQ_7_MODE_ROM && mode_counter == 7) { mode_counter++; }
            else if (addr == SEQ_8 && mode_counter == 8) { mode_counter++; }
            else if (addr == SEQ_9 && mode_counter == 9) { mode_counter++; }
            else if (addr == SEQ_10_MEDIA && mode_counter == 10) { set_mode(MODE_MEDIA); }
            else if (addr == SEQ_10_ROM && mode_counter == 10) { mode_counter++; }
            else if (addr == SEQ_11_ROM && mode_counter == 11) { mode_counter++; }
            else if (addr == SEQ_12_ROM && mode_counter == 12) { mode_counter++; }
            else if (addr == SEQ_13_ROM && mode_counter == 13) { set_mode(MODE_ROM); }
            else { mode_counter = 0; }

            return UNHANDLED_READ;
    }
}

void M3cf::write(Gba& gba, u32 addr, u16 value)
{
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
                    std::printf("invalid status command: 0x%02X\n", value);
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
                    std::printf("invalid CF command: 0x%02X\n", value);
                    assert(0);
                    break;
            }
            break;

        case RegAddr::ERR:
            assert(!"unhandled write to REG_M3CF_ERR");
            break;

        case RegAddr::SEC:
            std::printf("[M3CF] number of sectors: %u %u\n", value, REG_SEC * SECTOR_SIZE);
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
            write16(gba, sector_offset, value);
            sector_offset += 2;

            // flush on every sector write
            if (sector_offset == get_sector_offset() + REG_SEC * SECTOR_SIZE)
            {
                flush(gba, get_sector_offset(), REG_SEC * SECTOR_SIZE);
                std::printf("[M3CF] dumping file offset: %zu size: %u\n", get_sector_offset(), REG_SEC * SECTOR_SIZE);
            }
            break;

        default:
            // std::printf("invalid write to 0x%08X value: 0x%04X\n", addr, value);
            // assert(0);
            break;
    }
}

} // namespace gba::fat::m3cf
