// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// sudo mount -o defaults,umask=000 sd.raw sd_mount/
#include "arm7tdmi/arm7tdmi.hpp"
#include "backup/backup.hpp"
#include "backup/eeprom.hpp"
#include "backup/flash.hpp"
#include "bit.hpp"
#include "fat/fat.hpp"
#include "gba.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "ezflash.hpp"

#include <cassert>
#include <cstring>
#include <utility>

namespace gba::fat::ezflash {
namespace {

enum NorS71InfoOffset
{
    NOR_S71_INFO_OFFSET_SAVE = 0x790000,
    NOR_S71_INFO_OFFSET_NOR = 0x7A0000,
    NOR_S71_INFO_OFFSET_SET = 0x7B0000,
};

enum NorS71SetInfo
{
    NOR_S71_SET_INFO_LANG = NOR_S71_INFO_OFFSET_SET + (0 * 2),
    NOR_S71_SET_INFO_RESET = NOR_S71_INFO_OFFSET_SET + (1 * 2),
    NOR_S71_SET_INFO_RTS = NOR_S71_INFO_OFFSET_SET + (2 * 2),
    NOR_S71_SET_INFO_SLEEP = NOR_S71_INFO_OFFSET_SET + (3 * 2),
    NOR_S71_SET_INFO_CHEAT = NOR_S71_INFO_OFFSET_SET + (4 * 2),
    NOR_S71_SET_INFO_SLEEP_KEY0 = NOR_S71_INFO_OFFSET_SET + (5 * 2),
    NOR_S71_SET_INFO_SLEEP_KEY1 = NOR_S71_INFO_OFFSET_SET + (6 * 2),
    NOR_S71_SET_INFO_SLEEP_KEY2 = NOR_S71_INFO_OFFSET_SET + (7 * 2),
    NOR_S71_SET_INFO_MENU_KEY0 = NOR_S71_INFO_OFFSET_SET + (8 * 2),
    NOR_S71_SET_INFO_MENU_KEY1 = NOR_S71_INFO_OFFSET_SET + (9 * 2),
    NOR_S71_SET_INFO_MENU_KEY2 = NOR_S71_INFO_OFFSET_SET + (10 * 2),
    NOR_S71_SET_INFO_ENGINE = NOR_S71_INFO_OFFSET_SET + (11 * 2),
    NOR_S71_SET_INFO_SHOW_THUMBNAIL = NOR_S71_INFO_OFFSET_SET + (12 * 2),
    NOR_S71_SET_INFO_RTC_OPEN_STATUS = NOR_S71_INFO_OFFSET_SET + (13 * 2),

    // DE setting
    NOR_S71_SET_INFO_AUTO_SAVE_SEL = NOR_S71_INFO_OFFSET_SET + (14 * 2),
    NOR_S71_SET_INFO_MODE_B_INIT = NOR_S71_INFO_OFFSET_SET + (15 * 2),
    NOR_S71_SET_INFO_LED_OPEN_SEL = NOR_S71_INFO_OFFSET_SET + (16 * 2),
    NOR_S71_SET_INFO_BREATHING_R = NOR_S71_INFO_OFFSET_SET + (17 * 2),
    NOR_S71_SET_INFO_BREATHING_G = NOR_S71_INFO_OFFSET_SET + (18 * 2),
    NOR_S71_SET_INFO_BREATHING_B = NOR_S71_INFO_OFFSET_SET + (19 * 2),
    NOR_S71_SET_INFO_SD_R = NOR_S71_INFO_OFFSET_SET + (20 * 2),
    NOR_S71_SET_INFO_SD_G = NOR_S71_INFO_OFFSET_SET + (21 * 2),
    NOR_S71_SET_INFO_SD_B = NOR_S71_INFO_OFFSET_SET + (22 * 2),
};

// some of these addr are labled as unknown but that's not entierly true
// i just don't know of an appropriate name to give them.
// most of the unk are part of a sequence to r/w to a device
// such as the sd card, fpga or spi.
enum RegAddr : u32
{
    UNK0 = 0x9FE0000, // writes always 0xD200
    UNK1 = 0x8000000, // writes always 0x1500
    UNK2 = 0x8020000, // writes always 0xD200
    UNK3 = 0x8040000, // writes always 0x1500

    // i think this is used to flush pending data to sd card
    UNK5 = 0x9FC0000, // writes always 0x1500

    SD_RESPONSE = 0x9E00000,

    SD_CONTROL = 0x9400000,
    BUFFER_CONTROL = 0x9420000, // (A1)
    ROMPAGE = 0x9880000, // (C4)
    PS_RAMPAGE = 0x9860000, // (C3)
    RAMPAGE = 0x9C00000, // (E0)
    SPI_CONTROL = 0x9660000,
    // !!!NEVER EVER MESS WITH THIS REGISTER!!!
    // enabling the write and then disabling bricked my ezflash
    // it is unrecoverable, no longer recognised by my ds.
    SPI_WRITE = 0x9680000,
    RTC_STATUS = 0x96A0000,
    AUTO_SAVE_MODE = 0x96C0000,

    // DE stuff
    LED_CONTROL = 0x96E0000,
    RUMBLE_CONTROL = 0x9E20000,
    ROM64_FLAG = 0x9700000, // ???

    SD_ADDR_LOW = 0x9600000, // low 16 bits
    SD_ADDR_HIGH = 0x9620000, // upper 16 bits
    SD_BLOCKS = 0x9640000, // number of blocks to transfer

    // this is used for many things depending on spi control
    // is spi control is enabled then this is used for fpga version
    // otherwise, it is used for ???
    FPGA_VER = 0x9E00000,

    FLASH_BASE_S71 = 0x08000000, // writes always 0xF0
    FLASH_BASE_S71_END = 0x087FFFFF,

    FLASH_BASE_S98 = 0x09000000, // writes always 0xF0
    FLASH_BASE_S98_END = 0x097FFFFF,

    SET_INFO_LANG = 0x08000000 + NOR_S71_SET_INFO_LANG,
    SET_INFO_RESET = 0x08000000 + NOR_S71_SET_INFO_RESET,
    SET_INFO_RTS = 0x08000000 + NOR_S71_SET_INFO_RTS,
    SET_INFO_SLEEP = 0x08000000 + NOR_S71_SET_INFO_SLEEP,
    SET_INFO_CHEAT = 0x08000000 + NOR_S71_SET_INFO_CHEAT,
    SET_INFO_SLEEP_KEY0 = 0x08000000 + NOR_S71_SET_INFO_SLEEP_KEY0,
    SET_INFO_SLEEP_KEY1 = 0x08000000 + NOR_S71_SET_INFO_SLEEP_KEY1,
    SET_INFO_SLEEP_KEY2 = 0x08000000 + NOR_S71_SET_INFO_SLEEP_KEY2,
    SET_INFO_MENU_KEY0 = 0x08000000 + NOR_S71_SET_INFO_MENU_KEY0,
    SET_INFO_MENU_KEY1 = 0x08000000 + NOR_S71_SET_INFO_MENU_KEY1,
    SET_INFO_MENU_KEY2 = 0x08000000 + NOR_S71_SET_INFO_MENU_KEY2,
    SET_INFO_ENGINE = 0x08000000 + NOR_S71_SET_INFO_ENGINE,
    SET_INFO_SHOW_THUMBNAIL = 0x08000000 + NOR_S71_SET_INFO_SHOW_THUMBNAIL,
    SET_INFO_RTC_OPEN_STATUS = 0x08000000 + NOR_S71_SET_INFO_RTC_OPEN_STATUS,

