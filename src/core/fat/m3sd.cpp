// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "fat/fat.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include "m3sd.hpp"

#include <cstdio>
#include <cassert>
#include <cstring>

namespace gba::fat::m3sd {
namespace {

enum RegAddr
{
    DIR = 0x08800000, // direction control register
    DAT = 0x09000000, // SD data line, 8 bits at a time
    CMD = 0x09200000, // SD command byte
    ARGH = 0x09400000, // SD command argument, high halfword
    ARGL = 0x09600000, // SD command argument, low halfword
    STS = 0x09800000, // command and status register
};

enum Commands
{
    SD_GO_IDLE_STATE = 0,
    SD_ALL_SEND_CID = 2,
    SD_SEND_RELATIVE_ADDR = 3,
    SD_SELECT_CARD = 7,
    SD_SEND_CSD = 9,
    SD_STOP_TRANSMISSION = 12,
    SD_SEND_STATUS = 13,
    SD_GO_INACTIVE_STATE = 15,
    SD_SET_BLOCKLEN = 16,
    SD_READ_SINGLE_BLOCK = 17,
    SD_READ_MULTIPLE_BLOCK = 18,
    SD_WRITE_BLOCK = 24,
    SD_WRITE_MULTIPLE_BLOCK = 25,
    SD_APP_CMD = 55,
};

} // namespace

void M3sd::init(Gba& gba)
{
    REG_DIR = 0;
    REG_DAT = 0;
    REG_CMD = 0;
    REG_ARGH = 0;
    REG_ARGL = 0;
    REG_STS = 0;
}

auto M3sd::read(Gba& gba, u32 addr) -> u32
{
    switch (addr)
    {
        case RegAddr::DIR:
            assert(!"unhandled write to DIR");
            break;

        case RegAddr::DAT:
            assert(!"unhandled write to DAT");
            break;

        case RegAddr::CMD:
            assert(!"unhandled write to CMD");
            break;

        case RegAddr::ARGH:
            assert(!"unhandled write to ARGH");
            break;

        case RegAddr::ARGL:
            assert(!"unhandled write to ARGL");
            break;

        case RegAddr::STS:
            assert(!"unhandled write to STS");
            break;

        default:
            return fat::UNHANDLED_READ;
    }

    return fat::UNHANDLED_READ;
}

void M3sd::write(Gba& gba, u32 addr, u16 value)
{
    switch (addr)
    {
        case RegAddr::DIR:
            assert(!"unhandled write to DIR");
            break;

        case RegAddr::DAT:
            assert(!"unhandled write to DAT");
            break;

        case RegAddr::CMD:
            assert(!"unhandled write to CMD");
            break;

        case RegAddr::ARGH:
            assert(!"unhandled write to ARGH");
            break;

        case RegAddr::ARGL:
            assert(!"unhandled write to ARGL");
            break;

        case RegAddr::STS:
            assert(!"unhandled write to STS");
            break;

        default:
            std::printf("[M3SD] invalid write to 0x%08X value: 0x%04X\n", addr, value);
            assert(0);
            break;
    }
}

} // namespace gba::fat::m3sd
