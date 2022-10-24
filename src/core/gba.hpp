// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "arm7tdmi/arm7tdmi.hpp"
#include "gameboy/types.hpp"
#include "ppu/ppu.hpp"
#include "apu/apu.hpp"
#include "mem.hpp"
#include "dma.hpp"
#include "timer.hpp"
#include "scheduler.hpp"
#include "backup/backup.hpp"
#include "gpio.hpp"
#include "fwd.hpp"
#include <cassert>
#include <span>

namespace scheduler {

enum ID : s32
{
    PPU,
    // todo: profile removing the apu channel events
    APU_SQUARE0,
    APU_SQUARE1,
    APU_WAVE,
    APU_NOISE,
    APU_FRAME_SEQUENCER,
    APU_SAMPLE,
    TIMER0,
    TIMER1,
    TIMER2,
    TIMER3,
    DMA,
    INTERRUPT,
    HALT,

    // special event to indicate the end of a frame.
    // the cycles is set by the user in run();
    FRAME,

    // not an actual event, it just keeps track of the size
    END,
};

struct DeltaManager
{
    s32 deltas[ID::END]{};

    constexpr void reset()
    {
        for (auto& delta : deltas) { delta = 0; }
    }

    constexpr void add(s32 id, s32 delta)
    {
        assert(id < 15);
        assert(delta <= 0);
        deltas[id] = delta;
    }

    constexpr void remove(s32 id)
    {
        assert(id < 15);
        deltas[id] = 0;
    }

    [[nodiscard]] constexpr auto get(s32 id, s32 time) -> s32
    {
        assert(id < 15);
        return time + deltas[id];
    }
};

struct State
{
    struct Entry
    {
        s32 enabled;
        s32 cycles;
    };

    Entry entries[ID::END];
    DeltaManager delta;
    s32 scheduler_cycles;

    void on_savestate(const gba::Gba& gba);
    void on_loadstate(gba::Gba& gba) const;
};

} // namespace scheduler

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

enum class System
{
    GBA,
    GB,
};

struct Colour
{
    constexpr Colour(u16 colour) : bgr555{colour} { }

    [[nodiscard]] constexpr auto r() const -> u8 { return (bgr555 >> 0) & 31;  }
    [[nodiscard]] constexpr auto g() const -> u8 { return (bgr555 >> 5) & 31;  }
    [[nodiscard]] constexpr auto b() const -> u8 { return (bgr555 >> 10) & 31;  }

    // converts 5bit colour to 8bit colour
    // low 3 bits are filled, the latter method being the best.
#if 0
    [[nodiscard]] constexpr auto r8() const -> u8 { return (r() * 255 / 31); }
    [[nodiscard]] constexpr auto g8() const -> u8 { return (g() * 255 / 31); }
    [[nodiscard]] constexpr auto b8() const -> u8 { return (b() * 255 / 31); }
#else
    [[nodiscard]] constexpr auto r8() const -> u8 { return (r() << 3) | ((r() >> 2) & 0x7); }
    [[nodiscard]] constexpr auto g8() const -> u8 { return (g() << 3) | ((g() >> 2) & 0x7); }
    [[nodiscard]] constexpr auto b8() const -> u8 { return (b() << 3) | ((b() >> 2) & 0x7); }
#endif

    u16 bgr555;
};

struct RomName
{
    char str[17]; // NULL terminanted
};

struct State;
struct Header;

using AudioCallback = void(*)(void* user);
using VblankCallback = void(*)(void* user);
using HblankCallback = void(*)(void* user, u16 line);
using ColourCallback = u32(*)(void* user, Colour colour);

struct Gba
{
    // at the top so no offset needed into struct on r/w access
    mem::ReadArray rmap[16];
    mem::WriteArray wmap[16];

    s32 elapsed_cycles;

    scheduler::Scheduler scheduler;
    scheduler::DeltaManager delta;
    arm7tdmi::Arm7tdmi cpu;
    mem::Mem mem;
    ppu::Ppu ppu;
    apu::Apu apu;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;
    gpio::Gpio gpio;

    gb::Core gameboy;

    // 16kb, 32-bus
    u8 bios[1024 * 16];
    bool has_bios;

    // 32mb(max), 16-bus
    u8 rom[0x2000000];

    bool stretch;

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
    auto set_colour_callback(ColourCallback cb) { this->colour_callback = cb; }

    // set the pixels that the game will render to
    // IMPORTANT: if pixels == NULL, then no rendering will happen!
    auto set_pixels(void* pixels, u32 stride, u8 bpp) -> void;

    [[nodiscard]] auto get_render_mode() -> u8;
    // returns the priority of the layer
    [[nodiscard]] auto render_mode(std::span<u16> pixels, u8 mode, u8 layer) -> u8;

    [[nodiscard]] auto is_gba() const { return system == System::GBA; }
    [[nodiscard]] auto is_gb() const { return system == System::GB; }

    [[nodiscard]] auto get_rom_name() const -> RomName;

    System system;

    bool bit_crushing{false};
    bool frame_end;

    void* userdata{};
    std::span<s16> sample_data;
    std::size_t sample_count;
    u32 sample_rate;
    u32 sample_rate_calculated;

    void* pixels;
    u32 stride;
    u8 bpp;

    AudioCallback audio_callback{};
    VblankCallback vblank_callback{};
    HblankCallback hblank_callback{};
    ColourCallback colour_callback{};
};

struct State
{
    u32 magic; // see StateMeta::MAGIC
    u32 version; // see StateMeta::VERSION
    u32 size; // see StateMeta::SIZE
    u32 crc; // crc of game

    scheduler::State scheduler;
    arm7tdmi::Arm7tdmi cpu;
    mem::Mem mem;
    apu::Apu apu;
    ppu::Ppu ppu;
    dma::Channel dma[4];
    timer::Timer timer[4];
    backup::Backup backup;
    gpio::Gpio gpio;
    gb::State gb_state;
};

enum StateMeta : u32
{
    MAGIC = 0xFACADE,
    VERSION = 6,
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
