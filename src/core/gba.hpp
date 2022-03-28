// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "arm7tdmi/arm7tdmi.hpp"
#include "ppu.hpp"
#include "apu/apu.hpp"
#include "mem.hpp"
#include "dma.hpp"
#include "timer.hpp"
#include "scheduler.hpp"
#include "backup/backup.hpp"
#include "fwd.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace gba {

enum Button : std::uint16_t
{
    A       = 1 << 0,
    B       = 1 << 1,
    SELECT  = 1 << 2,
    START   = 1 << 3,
    RIGHT   = 1 << 4,
    LEFT    = 1 << 5,
    UP      = 1 << 6,
    DOWN    = 1 << 7,
    L       = 1 << 8,
    R       = 1 << 9,

    DIRECTIONAL = UP | DOWN | LEFT | RIGHT,
    // causes a reset if these buttons are pressed all at once
    RESET = A | B | START | SELECT,
};

struct State;
using AudioCallback = void(*)(void* user, std::int16_t left, std::int16_t right);

struct Gba
{
    bool halted;
    std::size_t cycles2;
    std::size_t cycles;
    scheduler::Scheduler scheduler;
    arm7tdmi::Arm7tdmi cpu;
    ppu::Ppu ppu;
    apu::Apu apu;
    mem::Mem mem;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;

    // this is not part of the mem struct because we do not
    // want to savestate this :harold:
    // 32mb(max), 16-bus
    alignas(uint32_t) uint8_t rom[0x2000000];
    bool has_bios;

    auto reset() -> void;
    auto loadrom(std::span<const std::uint8_t> new_rom) -> bool;
    auto loadbios(std::span<const std::uint8_t> new_bios) -> bool;
    auto run(std::size_t cycles = 280896) -> void;

    auto loadstate(const State& state) -> bool;
    auto savestate(State& state) const -> bool;

    // load a save from data, must be used after a game has loaded
    auto loadsave(std::span<const std::uint8_t> new_save) -> bool;
    // returns empty spam if the game doesn't have a save
    auto getsave() const -> std::span<const std::uint8_t>;

    // OR keys together
    auto setkeys(std::uint16_t buttons, bool down) -> void;

    auto set_userdata(void* user) { this->userdata = user; }
    auto set_audio_callback(AudioCallback cb) { this->audio_callback = cb; }

    void* userdata{};
    AudioCallback audio_callback{};
};

struct State
{
    uint32_t magic; // 0xFACADE
    uint32_t version; // 0x1
    uint32_t size; // size of state struct
    uint32_t crc; // crc of game

    std::size_t cycles2;
    std::size_t cycles;
    scheduler::Scheduler scheduler;
    arm7tdmi::Arm7tdmi cpu;
    apu::Apu apu;
    ppu::Ppu ppu;
    mem::Mem mem;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;
};

} // namespace gba
