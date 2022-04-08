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
    L       = 1 << 8,
    R       = 1 << 9,

    DIRECTIONAL = UP | DOWN | LEFT | RIGHT,
    // causes a reset if these buttons are pressed all at once
    RESET = A | B | START | SELECT,
};

struct State;
using AudioCallback = void(*)(void* user, s16 left, s16 right);
using VblankCallback = void(*)(void* user, u16 line);
using HblankCallback = void(*)(void* user, u16 line);

struct Gba
{
    scheduler::Scheduler scheduler;
    arm7tdmi::Arm7tdmi cpu;
    mem::Mem mem;
    ppu::Ppu ppu;
    apu::Apu apu;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;

    // this is not part of the mem struct because we do not
    // want to savestate this :harold:
    // 32mb(max), 16-bus
    alignas(u32) u8 rom[0x2000000];
    bool has_bios;

    auto reset() -> void;
    auto loadrom(std::span<const u8> new_rom) -> bool;
    auto loadbios(std::span<const u8> new_bios) -> bool;
    auto run(u32 cycles = 280896) -> void;

    auto loadstate(const State& state) -> bool;
    auto savestate(State& state) const -> bool;

    // load a save from data, must be used after a game has loaded
    auto loadsave(std::span<const u8> new_save) -> bool;
    // returns empty spam if the game doesn't have a save
    auto getsave() const -> std::span<const u8>;

    // OR keys together
    auto setkeys(u16 buttons, bool down) -> void;

    auto set_userdata(void* user) { this->userdata = user; }
    auto set_audio_callback(AudioCallback cb) { this->audio_callback = cb; }
    auto set_vblank_callback(VblankCallback cb) { this->vblank_callback = cb; }
    auto set_hblank_callback(HblankCallback cb) { this->hblank_callback = cb; }

    auto get_render_mode() -> u8
    {
        return ppu::get_mode(*this);
    }

    // returns the priority of the layer
    auto render_mode(std::span<u16> pixels, u8 mode, u8 layer) -> u8
    {
        return ppu::render_bg_mode(*this, mode, layer, pixels);
    }

    // only memcpy vram if one of the blocks are dirty
    // (these vars are placed here because i dont want to mess up my savestates atm).
    // the divisors were chosen to have the size be as close to 64 (cache line size).
    static constexpr inline auto dirty_vram_shift = 2048;
    static constexpr inline auto dirty_vram_size = sizeof(mem::Mem::vram) / dirty_vram_shift;

    static constexpr inline auto dirty_pram_shift = 16;
    static constexpr inline auto dirty_pram_size = sizeof(mem::Mem::pram) / dirty_pram_shift;

    static constexpr inline auto dirty_oam_shift = 16;
    static constexpr inline auto dirty_oam_size = sizeof(mem::Mem::oam) / dirty_oam_shift;

    bool dirty_vram_any{false};
    bool dirty_pram_any{false};
    bool dirty_oam_any{false};

    bool dirty_vram[dirty_vram_size]{false};
    bool dirty_pram[dirty_pram_size]{false};
    bool dirty_oam[dirty_oam_size]{false};

    bool bit_crushing{false};

    void* userdata{};
    AudioCallback audio_callback{};
    VblankCallback vblank_callback{};
    HblankCallback hblank_callback{};
};

struct State
{
    u32 magic; // 0xFACADE
    u32 version; // 0x1
    u32 size; // size of state struct
    u32 crc; // crc of game

    scheduler::Scheduler scheduler;
    arm7tdmi::Arm7tdmi cpu;
    mem::Mem mem;
    apu::Apu apu;
    ppu::Ppu ppu;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;
};

} // namespace gba
