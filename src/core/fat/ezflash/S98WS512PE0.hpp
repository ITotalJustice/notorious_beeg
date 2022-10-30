// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fwd.hpp"
#include <span>

namespace gba::flash::s98 {

struct S98WS512PE0
{
    // 64MB flash
    // this is the NOR that games are saved to
    // the meta data for the games are saved in S71GL064A08
    // only the data is stored in S98WS512PE0.
    u8 flash[64 * 1024 * 1024];

    // 32MB psram
    // this is where the rom is loaded when starting a game
    // this is also where the rom patches are applied by first loading
    // the rom and then applying the patches on top.
    u8 ram[32 * 1024 * 1024];

    // bank command is entirely unused in ezflash
    // this is kept for future compatibility with new / custom
    // kernel releases which may use the bank
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

template<> [[nodiscard]] auto S98WS512PE0::read_flash<u8>(u32 addr) const -> u8;
template<> [[nodiscard]] auto S98WS512PE0::read_flash<u16>(u32 addr) const -> u16;
template<> [[nodiscard]] auto S98WS512PE0::read_flash<u32>(u32 addr) const -> u32;

template<> void S98WS512PE0::write_flash<u8>(u32 addr, u8 value);
template<> void S98WS512PE0::write_flash<u16>(u32 addr, u16 value);
template<> void S98WS512PE0::write_flash<u32>(u32 addr, u32 value);

template<> [[nodiscard]] auto S98WS512PE0::read_ram<u8>(u32 addr) const -> u8;
template<> [[nodiscard]] auto S98WS512PE0::read_ram<u16>(u32 addr) const -> u16;
template<> [[nodiscard]] auto S98WS512PE0::read_ram<u32>(u32 addr) const -> u32;

template<> void S98WS512PE0::write_ram<u8>(u32 addr, u8 value);
template<> void S98WS512PE0::write_ram<u16>(u32 addr, u16 value);
template<> void S98WS512PE0::write_ram<u32>(u32 addr, u32 value);

} // namespace gba::flash::s98