    // DE
    SET_INFO_AUTO_SAVE_SEL = 0x08000000 + NOR_S71_SET_INFO_AUTO_SAVE_SEL,
    SET_INFO_MODE_B_INIT = 0x08000000 + NOR_S71_SET_INFO_MODE_B_INIT,
    SET_INFO_LED_OPEN_SEL = 0x08000000 + NOR_S71_SET_INFO_LED_OPEN_SEL,
    SET_INFO_BREATHING_R = 0x08000000 + NOR_S71_SET_INFO_BREATHING_R,
    SET_INFO_BREATHING_G = 0x08000000 + NOR_S71_SET_INFO_BREATHING_G,
    SET_INFO_BREATHING_B = 0x08000000 + NOR_S71_SET_INFO_BREATHING_B,
    SET_INFO_SD_R = 0x08000000 + NOR_S71_SET_INFO_SD_R,
    SET_INFO_SD_G = 0x08000000 + NOR_S71_SET_INFO_SD_G,
    SET_INFO_SD_B = 0x08000000 + NOR_S71_SET_INFO_SD_B,

    RTC_DATA = 0x080000C4,
    RTC_RW = 0x080000C6,
    RTC_ENABLE = 0x080000C8,
    // this is unused in ezflash kernel
    // todo: debug on hw what the output of
    // polling this register is!
    RTC_CART_NAME = 0x080000A0,
};

enum SetInfoLang : u16
{
    SET_INFO_LANG_ENGLISH = 0xE1E1,
    SET_INFO_LANG_CHINEASE = 0xE2E2,
    SET_INFO_LANG_DEFAULT = SET_INFO_LANG_ENGLISH,
};

enum SetInfoReset : u16
{
    SET_INFO_RESET_DISABLE = 0x0,
    SET_INFO_RESET_ENABLE = 0x1,
    SET_INFO_RESET_DEFAULT = SET_INFO_RESET_DISABLE,
};

enum SetInfoRts : u16
{
    SET_INFO_RTS_DISABLE = 0x0,
    SET_INFO_RTS_ENABLE = 0x1,
    SET_INFO_RTS_DEFAULT = SET_INFO_RTS_DISABLE,
};

enum SetInfoSleep : u16
{
    SET_INFO_SLEEP_DISABLE = 0x0,
    SET_INFO_SLEEP_ENABLE = 0x1,
    SET_INFO_SLEEP_DEFAULT = SET_INFO_SLEEP_DISABLE,
};

enum SetInfoCheat : u16
{
    SET_INFO_CHEAT_DISABLE = 0x0,
    SET_INFO_CHEAT_ENABLE = 0x1,
    SET_INFO_CHEAT_DEFAULT = SET_INFO_CHEAT_DISABLE,
};

enum SetInfoKey : u16
{
    SET_INFO_KEY_A = 0,
    SET_INFO_KEY_B = 1,
    SET_INFO_KEY_SELECT = 2,
    SET_INFO_KEY_START = 3,
    SET_INFO_KEY_RIGHT = 4,
    SET_INFO_KEY_LEFT = 5,
    SET_INFO_KEY_UP = 6,
    SET_INFO_KEY_DOWN = 7,
    SET_INFO_KEY_R = 8,
    SET_INFO_KEY_L = 9,
};

enum SetInfoSleepKey0 : u16
{
    SET_INFO_SLEEP_KEY0_DEFAULT = SET_INFO_KEY_L,
};

enum SetInfoSleepKey1 : u16
{
    SET_INFO_SLEEP_KEY1_DEFAULT = SET_INFO_KEY_R,
};

enum SetInfoSleepKey2 : u16
{
    SET_INFO_SLEEP_KEY2_DEFAULT = SET_INFO_KEY_SELECT,
};

enum SetInfoMenuKey0 : u16
{
    SET_INFO_MENU_KEY0_DEFAULT = SET_INFO_KEY_L,
};

enum SetInfoMenuKey1 : u16
{
    SET_INFO_MENU_KEY1_DEFAULT = SET_INFO_KEY_R,
};

enum SetInfoMenuKey2 : u16
{
    SET_INFO_MENU_KEY2_DEFAULT = SET_INFO_KEY_START,
};

enum SetInfoEngine : u16
{
    SET_INFO_ENGINE_DISABLE = 0x0,
    SET_INFO_ENGINE_ENABLE = 0x1,
    SET_INFO_ENGINE_DEFAULT = SET_INFO_ENGINE_ENABLE,
};

enum SetInfoShowThumbnail : u16
{
    SET_INFO_SHOW_THUMBNAIL_DISABLE = 0x0,
    SET_INFO_SHOW_THUMBNAIL_ENABLE = 0x1,
    SET_INFO_SHOW_THUMBNAIL_DEFAULT = SET_INFO_SHOW_THUMBNAIL_DISABLE,
};

enum SetInfoRtcOpenStatus : u16
{
    SET_INFO_RTC_OPEN_STATUS_DISABLE = 0x0,
    SET_INFO_RTC_OPEN_STATUS_ENABLE = 0x1,
    SET_INFO_RTC_OPEN_STATUS_DEFAULT = SET_INFO_RTC_OPEN_STATUS_ENABLE,
};

enum SetInfoAutoSaveSel : u16
{
    SET_INFO_AUTO_SAVE_SEL_DISABLE = 0x0,
    SET_INFO_AUTO_SAVE_SEL_ENABLE = 0x1,
    SET_INFO_AUTO_SAVE_SEL_DEFAULT = SET_INFO_AUTO_SAVE_SEL_DISABLE,
};

enum SetInfoModeBInit : u16
{
    SET_INFO_MODE_B_INIT_RUMBLE = 0x0,
    SET_INFO_MODE_B_INIT_RAM = 0x1,
    SET_INFO_MODE_B_INIT_LINK = 0x2,
    SET_INFO_MODE_B_INIT_DEFAULT = SET_INFO_MODE_B_INIT_LINK,
};

enum SetInfoLedOpenSel : u16
{
    SET_INFO_LED_OPEN_SEL_DISABLE = 0x0,
    SET_INFO_LED_OPEN_SEL_ENABLE = 0x1,
    SET_INFO_LED_OPEN_SEL_DEFAULT = SET_INFO_LED_OPEN_SEL_ENABLE,
};

enum SetInfoBreathingR : u16
{
    SET_INFO_BREATHING_R_DISABLE = 0x0,
    SET_INFO_BREATHING_R_ENABLE = 0x1,
    SET_INFO_BREATHING_R_DEFAULT = SET_INFO_BREATHING_R_ENABLE,
};

enum SetInfoBreathingG : u16
{
    SET_INFO_BREATHING_G_DISABLE = 0x0,
    SET_INFO_BREATHING_G_ENABLE = 0x1,
    SET_INFO_BREATHING_G_DEFAULT = SET_INFO_BREATHING_G_ENABLE,
};

enum SetInfoBreathingB : u16
{
    SET_INFO_BREATHING_B_DISABLE = 0x0,
    SET_INFO_BREATHING_B_ENABLE = 0x1,
    SET_INFO_BREATHING_B_DEFAULT = SET_INFO_BREATHING_B_ENABLE,
};

enum SetInfoSdR : u16
{
    SET_INFO_SD_R_DISABLE = 0x0,
    SET_INFO_SD_R_ENABLE = 0x1,
    SET_INFO_SD_R_DEFAULT = SET_INFO_SD_R_DISABLE,
};

enum SetInfoSdG : u16
{
    SET_INFO_SD_G_DISABLE = 0x0,
    SET_INFO_SD_G_ENABLE = 0x1,
    SET_INFO_SD_G_DEFAULT = SET_INFO_SD_G_DISABLE,
};

enum SetInfoSdB : u16
{
    SET_INFO_SD_B_DISABLE = 0x0,
    SET_INFO_SD_B_ENABLE = 0x1,
    SET_INFO_SD_B_DEFAULT = SET_INFO_SD_B_DISABLE,
};

enum FpgaVersion : u16
{
    FPGA_VER0 = 0,
    FPGA_VER1 = 1,
    FPGA_VER2 = 2,
    FPGA_VER3 = 3,
    FPGA_VER4 = 4,
    FPGA_VER5 = 5,
    FPGA_VER6 = 6,
    FPGA_VER7 = 7,
    FPGA_VER8 = 8,
    FPGA_VER9 = 9,
    // this will always cause an update
    FPGA_VER_TEST = 99,

