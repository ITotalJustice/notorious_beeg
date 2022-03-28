// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "gba.hpp"
#include "apu/apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "backup/backup.hpp"
#include "backup/flash.hpp"
#include "mem.hpp"
#include "scheduler.hpp"
#include "bios.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <fstream>
#include <ranges>

namespace gba {

auto Gba::reset() -> void
{
    // if the user did not load bios, then load builtin
    if (!this->has_bios)
    {
        gba::bios::load_normmatt_bios(*this);
    }

    this->scheduler.cycles = 0;
    this->cycles2 = 0;
    scheduler::reset(*this);
    arm7tdmi::reset(*this);
    mem::reset(*this);
    ppu::reset(*this);
    apu::reset(*this);
}

auto Gba::loadrom(std::span<const  std::uint8_t> new_rom) -> bool
{
    if (new_rom.size() > std::size(this->rom))
    {
        assert(!"rom is way too beeg");
        return false;
    }

    // todo: handle if the user has already set / loaded sram for the game
    // or maybe it should always be like this, load game, then load backup
    const auto backup_type = backup::find_type(new_rom);
    this->backup.type = backup_type;
    using enum backup::Type;

    switch (this->backup.type)
    {
        case NONE:
            break;

        case EEPROM:
            this->backup.eeprom.init(*this);
            break;

        case SRAM:
            this->backup.sram.init(*this);
            break;

        case FLASH: [[fallthrough]]; // these are aliases for each other
        case FLASH512:
            this->backup.flash.init(*this, backup::flash::Type::Flash64);
            break;

        case FLASH1M:
            this->backup.flash.init(*this, backup::flash::Type::Flash128);
            break;
    }

    std::ranges::copy(new_rom, this->rom);

    this->reset();

    return true;
}

auto Gba::loadbios(std::span<const std::uint8_t> new_bios) -> bool
{
    if (new_bios.size() > std::size(this->mem.bios))
    {
        assert(!"bios is way too beeg");
        return false;
    }

    std::ranges::copy(new_bios, this->mem.bios);
    this->has_bios = true;

    return true;
}

auto Gba::setkeys(std::uint16_t buttons, bool down) -> void
{
    #define KEY *reinterpret_cast<uint16_t*>(this->mem.io + (mem::IO_KEY & 0x3FF))

    // the pins go LOW when pressed!
    if (down)
    {
        KEY &= ~buttons;

    }
    else
    {
        KEY |= buttons;
    }

    // this can be better optimised at some point
    if (down && (buttons & DIRECTIONAL))
    {
        if (buttons & RIGHT)
        {
            KEY |= LEFT;
        }
        if (buttons & LEFT)
        {
            KEY |= RIGHT;
        }
        if (buttons & UP)
        {
            KEY |= DOWN;
        }
        if (buttons & DOWN)
        {
            KEY |= UP;
        }
    }

    #undef KEY
}

constexpr auto STATE_MAGIC = 0xFACADE;
constexpr auto STATE_VERSION = 0x1;
constexpr auto STATE_SIZE = sizeof(State);

auto Gba::loadstate(const State& state) -> bool
{
    if (state.magic != STATE_MAGIC)
    {
        return false;
    }
    if (state.version != STATE_VERSION)
    {
        return false;
    }
    if (state.size != STATE_SIZE)
    {
        return false;
    }
    if (state.crc != 0)
    {
        return false;
    }

    this->cycles2 = state.cycles2;
    this->cycles = state.cycles;
    this->scheduler = state.scheduler;
    this->cpu = state.cpu;
    this->apu = state.apu;
    this->ppu = state.ppu;
    this->mem = state.mem;
    this->dma[0] = state.dma[0];
    this->dma[1] = state.dma[1];
    this->dma[2] = state.dma[2];
    this->dma[3] = state.dma[3];
    this->timer[0] = state.timer[0];
    this->timer[1] = state.timer[1];
    this->timer[2] = state.timer[2];
    this->timer[3] = state.timer[3];
    this->backup = state.backup;
    mem::setup_tables(*this);
    scheduler::on_loadstate(*this);

    return true;
}

auto Gba::savestate(State& state) const -> bool
{
    state.magic = STATE_MAGIC;
    state.version = STATE_VERSION;
    state.size = STATE_SIZE;
    state.crc = 0;

    state.cycles2 = this->cycles2;
    state.cycles = this->cycles;
    state.scheduler = this->scheduler;
    state.cpu = this->cpu;
    state.apu = this->apu;
    state.ppu = this->ppu;
    state.mem = this->mem;
    state.dma[0] = this->dma[0];
    state.dma[1] = this->dma[1];
    state.dma[2] = this->dma[2];
    state.dma[3] = this->dma[3];
    state.timer[0] = this->timer[0];
    state.timer[1] = this->timer[1];
    state.timer[2] = this->timer[2];
    state.timer[3] = this->timer[3];
    state.backup = this->backup;

    return true;
}

// load a save from data, must be used after a game has loaded
auto Gba::loadsave(std::span<const std::uint8_t> new_save) -> bool
{
    using enum backup::Type;
    switch (this->backup.type)
    {
        case NONE: return false;
        case EEPROM: return this->backup.eeprom.load_data(new_save);
        case SRAM: return this->backup.sram.load_data(new_save);

        case FLASH: [[fallthrough]];
        case FLASH512: [[fallthrough]];
        case FLASH1M: return this->backup.flash.load_data(new_save);
    }

    std::unreachable();
}

// returns empty spam if the game doesn't have a save
auto Gba::getsave() const -> std::span<const std::uint8_t>
{
    using enum backup::Type;
    switch (this->backup.type)
    {
        case NONE: return {};
        case EEPROM: return this->backup.eeprom.get_data();
        case SRAM: return this->backup.sram.get_data();

        case FLASH: [[fallthrough]];
        case FLASH512: [[fallthrough]];
        case FLASH1M: return this->backup.flash.get_data();
    }

    std::unreachable();
}

#if ENABLE_SCHEDULER
auto Gba::run(std::size_t _cycles) -> void
{
    auto& gba = *this;
    std::uint8_t cycles_elasped = 0;

    if (gba.cpu.halted) [[unlikely]]
    {
        arm7tdmi::on_halt_event(gba);
    }

    for (; this->cycles2 < _cycles; this->cycles2 += cycles_elasped) [[likely]]
    {
        // reset cycles each ittr
        gba.cycles = 0;

        arm7tdmi::run(gba);
        cycles_elasped = gba.cycles;

        // tick scheduler
        this->scheduler.cycles += cycles_elasped;
        if (this->scheduler.next_event_cycles <= this->scheduler.cycles)
        {
            scheduler::fire(gba);
        }
    }

    this->cycles2 -= _cycles;
}
#else
auto Gba::run(std::size_t _cycles) -> void
{
    auto& gba = *this;
    std::uint8_t cycles_elasped = 0;

    for (; this->cycles2 < _cycles; this->cycles2 += cycles_elasped) [[likely]]
    {
        // reset cycles each ittr
        gba.cycles = 0;

        arm7tdmi::run(gba);
        cycles_elasped = gba.cycles;
        gba.scheduler.cycles += cycles_elasped;
        ppu::run(gba, cycles_elasped);
        apu::run(gba, cycles_elasped);
        timer::run(gba, cycles_elasped);
    }

    this->cycles2 -= _cycles;
}
#endif

} // namespace gba
