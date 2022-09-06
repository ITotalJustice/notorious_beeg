// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "arm7tdmi/arm7tdmi.hpp"
#include "ppu/ppu.hpp"
#include "apu/apu.hpp"
#include "mem.hpp"
#include "dma.hpp"
#include "timer.hpp"
#include "scheduler.hpp"
#include "backup/backup.hpp"
#include "gpio.hpp"
#include "fwd.hpp"
#include <span>
#include <string_view>

namespace gba {

enum Button : u16
{
    A       = 1 << 0,
    B       = 1 << 1,
    SELECT  = 1 << 2,
    START   = 1 << 3,
    RIGHT   = 1 << 4,
    LEFT    = 1 << 5,
    UP      = 1 << 6,
    DOWN    = 1 << 7,
    R       = 1 << 8,
    L       = 1 << 9,

    DIRECTIONAL = UP | DOWN | LEFT | RIGHT,
    // causes a reset if these buttons are pressed all at once
    RESET = A | B | START | SELECT,

    // all button pressed at once, useful for clearing all buttons
    // at the same time.
    ALL = A | B | SELECT | START | RIGHT | LEFT | UP | DOWN | R | L,
};

struct State;
struct Header;

using AudioCallback = void(*)(void* user);
using VblankCallback = void(*)(void* user);
using HblankCallback = void(*)(void* user, u16 line);

struct Gba
{
    // at the top so no offset needed into struct on r/w access
    mem::ReadArray rmap[16];
    mem::WriteArray wmap[16];

    scheduler::Scheduler scheduler;
    arm7tdmi::Arm7tdmi cpu;
    mem::Mem mem;
    ppu::Ppu ppu;
    apu::Apu apu;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;
    gpio::Gpio gpio;

    // 16kb, 32-bus
    u8 bios[1024 * 16];
    // 32mb(max), 16-bus
    u8 rom[0x2000000];

    bool has_bios;

    auto reset() -> void;
    [[nodiscard]] auto loadrom(std::span<const u8> new_rom) -> bool;
    [[nodiscard]] auto loadbios(std::span<const u8> new_bios) -> bool;
    auto run(u32 cycles = 280896) -> void;

    [[nodiscard]] auto loadstate(const State& state) -> bool;
    [[nodiscard]] auto savestate(State& state) const -> bool;

    // load a save from data, must be used after a game has loaded
    [[nodiscard]] auto loadsave(std::span<const u8> new_save) -> bool;
    // checks if the save has been written to.
    // the flag is cleared upon calling this function.
    [[nodiscard]] auto is_save_dirty(bool auto_clear) -> bool;
    // returns empty span if the game doesn't have a save
    // call is_save_dirty() first to see if the game needs saving!
    [[nodiscard]] auto getsave() const -> std::span<const u8>;

    // OR keys together
    auto setkeys(u16 buttons, bool down) -> void;

    auto set_userdata(void* user) { this->userdata = user; }
    auto set_audio_callback(AudioCallback cb, std::span<s16> data, u32 sample_rate = 65536) -> void;
    auto set_vblank_callback(VblankCallback cb) { this->vblank_callback = cb; }
    auto set_hblank_callback(HblankCallback cb) { this->hblank_callback = cb; }

    [[nodiscard]] auto get_render_mode() -> u8;
    // returns the priority of the layer
    [[nodiscard]] auto render_mode(std::span<u16> pixels, u8 mode, u8 layer) -> u8;

    bool bit_crushing{false};

    void* userdata{};
    std::span<s16> sample_data;
    std::size_t sample_count;
    std::uint32_t sample_rate_calculated;

    AudioCallback audio_callback{};
    VblankCallback vblank_callback{};
    HblankCallback hblank_callback{};
};

struct State
{
    u32 magic; // see StateMeta::MAGIC
    u32 version; // see StateMeta::VERSION
    u32 size; // see StateMeta::SIZE
    u32 crc; // crc of game

    scheduler::Scheduler scheduler;
    arm7tdmi::Arm7tdmi cpu;
    mem::Mem mem;
    apu::Apu apu;
    ppu::Ppu ppu;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;
    gpio::Gpio gpio;
};

enum StateMeta : u32
{
    MAGIC = 0xFACADE,
    VERSION = 4,
    SIZE = sizeof(State),
};

struct Header
{
    Header() = default;
    Header(std::span<const u8> rom);

    u32 rom_entry_point{};
    u8 nintendo_logo[156]{};
    // uppercase ascii
    char game_title[12]{};
    char game_code[4]{};
    char maker_code[2]{};
    // this has to be 0x96
    u8 fixed_value{};
    // should be 0x00
    u8 main_unit_code{};
    u8 device_type{};
    // should be all zero
    u8 _reserved_area[7]{};
    // should be 0
    u8 software_version{};
    // header checksum
    u8 complement_check{};
    // should be all zero
    u8 _reserved_area2[2]{};

    [[nodiscard]] auto raw() const { return reinterpret_cast<const u8*>(this); }
    [[nodiscard]] auto span() const { return std::span{raw(), sizeof(*this)}; }

    [[nodiscard]] auto raw() { return reinterpret_cast<u8*>(this); }
    [[nodiscard]] auto span() { return std::span{raw(), sizeof(*this)}; }


    [[nodiscard]] auto validate_checksum() const -> u8;
    [[nodiscard]] auto validate_fixed_value() const -> bool;
    [[nodiscard]] auto validate_all() const -> bool;

};

static_assert(
    sizeof(Header) == 192,
    "Header size has changed!"
);

} // namespace gba