    FPGA_VER_MAX = FPGA_VER9,
};

enum DeFpgaVersion : u16
{
    DE_FPGA_VER0 = 0,
    DE_FPGA_VER1 = 1,
    DE_FPGA_VER2 = 2,
    DE_FPGA_VER3 = 3,
    DE_FPGA_VER4 = 4,
    // this will always cause an update
    DE_FPGA_VER_TEST = 99,

    DE_FPGA_VER_MAX = DE_FPGA_VER4,
};

enum FwCrc : u32
{
    FW3_CRC = 0x22475DDC,
    FW4_CRC = 0xEE2DACE7,
    FW5_CRC = 0x5B6B5129,
    FW6_CRC = 0x7E6212AB,
    FW7_CRC = 0xEFD03788,
    FW8_CRC = 0x02D2ED6B,
    FW9_CRC = 0xB23F6EAE,
};

enum DeFwCrc : u32
{
    DE_FW1_CRC = 0x480D0853,
    DE_FW2_CRC = 0xA07D712F,
    DE_FW3_CRC = 0x3DA3D970,
    DE_FW4_CRC = 0x76352215,
};

enum NorId : u16
{
    NOR_S71_ID = 0x2202, // PL064
    NOR_S98_ID = 0x223D, // S98
};

enum NorSize : u32
{
    NOR_S71_SIZE = 64 * 1024 * 1024,
    NOR_S98_SIZE = 512 * 1024 * 1024,
};

enum NorMaxGameSlots : u32
{
    NOR_S71_MAX_GAME_SLOTS = NOR_S71_SIZE / 0x400000,
    NOR_S98_MAX_GAME_SLOTS = NOR_S98_SIZE / 0x400000,
};

enum RomPage : u16
{
    // multiply the game number by this offset to get the
    // actual offset in the nor flash
    // for game0, set rompage to 0x0, for game2, set rompage to 0x80
    ROMPAGE_NOR_OFFSET = 0x0040,

    ROMPAGE_BOOTLOADER = 0x8000,
    ROMPAGE_KERNEL = 0x8002,

    // not sure if this is correct
    ROMPAGE_NOR_S71 = 0x8000,
};

enum PsRampage : u16
{
    PS_RAMPAGE_UNK0 = 0x0000,
    PS_RAMPAGE_UNK1 = 0x1000,
    PS_RAMPAGE_UNK2 = 0x2000,
    PS_RAMPAGE_UNK3 = 0x3000,
};

enum Rampage : u16
{
    RAMPAGE_SAVE1 = 0x00,
    RAMPAGE_SAVE2 = 0x10,
    // this is used when loading the ezflash in game menu
    // the contents of vram are copied to this region and
    // then read back into vram when leaving the menu
    RAMPAGE_VRAM_BUFFER1 = 0x20,
    RAMPAGE_VRAM_BUFFER2 = 0x30,

    RAMPAGE_RTS_EWRAM1 = 0x40,
    RAMPAGE_RTS_EWRAM2 = 0x50,
    RAMPAGE_RTS_EWRAM3 = 0x60,
    RAMPAGE_RTS_EWRAM4 = 0x70,

    RAMPAGE_RTS_IWRAM_PRAM = 0x80,

    RAMPAGE_RTS_VRAM1 = 0x90,
    RAMPAGE_RTS_VRAM2_OAM_R4R11_IO = 0xA0,
    RAMPAGE_RTS_UNK = 0xB0,

    RAMPAGE_RTS = 0x20,
    RAMPAGE_RTS_START = RAMPAGE_RTS_EWRAM1,
    // there's more banks here for rts
};

enum SdControl : u16
{
    SD_CONTROL_DISABLE = 0,
    SD_CONTROL_ENABLE = 1,
    SD_CONTROL_READ_STATE = 3,
};

enum SpiControl : u16
{
    SPI_CONTROL_DISABLE = 0,
    SPI_CONTROL_ENABLE = 1,
};

enum SpiWrite : u16
{
    SPI_WRITE_DISABLE = 0,
    SPI_WRITE_ENABLE = 1,
};

// this is a guess
enum BufferControl : u16
{
    BUFFER_CONTROL_DISABLE = 0,
    BUFFER_CONTROL_ENABLE = 1,
    BUFFER_CONTROL_READ_STATE = 3,
};

enum RtcStatus : u16
{
    RTC_STATUS_DISABLE = 0,
    RTC_STATUS_ENABLE = 1,
};

enum AutoSaveMode : u16
{
    AUTO_SAVE_MODE_DISABLE = 0,
    AUTO_SAVE_MODE_ENABLE = 1,
};

enum SdResponse : u16
{
    SD_RESPONSE_UN0 = 0x0000, // ok?
    SD_RESPONSE_UN1 = 0x0001, // busy? error?
    SD_RESPONSE_UN2 = 0xEEE1, // busy?
};

// all registers are 4 bytes wide
enum FatTable
{
    FAT_TABLE_UNK0 = 0x000, // always 0x00000000 (?), init to 0x00000000 on startup
    FAT_TABLE_START_CLUSTER_ROM = 0x004, // start of rom data cluster, init to 0xFFFFFFFF on startup
    FAT_TABLE_UNK2 = 0x008, // next rom cluster or 0xFFFFFFFF if non-fragment

