// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"
#include <span>

namespace gba::flash::s71 {

struct S71GL064A08
{
    // 8MB NOR flash.
    // the bootloader and kernel both reside in here.
    // the bootloader is closed source so it's not possible (yet)
    // to emulate the bootup sequence of the ezflash, such as showing the
    // splash screen and holding R to force a fw update.
    // meta data for all games installed to
    u8 flash[8 * 1024 * 1024];

    // 1MB sram.
    // this is used for save data.
    // the rom's save date is loaded here and written
    // to the sd card when the game writes to this ram.
    // because of this, it's very easy to end up with a corrupt
    // save file as the time it takes for the save data to be written
    // to sd card is too slow, and no feedback is given to the user
    // upon completion.
    // this was fixed in the omega de as it uses fram (persistent) instead
    // of sram and only writes the save data when rebooting the console.
    u8 ram[1 * 1024 * 1024];

    u32 bank;

    u16 buffer_count;

    u8 command;
    u8 state;

    auto init() -> void;
    auto load_data(std::span<const u8> new_data) -> bool;
    [[nodiscard]] auto get_data() const -> std::span<const u8>;

    template<typename T> [[nodiscard]] auto read_flash(u32 addr) const -> T;
    template<typename T> void write_flash(u32 addr, T value);

    template<typename T> [[nodiscard]] auto read_ram(u32 addr) const -> T;
    template<typename T> void write_ram(u32 addr, T value);

    // helper to write raw data to flash, useful for if the flash
    // needs to be init with data on startup.
    void write_flash_data(u32 addr, const void* data, u32 size);
    void read_flash_data(u32 addr, void* data, u32 size) const;

private:
    template<typename T> [[nodiscard]] auto read_flash_internal(u32 addr) const -> T;
    template<typename T> void write_flash_internal(u32 addr, T value);

    template<typename T> [[nodiscard]] auto read_ram_internal(u32 addr) const -> T;
    template<typename T> void write_ram_internal(u32 addr, T value);
};

template<> [[nodiscard]] auto S71GL064A08::read_flash<u8>(u32 addr) const -> u8;
template<> [[nodiscard]] auto S71GL064A08::read_flash<u16>(u32 addr) const -> u16;
template<> [[nodiscard]] auto S71GL064A08::read_flash<u32>(u32 addr) const -> u32;

template<> void S71GL064A08::write_flash<u8>(u32 addr, u8 value);
template<> void S71GL064A08::write_flash<u16>(u32 addr, u16 value);
template<> void S71GL064A08::write_flash<u32>(u32 addr, u32 value);

template<> [[nodiscard]] auto S71GL064A08::read_ram<u8>(u32 addr) const -> u8;
template<> [[nodiscard]] auto S71GL064A08::read_ram<u16>(u32 addr) const -> u16;
template<> [[nodiscard]] auto S71GL064A08::read_ram<u32>(u32 addr) const -> u32;

template<> void S71GL064A08::write_ram<u8>(u32 addr, u8 value);
template<> void S71GL064A08::write_ram<u16>(u32 addr, u16 value);
template<> void S71GL064A08::write_ram<u32>(u32 addr, u32 value);

} // namespace gba::flash::s71
