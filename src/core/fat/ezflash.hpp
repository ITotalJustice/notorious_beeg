// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"

#include "ezflash/S71GL064A08.hpp"
#include "ezflash/S98WS512PE0.hpp"
#include <span>

// SOURCE:
namespace gba::fat::ezflash {

enum class Type
{
    OMEGA,
    OMEGA_DE,
};

struct Rts
{
    // 256kb, 16-bit bus
    u8 ewram[1024 * 256]; // 0x00000

    // 32kb, 32-bit bus
    u8 iwram[1024 * 32]; // 0x40000

    // 1kb, 16-bit
    u8 pram[1024 * 1]; // 0x48000

    u8 pad0[0x7C00]; // 0x48400

    // 96kb, 16-bit bus
    u8 vram[1024 * 96]; // 0x50000

    // 1kb, 32-bit
    u8 oam[1024 * 1]; // 0x68000

    // cpu registers???
    u8 r4_r11[0xC00]; // 0x68400

    u8 io[0x400]; // 0x69000

    // 0x6bf0
    u8 unk[458'752 - 431'104-16];

    // "EZ-OmegaRTCFILE."
    char EZ_OmegaRTCFILE[16]; // 0x6FFF0
};

static_assert(sizeof(Rts) == 458752);

// MetaData for games installed to the nor.
// this data is stored in S71GL064A08 whilst the game data
// is stored in S98WS512PE0.
// when loading the nor game list, it checks header for the rom in
// S98WS512PE0, specifically it checks if the resvered areas at 0x190
// is all zeros (it checks other values too, maybe homebrew use them?)
// and also checks 2 bytes from the nintendo logo data.
struct FM_NOR_FS
{
    // file name which is displayed in the menu
    char filename[100];
    // rompage within nor flash of the data for the game
    u16 rompage;
    // is the game patched?
    u16 have_patch;
    // is the game RTS patched?
    u16	have_RTS;
    // DE, otherwise reserved
    // set to 1 if the `maker_code` is header is "5G" which
    // indicates that the rom is 64MB in size
    u16 is_64MBrom;
    u32 filesize;
    // DE, otherwise reserved
    // set to the save type (flash, eeprom, sram etc)
    u8 savemode;
	u8 reserved[3];
    // this is a bit strange, basically it's used for checking if
    // this is a valid entry, so if it does not equal the
    // game_title and game_code in rom header then the entry is
    // invalid, SEE: `GetFileListFromNor()`
    char gamename[0x10];
};

struct Ezflash
{
    u32 sd_addr;

    flash::s71::S71GL064A08 S71GL064A08;
    flash::s98::S98WS512PE0 S98WS512PE0;

    u8 FAT_table_buffer[0x400];
    u32 fat_table_index;

    u8 sd_buffer[512 * 4]; // max buffer size
    u32 sd_buffer_index;

    u32 start_command;

    u16 REG_SD_RESPONSE;

    u16 REG_ROMPAGE;
    u16 REG_PS_RAMPAGE; // multiples of 0x1000
    u16 REG_RAMPAGE; // multiples of 0x10, max 0xB0(?)

    u16 REG_FPGA_VER;
    u16 REG_SD_CONTROL;
    u16 REG_SPI_CONTROL;
    u16 REG_SPI_WRITE;
    u16 REG_BUFFER_CONTROL;
    u16 REG_RTC_STATUS;

    u16 REG_SD_ADDR_LOW;
    u16 REG_SD_ADDR_HIGH;
    u16 REG_SD_BLOCKS;

    u16 REG_AUTO_SAVE_MODE;

    Type type;

    bool dirty;

    void init(Gba& gba, Type new_type);
    void reset(Gba& gba);

    template<typename T> [[nodiscard]] auto read(Gba& gba, u32 addr, bool& handled) -> T;
    template<typename T> void write(Gba& gba, u32 addr, T value, bool& handled);

    [[nodiscard]] auto is_os_mode() const -> bool;
    [[nodiscard]] auto is_game_mode() const -> bool;

    [[nodiscard]] auto get_info(u16 info) const -> u16;
    void set_info(u16 info, u16 value);

    void flush_save(Gba& gba);

    // get the number of games installed to S98WS512PE0
    [[nodiscard]] auto get_nor_rom_count() const -> u8;
    // fill a number of entries and return the number filled
    [[nodiscard]] auto get_nor_rom_entries(std::span<FM_NOR_FS> entries) const -> u8;

    [[nodiscard]] auto get_data() const -> SaveData;
    [[nodiscard]] auto load_data(std::span<const u8> data) -> bool;

    [[nodiscard]] auto is_dirty() const -> bool { return true; }
    // [[nodiscard]] auto is_dirty() const -> bool { return dirty; }
    void clear_dirty_flag() { dirty = false; }

private:
    template<typename T> [[nodiscard]] auto read_internal(Gba& gba, u32 addr, bool& handled) -> T;
    template<typename T> void write_internal(Gba& gba, u32 addr, T value, bool& handled);

    [[nodiscard]] auto read_set(u32 addr) const -> u16;
    void write_set(u32 addr, u16 value);

    void on_enter_game_mode(Gba& gba);
    void flush_sd_buffer(Gba& gba, u32& addr);
    void flush_rts(Gba& gba);
    void fat_transfer(Gba& gba, u8 transfer_type, u8* ptr, u32 size, u16 fat_index);
};

template<> [[nodiscard]] auto Ezflash::read<u8>(Gba& gba, u32 addr, bool& handled) -> u8;
template<> [[nodiscard]] auto Ezflash::read<u16>(Gba& gba, u32 addr, bool& handled) -> u16;
template<> [[nodiscard]] auto Ezflash::read<u32>(Gba& gba, u32 addr, bool& handled) -> u32;

template<> void Ezflash::write<u8>(Gba& gba, u32 addr, u8 value, bool& handled);
template<> void Ezflash::write<u16>(Gba& gba, u32 addr, u16 value, bool& handled);
template<> void Ezflash::write<u32>(Gba& gba, u32 addr, u32 value, bool& handled);

} // namespace gba::fat::ezflash