    FAT_TABLE_GAMEFILE_SIZE = 0x1F0, // size of rom in bytes
    FAT_TABLE_MODE = 0x1F4, // SEE: FatTableMode
    FAT_TABLE_CLUSTER_SIZE = 0x1F8, // ???, usually 8
    FAT_TABLE_SAVEFILE_SIZE_AND_TYPE = 0x1FC, // size=24 bits, type=8bits

    // these are always set even if the game doesnt have save
    // todo: verify above with a game with mode=0x00
    FAT_TABLE_START_CLUSTER_SAVE = 0x204, // start of save data cluster
    FAT_TABLE_UNK9 = 0x208, // next save cluster or 0xFFFFFFFF if non-fragment

    // these fields are only set when the game has rts on disk to load
    // and the game is loaded with patches
    FAT_TABLE_START_CLUSTER_RTS = 0x304, // start of rts data cluster
    FAT_TABLE_UNK12 = 0x308, // next rts cluster or 0xFFFFFFFF if non-fragment
};

enum FatTableMode : u32
{
    // this is used when loading a rom into psram
    // psram is then eventually mapped intro rom region
    // ezflash then does a softreset swi
    FAT_TABLE_MODE_ROM_COPY_PSRAM = 0x1,

    // idk what this does yet, haven't checked
    FAT_TABLE_MODE_PARAMATER = 0x2,
};

enum RtsSize : u32
{
    RTS_SIZE = 458'752,
};

enum RumbleStrength : u8
{
    RUMBLE_STRENGTH_WEAK = 0xF0,
    RUMBLE_STRENGTH_MEDIUM = 0xF2,
    RUMBLE_STRENGTH_STRONG = 0xF1,
};

enum StartCommand : u32
{
    COMMAND_NONE = 0x0,

    COMMAND_SEQ1_ADDR = 0x9fe0000,
    COMMAND_SEQ2_ADDR = 0x8000000,
    COMMAND_SEQ3_ADDR = 0x8020000,
    COMMAND_SEQ4_ADDR = 0x8040000,
    COMMAND_SEQ5_ADDR = 0x9fc0000,

