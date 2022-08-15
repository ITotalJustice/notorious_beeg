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
#include <cstring>
#include <ranges>
#include <numeric>

namespace gba {
namespace {

// fills the rom with OOB values
// NOTE: this does NOT work for OOB dma, as they return open bus!!!
// > offset is the starting point in rom to fill rom
// > for optimising, offset=rom_size, otherwise fill the entire rom
constexpr auto fill_rom_oob_values(std::span<u8> rom, const u32 offset)
{
    for (auto i = offset; i < rom.size(); i += 2)
    {
        rom[i + 0] = i >> 1; // lower nibble (addr >> 1)
        rom[i + 1] = i >> 9; // upper nibble (addr >> (8 + 1))
    }
}

} // namespace

Header::Header(std::span<const u8> rom)
{
    if (rom.size() >= sizeof(*this))
    {
        std::memcpy(this->raw(), rom.data(), sizeof(*this));
    }
}

auto Header::validate_checksum() const -> u8
{
    constexpr auto CHECKSUM_OFFSET = 0xA0;
    constexpr auto CHECKSUM_SIZE = 0x1D;

    const auto check_data = this->span().subspan(CHECKSUM_OFFSET, CHECKSUM_SIZE);

    // perform checksum, sub all entries (with 0xFF wrapping)
    const auto checksum = std::accumulate(
        check_data.begin(), check_data.end(),
        -0x19, std::minus<u8>{}
    );

    return checksum == this->complement_check;
}

auto Header::validate_fixed_value() const -> bool
{
    constexpr auto FIXED_VALUE = 0x96;
    return this->fixed_value == FIXED_VALUE;
}

auto Header::validate_all() const -> bool
{
    if (!validate_checksum())
    {
        std::printf("failed to validate checksum\n");
        return false;
    }

    if (!validate_fixed_value())
    {
        std::printf("failed to validate fixed value\n");
        return false;
    }

    return true;
}

auto Gba::reset() -> void
{
    // if the user did not load bios, then load builtin
    if (!this->has_bios)
    {
        gba::bios::load_normmatt_bios(*this);
    }

    bool skip_bios = true;

    scheduler::reset(*this);
    mem::reset(*this, skip_bios); // this needed to be before arm::reset because memtables
    ppu::reset(*this, skip_bios);
    apu::reset(*this, skip_bios);
    gpio::reset(*this, skip_bios);
    arm7tdmi::reset(*this, skip_bios);
}

auto Gba::loadrom(std::span<const u8> new_rom) -> bool
{
    if (new_rom.size() > sizeof(this->rom))
    {
        assert(!"rom is way too beeg");
        return false;
    }

    const Header header{new_rom};

    if (!header.validate_all())
    {
        assert(!"rom failed to validate rom header!");
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

    // pre-calc the OOB rom read values, which is addr >> 1
    fill_rom_oob_values(this->rom, new_rom.size());

    std::ranges::copy(new_rom, this->rom);

    this->reset();

    return true;
}

auto Gba::loadbios(std::span<const u8> new_bios) -> bool
{
    if (new_bios.size() > std::size(this->bios))
    {
        assert(!"bios is way too beeg");
        return false;
    }

    // std::copy()
    // std::ranges::copy()
    std::ranges::copy(new_bios, this->bios);
    this->has_bios = true;

    this->reset();

    return true;
}

auto Gba::setkeys(u16 buttons, bool down) -> void
{
    auto& gba = *this;

    // the pins go LOW when pressed!
    if (down)
    {
        REG_KEY &= ~buttons;

    }
    else
    {
        REG_KEY |= buttons;
    }

    // this can be better optimised at some point
    if (down && ((buttons & DIRECTIONAL) != 0))
    {
        if ((buttons & RIGHT) != 0)
        {
            REG_KEY |= LEFT;
        }
        if ((buttons & LEFT) != 0)
        {
            REG_KEY |= RIGHT;
        }
        if ((buttons & UP) != 0)
        {
            REG_KEY |= DOWN;
        }
        if ((buttons & DOWN) != 0)
        {
            REG_KEY |= UP;
        }
    }
}

auto Gba::get_render_mode() -> u8
{
    return ppu::get_mode(*this);
}

auto Gba::render_mode(std::span<u16> pixels, u8 mode, u8 layer) -> u8
{
    return ppu::render_bg_mode(*this, mode, layer, pixels);
}

auto Gba::loadstate(const State& state) -> bool
{
    if (state.magic != StateMeta::MAGIC)
    {
        return false;
    }
    if (state.version != StateMeta::VERSION)
    {
        return false;
    }
    if (state.size != StateMeta::SIZE)
    {
        return false;
    }
    if (state.crc != 0)
    {
        return false;
    }

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
    this->gpio = state.gpio;

    mem::setup_tables(*this);
    scheduler::on_loadstate(*this);

    return true;
}

auto Gba::savestate(State& state) const -> bool
{
    state.magic = StateMeta::MAGIC;
    state.version = StateMeta::VERSION;
    state.size = StateMeta::SIZE;
    state.crc = 0;

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
    state.gpio = this->gpio;

    return true;
}

// load a save from data, must be used after a game has loaded
auto Gba::loadsave(std::span<const u8> new_save) -> bool
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
auto Gba::getsave() const -> std::span<const u8>
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

static auto on_frame_event(Gba& gba)
{
    gba.scheduler.frame_end = true;
}

auto Gba::run(u32 _cycles) -> void
{
    this->scheduler.frame_end = false;

    scheduler::add(*this, scheduler::Event::FRAME, on_frame_event, _cycles);

    if (this->cpu.halted) [[unlikely]]
    {
        arm7tdmi::on_halt_event(*this);
    }

    while (!this->scheduler.frame_end) [[likely]]
    {
        this->scheduler.elapsed = 0;
        arm7tdmi::run(*this);

        // tick scheduler
        this->scheduler.cycles += this->scheduler.elapsed;
        if (this->scheduler.next_event_cycles <= this->scheduler.cycles)
        {
            scheduler::fire(*this);
        }
    }
}

} // namespace gba
