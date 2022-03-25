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

static State state = {};

constexpr auto STATE_MAGIC = 0xFACADE;
constexpr auto STATE_VERSION = 0x1;
constexpr auto STATE_SIZE = sizeof(state);

auto Gba::loadstate(std::string_view path) -> bool
{
    std::ifstream fs{path.data(), std::ios_base::binary};

    if (fs.good()) {
        fs.read(reinterpret_cast<char*>(&state), sizeof(state));

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

        std::memcpy(&this->cycles2, &state.cycles2, sizeof(state.cycles2));
        std::memcpy(&this->cycles, &state.cycles, sizeof(state.cycles));
        std::memcpy(&this->scheduler, &state.scheduler, sizeof(state.scheduler));
        std::memcpy(&this->cpu, &state.cpu, sizeof(state.cpu));
        std::memcpy(&this->apu, &state.apu, sizeof(state.apu));
        std::memcpy(&this->ppu, &state.ppu, sizeof(state.ppu));
        std::memcpy(&this->mem, &state.mem, sizeof(state.mem));
        std::memcpy(&this->dma, &state.dma, sizeof(state.dma));
        std::memcpy(&this->timer, &state.timer, sizeof(state.timer));
        std::memcpy(&this->backup, &state.backup, sizeof(state.backup));
        mem::setup_tables(*this);
        scheduler::on_loadstate(*this);
    }

    return false;
}

auto Gba::savestate(std::string_view path) -> bool
{
    std::ofstream fs{path.data(), std::ios_base::binary};

    if (fs.good()) {
        state.magic = STATE_MAGIC;
        state.version = STATE_VERSION;
        state.size = STATE_SIZE;
        state.crc = 0;

        std::memcpy(&state.cycles2, &this->cycles2, sizeof(this->cycles2));
        std::memcpy(&state.cycles, &this->cycles, sizeof(this->cycles));
        std::memcpy(&state.scheduler, &this->scheduler, sizeof(this->scheduler));
        std::memcpy(&state.cpu, &this->cpu, sizeof(this->cpu));
        std::memcpy(&state.apu, &this->apu, sizeof(this->ppu));
        std::memcpy(&state.ppu, &this->ppu, sizeof(this->ppu));
        std::memcpy(&state.mem, &this->mem, sizeof(this->mem));
        std::memcpy(&state.dma, &this->dma, sizeof(this->dma));
        std::memcpy(&state.timer, &this->timer, sizeof(this->timer));
        std::memcpy(&state.backup, &this->backup, sizeof(this->backup));

        fs.write(reinterpret_cast<const char*>(&state), sizeof(state));
        return true;
    }

    return false;
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