    COMMAND_SEQ1_VALUE = 0xD200,
    COMMAND_SEQ2_VALUE = 0x1500,
    COMMAND_SEQ3_VALUE = 0xD200,
    COMMAND_SEQ4_VALUE = 0x1500,
    COMMAND_SEQ5_VALUE = 0x1500,
};

enum FatTransferType
{
    SRC, // copy from fat
    DST, // copy to fat
};

auto verify_SD_CONTROL(u16 value) -> bool
{
    return value == SD_CONTROL_DISABLE || value == SD_CONTROL_ENABLE || value == SD_CONTROL_READ_STATE;
}

auto verify_SPI_CONTROL(u16 value) -> bool
{
    return value == SPI_CONTROL_DISABLE || value == SPI_CONTROL_ENABLE;
}

auto verify_SPI_WRITE(u16 value) -> bool
{
    return value == SPI_WRITE_DISABLE || value == SPI_WRITE_ENABLE;
}

auto verify_BUFFER_CONTROL(u16 value) -> bool
{
    return value == BUFFER_CONTROL_DISABLE || value == BUFFER_CONTROL_ENABLE || value == BUFFER_CONTROL_READ_STATE;
}

auto verify_PS_RAMPAGE(u16 value) -> bool
{
    return (value & 0xFFF) == 0 && value <= PS_RAMPAGE_UNK3;
}

auto verify_RAMPAGE(u16 value) -> bool
{
    return (value & 0xF) == 0 && value <= RAMPAGE_RTS_UNK;
}

auto verify_RTC_STATUS(u16 value) -> bool
{
    return value == RTC_STATUS_DISABLE || value == RTC_STATUS_ENABLE;
}

auto verify_AUTO_SAVE_MODE(u16 value) -> bool
{
    return value == AUTO_SAVE_MODE_DISABLE || value == AUTO_SAVE_MODE_ENABLE;
}

auto verify_SD_BLOCKS(u16 value) -> bool
{
    // afaik the max blocks is 4
    const auto blocks = value & 0xFF;
    return blocks <= 4;
}

auto get_sd_addr(u16 high, u16 low) -> u32
{
    return ((high << 16) | low) * fat::SECTOR_SIZE;
}

inline auto read16_raw(const u8* data) -> u16
{
    u16 result{};

    result |= data[0] << 0;
    result |= data[1] << 8;

    return result;
}

inline auto read32_raw(const u8* data) -> u32
{
    u32 result{};

    result |= data[0] << 0;
    result |= data[1] << 8;
    result |= data[2] << 16;
    result |= data[3] << 24;

    return result;
}

inline auto write16_raw(u8* data, u16 value) -> void
{
    data[0] = value >> 0;
    data[1] = value >> 8;
}

auto get_backup_type_from_save_mode(u8 save_mode) -> backup::Type
{
    switch (save_mode)
    {
        case 0x00:
        // this is actually sram for any game that is not in the table
        // ie homebrew, so ezflash decides that theres a chance the homebrew
        // may use sram so it treats it as such just in case
        // i don't emulate this behaviour however as i'd rather the homebrew
        // be added to a table and maybe submit a pr to make the rom detecable
        // by emulators, i did this with OpenLara in the past.
        case 0x10:
            return backup::Type::EZFLASH_NONE;

        case 0x11:
            return backup::Type::EZFLASH_SRAM;

        case 0x21:
            return backup::Type::EZFLASH_EEPROM512;

        case 0x22:
        case 0x23:
            return backup::Type::EZFLASH_EEPROM8K;

        case 0x32:
        case 0x33:
            return backup::Type::EZFLASH_FLASH512;

        case 0x31:
            return backup::Type::EZFLASH_FLASH1M;

        default:
            // std::printf("[EZFLASH] unknown save type: 0x%02X\n", save_mode);
            return backup::Type::EZFLASH_NONE;
    }
}

inline auto is_rtc_addr(u32 addr) -> bool
{
    return addr == RTC_DATA || addr == RTC_RW || addr == RTC_ENABLE;
}

} // namespace

void Ezflash::flush_sd_buffer(Gba& gba, u32& addr)
{
    for (u32 i = 0; i < sd_buffer_index; i += 2)
    {
        const auto data = read16_raw(sd_buffer + i);
        fat::write16(gba, addr, data);
        addr += 2;
    }

    sd_buffer_index = 0;
}

void Ezflash::flush_rts(Gba& gba)
{
    assert(REG_RAMPAGE >= RAMPAGE_RTS);
    constexpr u32 offset = RAMPAGE_RTS_START * 0x1000;
    fat_transfer(gba, FatTransferType::DST, S71GL064A08.ram + offset, RTS_SIZE, FAT_TABLE_START_CLUSTER_RTS);
}

// todo: verify cluster size etc
void Ezflash::fat_transfer(Gba& gba, u8 transfer_type, u8* ptr, u32 size, u16 fat_index)
{
    constexpr auto FAT_CLUSTER_EOF = 0xFFFFFFFF;

    // index counter for ptr
    u32 count = 0;
    // index counter for fat, reset of cluster change
    u32 fat_count = 0;
    // offset into fat
    u32 offset = read32_raw(FAT_table_buffer + fat_index) * 512;
    // advance to the next cluster offset
    fat_index += 4;

    while (count < size)
    {
        // todo: figure out cluster size!
        // todo: create loads of diff fat files and test this!
        const auto loop_max = std::min<u32>(size - count, 512 * 8);

        for (u32 i = 0; i < loop_max; i += 2)
        {
            switch (transfer_type)
            {
                case FatTransferType::SRC: {
                    const auto data = fat::read16(gba, offset + fat_count);
                    write16_raw(ptr + count, data);
                }   break;

                case FatTransferType::DST: {
                    const auto data = read16_raw(ptr + count);
                    fat::write16(gba, offset + fat_count, data);
                }   break;
            }

            count += 2;
            fat_count += 2;
        }

        // fetch new cluster offset
        const auto new_offset = read32_raw(FAT_table_buffer + fat_index);

        // if EOF, then we use the same cluster offset, otherwise, jump to it
        if (new_offset != FAT_CLUSTER_EOF)
        {
            if (transfer_type == FatTransferType::DST)
            {
                fat::flush(gba, offset, fat_count);
            }

            assert(!"found fragmented file!");
            offset = new_offset * 512;
            // reset fat index offset
            fat_count = 0;
            // advance to the next cluster offset
            fat_index += 4;
        }
    }

    if (transfer_type == FatTransferType::DST)
    {
        fat::flush(gba, offset, fat_count);
    }
}

template<typename T>
auto Ezflash::read_internal(Gba& gba, u32 addr, bool& handled) -> T
{
    handled = true;

    if (is_game_mode() && addr >= 0x08000000 && addr <= 0x0DFFFFFF)
    {
        // this is always readable
        if (addr == FPGA_VER && REG_SPI_CONTROL == SPI_CONTROL_ENABLE)
        {
            return REG_FPGA_VER;
        }

        addr &= 0x01FFFFFF;

        // reading from psram
        if (REG_ROMPAGE == 0x200)
        {
            return S98WS512PE0.read_ram<T>(addr);
        }
        // reading from nor
        else
        {
            const auto offset = REG_ROMPAGE * 0x20000;
            return S98WS512PE0.read_flash<T>(addr + offset);
        }
    }

    if (is_os_mode() && !start_command && addr >= FLASH_BASE_S71 && addr <= FLASH_BASE_S71_END)
    {
        if (gba.gpio.rw && REG_RTC_STATUS == RTC_STATUS_ENABLE && is_rtc_addr(addr))
        {
            handled = false;
        }
        else
        {
            return S71GL064A08.read_flash<T>(addr - FLASH_BASE_S71);
        }
    }
    else if (is_os_mode() && !start_command && addr >= FLASH_BASE_S98 && addr <= FLASH_BASE_S98_END)
    {
        assert(REG_ROMPAGE >= 0x8002);
        const auto page = (REG_ROMPAGE - 0x8002) * 0x800;
        return S98WS512PE0.read_flash<T>((addr - FLASH_BASE_S98) + page);
    }
    else if (is_os_mode() && !start_command && addr >= 0x08800000 && addr <= 0x08FFFFFF)
    {
        u32 offset = addr - 0x08800000;
        offset += REG_PS_RAMPAGE * 0x800;
        return S98WS512PE0.read_ram<T>(offset);
    }
    else if (addr >= 0x0E000000 && addr <= 0x0E00FFFF)
    {
        if (is_os_mode() || REG_RAMPAGE >= RAMPAGE_RTS)
        {
            const auto offset = REG_RAMPAGE * 0x1000;
            return S71GL064A08.read_ram<T>((addr - 0x0E000000) + offset);
        }
    }
    else if (is_os_mode() && addr >= 0x09E00000 && addr <= 0x09E00000 + 512 * 4 && REG_BUFFER_CONTROL == SPI_CONTROL_DISABLE && REG_SD_CONTROL == SD_CONTROL_ENABLE)
    {
        assert(sizeof(T) == 2 && "non 16bit sd access");

        const auto result = fat::read16(gba, sd_addr);
        sd_addr += 2;
        return result;
    }
    else if (is_os_mode())
    {
        switch (addr)
        {
            case FPGA_VER:
                // if enabled, write fpga version
                if (REG_SPI_CONTROL == SPI_CONTROL_ENABLE)
                {
                    return REG_FPGA_VER;
                }
                // https://github.com/ez-flash/omega-kernel/blob/f8b2550b4e83752c12997a4a3440543319a13006/source/Ezcard_OP.c#L227
                // https://github.com/ez-flash/omega-de-kernel/blob/5594adc26e2888724fea578468897254c2ba5a9d/source/Ezcard_OP.c#L227
                else if (REG_BUFFER_CONTROL == 3)
                {
                    return static_cast<T>(SD_RESPONSE_UN2);
                }
                // otherwise it's used for SD_Response
                else
                {
                    switch (REG_SD_CONTROL)
                    {
                        case SD_CONTROL_DISABLE:
                            assert(!"unhandled sd status read which control is SD_CONTROL_DISABLE");
                            break;

                        case SD_CONTROL_ENABLE:
                            assert(!"this should be handled in the above if statment that reads fat!");
                            break;

                        case SD_CONTROL_READ_STATE:
                            return SD_RESPONSE_UN0;

                        default:
                            assert(!"unhandled sd status read which control is invalid");
                            break;
                    }
                }
                break;
        }
    }

    #if 1
    if (is_os_mode() && !is_rtc_addr(addr))
    {
        log::print_info(gba, log::Type::EZFLASH, "unhandled read addr: 0x%X\n", addr);
    }
    #endif

    handled = false;
    return 0;
}

template<typename T>
void Ezflash::write_internal(Gba& gba, u32 addr, T value, bool& handled)
{
    handled = true;

    if (is_os_mode() && !start_command && addr >= FLASH_BASE_S71 && addr <= FLASH_BASE_S71_END)
    {
        if (REG_RTC_STATUS == RTC_STATUS_ENABLE && is_rtc_addr(addr))
        {
            handled = false;
        }
        else
        {
            S71GL064A08.write_flash<T>(addr - FLASH_BASE_S71, value);
        }
    }
    else if (is_os_mode() && !start_command && addr >= FLASH_BASE_S98 && addr <= FLASH_BASE_S98_END)
    {
        assert(REG_ROMPAGE >= 0x8002);
        const auto page = (REG_ROMPAGE - 0x8002) * 0x800;
        S98WS512PE0.write_flash<T>((addr - FLASH_BASE_S98) + page, value);
    }
    else if (is_os_mode() && !start_command && addr >= 0x08800000 && addr <= 0x08FFFFFF)
    {
        u32 offset = addr - 0x08800000;
        offset += REG_PS_RAMPAGE * 0x800;

        S98WS512PE0.write_ram<T>(offset, value);
    }
    else if (addr >= 0x0E000000 && addr <= 0x0E00FFFF)
    {
        if (is_os_mode() || REG_RAMPAGE >= RAMPAGE_RTS)
        {
            const auto offset = REG_RAMPAGE * 0x1000;
            S71GL064A08.write_ram<T>((addr - 0x0E000000) + offset, value);
        }
        else
        {
            handled = false;
        }
    }
    else if (is_os_mode() && addr >= 0x09E00000 && addr <= 0x09E003FE && REG_BUFFER_CONTROL == 1)
    {
        assert(sizeof(T) == 2 && "non 16bit fat access");
        assert(fat_table_index < sizeof(FAT_table_buffer));

        write16_raw(FAT_table_buffer + fat_table_index, value);
        fat_table_index += 2;

        if (fat_table_index == sizeof(FAT_table_buffer))
        {
            fat_table_index = 0; // maybe?

            const auto fat_gamesize = read32_raw(FAT_table_buffer + FAT_TABLE_GAMEFILE_SIZE);
            const auto fat_mode = read32_raw(FAT_table_buffer + FAT_TABLE_MODE);

            switch (fat_mode)
            {
                case FAT_TABLE_MODE_ROM_COPY_PSRAM:
                    fat_transfer(gba, FatTransferType::SRC, S98WS512PE0.ram, fat_gamesize, FAT_TABLE_START_CLUSTER_ROM);
                    break;

                case FAT_TABLE_MODE_PARAMATER: // handled in on_enter_game_mode()
                    break;
            }

            log::print_info(gba, log::Type::EZFLASH, "doing Send_FATbuffer: addr: 0x%08X 0x%04X\n", addr, fat_table_index);
        }
    }
    else if (is_os_mode() && addr >= 0x09E00000 && addr <= 0x09E00000 + 512 * 4 && REG_SD_CONTROL == SD_CONTROL_READ_STATE)
    {
        assert(sizeof(T) == 2 && "non 16bit sd access");
        assert(sd_buffer_index < sizeof(sd_buffer));

        write16_raw(sd_buffer + sd_buffer_index, value);
        sd_buffer_index += 2;
    }
    else
    {
        switch (addr)
        {
            case COMMAND_SEQ1_ADDR:
                if (value == COMMAND_SEQ1_VALUE)
                {
                    start_command = COMMAND_SEQ1_ADDR;
                }
                assert(value == COMMAND_SEQ1_VALUE);
                break;

            case COMMAND_SEQ2_ADDR:
                if (value == COMMAND_SEQ2_VALUE)
                {
                    start_command = COMMAND_SEQ2_ADDR;
                }
                assert(value == COMMAND_SEQ2_VALUE);
                break;

            case COMMAND_SEQ3_ADDR:
                if (value == COMMAND_SEQ3_VALUE)
                {
                    start_command = COMMAND_SEQ3_ADDR;
                }
                assert(value == COMMAND_SEQ3_VALUE);
                break;

            case COMMAND_SEQ4_ADDR:
                if (value == COMMAND_SEQ4_VALUE)
                {
                    start_command = COMMAND_SEQ4_ADDR;
                }
                assert(value == COMMAND_SEQ4_VALUE);
                break;

            // this is always at the end of a sd card command seq
            // i treat this as a flush command for any data that's written
            // to the sd card as the data is first written and then its told
            // the address of where to go...
            case COMMAND_SEQ5_ADDR:
                assert(value == COMMAND_SEQ5_VALUE);
                if (sd_buffer_index)
                {
                    const auto old_addr = sd_addr;
                    flush_sd_buffer(gba, sd_addr);
                    fat::flush(gba, old_addr, sd_addr - old_addr);
                }
                start_command = COMMAND_NONE;
                break;

            case SD_CONTROL:
                assert(verify_SD_CONTROL(value));
                REG_SD_CONTROL = value;
                assert(is_os_mode());
                break;

            case BUFFER_CONTROL:
                assert(verify_BUFFER_CONTROL(value));
                REG_BUFFER_CONTROL = value;
                assert(is_os_mode());
                break;

            case ROMPAGE:
                // assert(verify_ROMPAGE(value));
                if (is_game_mode())
                {
                    REG_ROMPAGE = value;
                    if (is_os_mode())
                    {
                        log::print_info(gba, log::Type::EZFLASH, "left game mode and entered os mode\n");
                    }
                    else
                    {
                        log::print_info(gba, log::Type::EZFLASH, "rompage write in game mode\n");
                    }
                }
                else
                {
                    REG_ROMPAGE = value;
                    if (is_game_mode())
                    {
                        log::print_info(gba, log::Type::EZFLASH, "entered gamemode!\n");
                        on_enter_game_mode(gba);
                    }
                }
                break;

            case PS_RAMPAGE:
                assert(verify_PS_RAMPAGE(value));
                REG_PS_RAMPAGE = value;
                assert(is_os_mode());
                break;

            case RAMPAGE:
                assert(verify_RAMPAGE(value));
                // if we are switching from RTS bank to normal sram bank
                // then flush the rts data.
                if (is_game_mode() && REG_RAMPAGE >= RAMPAGE_RTS_START && value < RAMPAGE_RTS_START)
                {
                    flush_rts(gba);
                }
                REG_RAMPAGE = value;
                break;

            case SPI_CONTROL:
                assert(verify_SPI_CONTROL(value));
                REG_SPI_CONTROL = value;
                assert(is_os_mode());
                break;

            case SPI_WRITE:
                assert(verify_SPI_WRITE(value));
                REG_SPI_WRITE = value;
                assert(is_os_mode());
                break;

            case RTC_STATUS:
                assert(verify_RTC_STATUS(value));
                REG_RTC_STATUS = value;
                assert(is_os_mode());
                break;

            case SD_ADDR_LOW:
                REG_SD_ADDR_LOW = value;
                assert(is_os_mode());
                break;

            case SD_ADDR_HIGH:
                REG_SD_ADDR_HIGH = value;
                assert(is_os_mode());
                sd_addr = get_sd_addr(REG_SD_ADDR_HIGH, REG_SD_ADDR_LOW);
                break;

            case SD_BLOCKS:
                assert(verify_SD_BLOCKS(value));
                REG_SD_BLOCKS = value;
                assert(is_os_mode());
                break;

            case AUTO_SAVE_MODE:
                assert(verify_AUTO_SAVE_MODE(value));
                REG_AUTO_SAVE_MODE = value;
                assert(is_os_mode());
                break;

            default:
                handled = false;
                break;
        }
    }

    #if 1
    if (!handled && is_os_mode() && !is_rtc_addr(addr))
    {
        log::print_warn(gba, log::Type::EZFLASH, "unhandled write addr: 0x%X value: 0x%04X\n", addr, value);
    }
    #endif
}

template<> auto Ezflash::read<u8>(Gba& gba, u32 addr, bool& handled) -> u8
{
    return read_internal<u8>(gba, addr, handled);
}
template<> auto Ezflash::read<u16>(Gba& gba, u32 addr, bool& handled) -> u16
{
    return read_internal<u16>(gba, addr, handled);
}
template<> auto Ezflash::read<u32>(Gba& gba, u32 addr, bool& handled) -> u32
{
    return read_internal<u32>(gba, addr, handled);
}

template<> void Ezflash::write<u8>(Gba& gba, u32 addr, u8 value, bool& handled)
{
    return write_internal<u8>(gba, addr, value, handled);
}
template<> void Ezflash::write<u16>(Gba& gba, u32 addr, u16 value, bool& handled)
{
    return write_internal<u16>(gba, addr, value, handled);
}
template<> void Ezflash::write<u32>(Gba& gba, u32 addr, u32 value, bool& handled)
{
    return write_internal<u32>(gba, addr, value, handled);
}

auto Ezflash::is_os_mode() const -> bool
{
    return bit::is_set<15>(REG_ROMPAGE);
}

auto Ezflash::is_game_mode() const -> bool
{
    return !bit::is_set<15>(REG_ROMPAGE);
}

auto Ezflash::get_info(u16 info) const -> u16
{
    return read_set(0x7B0000 + (info * 2));
}

void Ezflash::set_info(u16 info, u16 value)
{
    write_set(0x7B0000 + (info * 2), value);
}

auto Ezflash::read_set(u32 addr) const -> u16
{
    return this->S71GL064A08.read_flash<u16>(addr);
}

void Ezflash::write_set(u32 addr, u16 value)
{
    this->S71GL064A08.write_flash_data(addr, &value, sizeof(value));
}

// get the number of games installed to S98WS512PE0
auto Ezflash::get_nor_rom_count() const -> u8
{
    u8 count{};
    u32 addr{};

    for (;;)
    {
        // should be 0x51AE
        const auto nin_logo = this->S98WS512PE0.read_flash<u16>(addr + 0x6);
        // should be 0x96
        const auto fixed_value = this->S98WS512PE0.read_flash<u16>(addr + 0xB2);
        // should be 0x0000
        const auto reserved = this->S98WS512PE0.read_flash<u8>(addr + 0xBE);

        if (nin_logo == 0x51AE && fixed_value == 0x96 && reserved == 0x00)
        {
            struct FM_NOR_FS norfs{};
            this->S71GL064A08.read_flash_data(NOR_S71_INFO_OFFSET_NOR + count * sizeof(norfs), &norfs, sizeof(norfs));

            char gamename[0x10];
            this->S98WS512PE0.read_flash_data(addr + 0xA0, gamename, sizeof(gamename));

            if (std::memcmp(gamename, norfs.gamename, sizeof(gamename)) == 0)
            {
                addr += norfs.filesize;
                count++;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    return count;
}

// fill a number of entries and return the number filled
auto Ezflash::get_nor_rom_entries(std::span<FM_NOR_FS> entries) const -> u8
{
    u8 count{};

    for (auto& norfs : entries)
    {
        const auto addr = NOR_S71_INFO_OFFSET_NOR + count * sizeof(norfs);
        if (addr >= NOR_S71_INFO_OFFSET_SET)
        {
            break;
        }

        this->S71GL064A08.read_flash_data(addr, &norfs, sizeof(norfs));

        if (norfs.filesize)
        {
            count++;
        }
        else
        {
            break;
        }
    }

    return count;
}

void Ezflash::init(Gba& gba, Type new_type)
{
    type = new_type;

    S71GL064A08.init();
    S98WS512PE0.init();

    write_set(NOR_S71_SET_INFO_LANG, SET_INFO_LANG_DEFAULT);
    write_set(NOR_S71_SET_INFO_RESET, SET_INFO_RESET_DEFAULT);
    write_set(NOR_S71_SET_INFO_RTS, SET_INFO_RTS_DEFAULT);
    write_set(NOR_S71_SET_INFO_SLEEP, SET_INFO_SLEEP_DEFAULT);
    write_set(NOR_S71_SET_INFO_CHEAT, SET_INFO_CHEAT_DEFAULT);
    write_set(NOR_S71_SET_INFO_SLEEP_KEY0, SET_INFO_SLEEP_KEY0_DEFAULT);
    write_set(NOR_S71_SET_INFO_SLEEP_KEY1, SET_INFO_SLEEP_KEY1_DEFAULT);
    write_set(NOR_S71_SET_INFO_SLEEP_KEY2, SET_INFO_SLEEP_KEY2_DEFAULT);
    write_set(NOR_S71_SET_INFO_MENU_KEY0, SET_INFO_MENU_KEY0_DEFAULT);
    write_set(NOR_S71_SET_INFO_MENU_KEY1, SET_INFO_MENU_KEY1_DEFAULT);
    write_set(NOR_S71_SET_INFO_MENU_KEY2, SET_INFO_MENU_KEY2_DEFAULT);
    write_set(NOR_S71_SET_INFO_ENGINE, SET_INFO_ENGINE_DEFAULT);
    write_set(NOR_S71_SET_INFO_SHOW_THUMBNAIL, SET_INFO_SHOW_THUMBNAIL_DEFAULT);
    write_set(NOR_S71_SET_INFO_RTC_OPEN_STATUS, SET_INFO_RTC_OPEN_STATUS_DEFAULT);

    // DE settings
    if (type == Type::OMEGA_DE)
    {
        write_set(NOR_S71_SET_INFO_AUTO_SAVE_SEL, SET_INFO_AUTO_SAVE_SEL_DEFAULT);
        write_set(NOR_S71_SET_INFO_MODE_B_INIT, SET_INFO_MODE_B_INIT_DEFAULT);
        write_set(NOR_S71_SET_INFO_LED_OPEN_SEL, SET_INFO_LED_OPEN_SEL_DEFAULT);
        write_set(NOR_S71_SET_INFO_BREATHING_R, SET_INFO_BREATHING_R_DEFAULT);
        write_set(NOR_S71_SET_INFO_BREATHING_G, SET_INFO_BREATHING_G_DEFAULT);
        write_set(NOR_S71_SET_INFO_BREATHING_B, SET_INFO_BREATHING_B_DEFAULT);
        write_set(NOR_S71_SET_INFO_SD_R, SET_INFO_SD_R_DEFAULT);
        write_set(NOR_S71_SET_INFO_SD_G, SET_INFO_SD_G_DEFAULT);
        write_set(NOR_S71_SET_INFO_SD_B, SET_INFO_SD_B_DEFAULT);
    }
}

void Ezflash::reset(Gba& gba)
{
    std::memcpy(S71GL064A08.flash, gba.rom, 0x200000);

    gba.backup.init(gba, backup::Type::EZFLASH_NONE);

    std::memset(FAT_table_buffer, 0, sizeof(FAT_table_buffer));
    fat_table_index = 0;

    std::memset(sd_buffer, 0, sizeof(sd_buffer));
    sd_buffer_index = 0;

    start_command = COMMAND_NONE;

    // i am not sure of these values on startup
    REG_PS_RAMPAGE = PS_RAMPAGE_UNK0;
    REG_RAMPAGE = 0x0000;
    REG_ROMPAGE = ROMPAGE_KERNEL; // set to OS mode

    REG_FPGA_VER = FPGA_VER_MAX;
    REG_SD_CONTROL = SD_CONTROL_DISABLE;
    REG_SPI_CONTROL = SPI_CONTROL_DISABLE;
    REG_SPI_WRITE = SPI_WRITE_DISABLE;
    REG_BUFFER_CONTROL = BUFFER_CONTROL_DISABLE;
    REG_RTC_STATUS = RTC_STATUS_DISABLE;

    REG_SD_ADDR_LOW = 0x0000;
    REG_SD_ADDR_HIGH = 0x0000;
    REG_SD_BLOCKS = 0x0000;

    REG_AUTO_SAVE_MODE = AUTO_SAVE_MODE_DISABLE;
}

void Ezflash::on_enter_game_mode(Gba& gba)
{
    // const auto fat_gamesize = read32_raw(FAT_table_buffer + FAT_TABLE_GAMEFILE_SIZE);
    const auto fat_mode = read32_raw(FAT_table_buffer + FAT_TABLE_MODE);
    const auto fat_savefile_size = read32_raw(FAT_table_buffer + FAT_TABLE_SAVEFILE_SIZE_AND_TYPE) & 0xFFFFFF;
    const auto fat_save_type = read32_raw(FAT_table_buffer + FAT_TABLE_SAVEFILE_SIZE_AND_TYPE) >> 24;

    #if 1
    for (u32 i = 0; i < sizeof(FAT_table_buffer); i += 4)
    {
        const auto data = read32_raw(FAT_table_buffer + i);
        if (data)
        {
            log::print_debug(gba, log::Type::EZFLASH, "FAT[0x%03X] 0x%08X\n", i, data);
        }
    }
    #endif

    switch (fat_mode)
    {
        case FAT_TABLE_MODE_ROM_COPY_PSRAM:
            // the game maps ram to rom addr so oob rom read will retun
            // whatever values were in ram, for this reason, memcpy
            // over the entire ram region.
            // std::memcpy(gba.rom, S98WS512PE0.ram, fat_gamesize);
            // std::memcpy(gba.rom, S98WS512PE0.ram, sizeof(S98WS512PE0.ram));
            break;

        case FAT_TABLE_MODE_PARAMATER:
            // NOTE: if multiple games are installed and loaded game0
            // is it then possible ingame to read the rest of the contents
            // of nor, so basically reading the rest of the game data?
            // i am 99% sure its possible, so a POC can be made where
            // a test rom actually boots into another game installed as long
            // as it's within the 32mb range, ie test_rom + emerald is possible
            // std::memcpy(gba.rom, S98WS512PE0.flash + REG_ROMPAGE * 0x20000, fat_gamesize);
            break;

        default:
            assert(!"unknown mode");
            break;
    }

    #if 0
    FILE* f = fopen("poke-em-patched.gba", "wb");
    fwrite(gba.rom, fat_gamesize, 1, f);
    fclose(f);
    #endif

    const auto backup_type = get_backup_type_from_save_mode(fat_save_type);
    log::print_info(gba, log::Type::EZFLASH, "loading backup save: %u\n", std::to_underlying(backup_type));
    gba.backup.init(gba, backup_type);

    // load a save
    if (backup_type != backup::Type::EZFLASH_NONE && fat_savefile_size)
    {
        assert(fat_savefile_size <= 0x20 * 0x1000 && "OOB save size!");
        // the data is copied to sram so there's no need to fetch
        // via a fat_transfer, which nicely simplifies it

        switch (backup_type)
        {
            case backup::Type::EZFLASH_EEPROM512:
            case backup::Type::EZFLASH_EEPROM8K:
                gba.backup.eeprom.load_data(gba, {S71GL064A08.ram, fat_savefile_size});
                break;

            case backup::Type::EZFLASH_SRAM:
                gba.backup.sram.load_data(gba, {S71GL064A08.ram, fat_savefile_size});
                break;

            case backup::Type::EZFLASH_FLASH512:
            case backup::Type::EZFLASH_FLASH1M:
                gba.backup.flash.load_data(gba, {S71GL064A08.ram, fat_savefile_size});
                break;

            default:
                break;
        }
    }

    gpio::reset(gba, true);
    mem::setup_tables(gba);
}

void Ezflash::flush_save(Gba& gba)
{
    if (!is_game_mode() || !gba.is_save_dirty(true))
    {
        return;
    }

    const auto fat_savefile_size = read32_raw(FAT_table_buffer + FAT_TABLE_SAVEFILE_SIZE_AND_TYPE) & 0xFFFFFF;
    const auto fat_save_type = read32_raw(FAT_table_buffer + FAT_TABLE_SAVEFILE_SIZE_AND_TYPE) >> 24;
    const auto fat_start_cluster_save = read32_raw(FAT_table_buffer + FAT_TABLE_START_CLUSTER_SAVE);
    const auto save_offset_in_fat = fat_start_cluster_save * 512;
    const auto backup_type = get_backup_type_from_save_mode(fat_save_type);

    std::span<u8> data;

    switch (backup_type)
    {
        case backup::Type::EZFLASH_EEPROM512:
        case backup::Type::EZFLASH_EEPROM8K:
            data = gba.backup.eeprom.data;
            break;

        case backup::Type::EZFLASH_SRAM:
            data = gba.backup.sram.data;
            break;

        case backup::Type::EZFLASH_FLASH512:
        case backup::Type::EZFLASH_FLASH1M:
            data = gba.backup.flash.data;
            break;

        default:
            break;
    }

    if (!data.empty())
    {
        if (type == Type::OMEGA_DE)
        {
            std::memcpy(S71GL064A08.ram, data.data(), data.size());
        }
        else if (fat_savefile_size && save_offset_in_fat) // this should always be true, but check to make sure we dont nuke sd card!
        {
            fat_transfer(gba, FatTransferType::DST, data.data(), fat_savefile_size, FAT_TABLE_START_CLUSTER_SAVE);
        }

        log::print_info(gba, log::Type::EZFLASH, "flushing save!\n");
    }
}

auto Ezflash::get_data() const -> SaveData
{
    SaveData save{};
    save.write_entry(S71GL064A08.flash);
    save.write_entry(S98WS512PE0.flash);

    // the omega uses fram instead of sram, this persists after power off
    // on reloading the kernel, it checks the norS71 for a flag set for
    // save data, if set, it dumps the data from fram to the save file.
    if (type == Type::OMEGA_DE)
    {
        save.write_entry({S71GL064A08.ram, 0x20000});
    }

    return save;
}

auto Ezflash::load_data(std::span<const u8> data) -> bool
{
    constexpr auto minimum_size = sizeof(S71GL064A08.flash) + sizeof(S98WS512PE0.flash);
    constexpr auto minimum_size_de = minimum_size + 0x20000;

    if (data.size() != minimum_size && type == Type::OMEGA)
    {
        return false;
    }
    else if (data.size() != minimum_size_de && type == Type::OMEGA_DE)
    {
        return false;
    }

    const auto S71_data = data.subspan(0, sizeof(S71GL064A08.flash));
    const auto S98_data = data.subspan(sizeof(S71GL064A08.flash), sizeof(S98WS512PE0.flash));

    std::memcpy(S71GL064A08.flash, S71_data.data(), S71_data.size());
    std::memcpy(S98WS512PE0.flash, S98_data.data(), S98_data.size());

    if (type == Type::OMEGA_DE)
    {
        const auto fram_data = data.subspan(sizeof(S71GL064A08.flash) + sizeof(S98WS512PE0.flash), 0x20000);
        std::memcpy(S71GL064A08.ram, fram_data.data(), fram_data.size());
    }

    return true;
}

} // namespace gba::fat::ezflash
