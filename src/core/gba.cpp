// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "gba.hpp"
#include "apu/apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "backup/backup.hpp"
#include "backup/eeprom.hpp"
#include "backup/flash.hpp"
#include "fat/fat.hpp"
#include "gameboy/gb.hpp"
#include "key.hpp"
#include "mem.hpp"
#include "scheduler.hpp"
#include "bios.hpp"
#include "gameboy/internal.hpp"
#include "gameboy/ppu/ppu.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <utility>

namespace gba {
namespace {

void on_scheduler_reset_cb(void* user, s32 id, s32 cycles_late)
{
    auto gba = static_cast<Gba*>(user);
    // call default scheduler reset event
    scheduler::Scheduler::reset_event(&gba->scheduler, scheduler::RESERVED_ID);
    // adjust anything that uses timestamps here!
    gba->apu.square0.timestamp -= scheduler::TIMEOUT_VALUE;
    gba->apu.square1.timestamp -= scheduler::TIMEOUT_VALUE;
    gba->apu.wave.timestamp -= scheduler::TIMEOUT_VALUE;
    gba->apu.noise.timestamp -= scheduler::TIMEOUT_VALUE;
    // dont forget gb timers :)
    if (gba->is_gb() && gba->gameboy.timer.tima_reload_timestamp >= scheduler::TIMEOUT_VALUE)
    {
        gba->gameboy.timer.tima_reload_timestamp -= scheduler::TIMEOUT_VALUE;
    }

    gba->scheduler.add_absolute(id, scheduler::TIMEOUT_VALUE, on_scheduler_reset_cb, user);
}

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

void reset_gb(Gba& gba)
{
    gb::reset(gba);
}

void reset_gba(Gba& gba)
{
    // if the user did not load bios, then load builtin
    if (!gba.has_bios)
    {
        gba::bios::load_normmatt_bios(gba);
    }

    bool skip_bios = true;

    // disable waitloop detecting if a fat device is enabled
    // as its possible the code executing is not the rom but
    // code mapped to the rom region, ie, ezflash.
    // this is too expensive to check for this in waitloop detection
    // so its safer to just disable it.
    gba.waitloop.reset(gba, gba.fat_device.type == fat::Type::NONE);
    fat::reset(gba);
    gpio::reset(gba, skip_bios); // this is needed before mem::reset because rw needs resetting
    mem::reset(gba, skip_bios); // this needed to be before arm::reset because memtables
    ppu::reset(gba, skip_bios);
    apu::reset(gba, skip_bios);
    arm7tdmi::reset(gba, skip_bios);
}

auto set_buttons_gb(Gba& gba, u16 buttons, bool down)
{
    gb::set_buttons(gba, buttons, down);

    if (down && buttons & Button::L)
    {
        gba.stretch = true;
    }

    if (down && buttons & Button::R)
    {
        gba.stretch = false;
    }
}

auto set_buttons_gba(Gba& gba, u16 buttons, bool down)
{
    key::set_key(gba, buttons, down);
}

auto loadsave_gb(Gba& gba, std::span<const u8> new_save) -> bool
{
    if (new_save.size() > gb::SAVE_SIZE_MAX)
    {
        return false;
    }
    std::memcpy(gba.gameboy.ram, new_save.data(), new_save.size());
    return true;
}

auto loadsave_gba(Gba& gba, std::span<const u8> new_save) -> bool
{
    return gba.backup.load_data(gba, new_save);
}

auto is_save_dirty_gb(Gba& gba, bool auto_clear) -> bool
{
    const auto result = gba.gameboy.ram_dirty;

    if (auto_clear)
    {
        gba.gameboy.ram_dirty = false;
    }

    return result;
}

auto is_save_dirty_gba(Gba& gba, bool auto_clear) -> bool
{
    const auto result = gba.backup.is_dirty(gba);

    if (auto_clear)
    {
        gba.backup.clear_dirty_flag(gba);
    }

    return result;
}

auto get_save_gb(const Gba& gba) -> SaveData
{
    if (gb::has_save(gba))
    {
        const auto size = gb::calculate_savedata_size(gba);
        if (size)
        {
            SaveData save;
            save.write_entry({ gba.gameboy.ram, size });
            return save;
        }
    }

    return {};
}

auto get_save_gba(const Gba& gba) -> SaveData
{
    return gba.backup.get_data(gba);
}

auto run_gb(Gba& gba, u32 cycles)
{
    gb::run(gba, cycles / 4);
}

void on_frame_end_event(void* user, s32 id, s32 late)
{
    auto& gba = *static_cast<Gba*>(user);
    gba.delta.add(id, late);
    gba.frame_end = true;
}

auto run_gba(Gba& gba, u32 cycles)
{
    // this needs better impl because some events will rely on
    // being fired, such as sampling, hblank, vblank etc
    if (arm7tdmi::is_stop_mode(gba))
    {
        // the keys are always "checked" on a real gba
        // there is no point in doing this when emulating
        // the gba, only exception is when in stop mode as
        // it's possible to already hold keys down whilst entering
        // stop mode and have it "immediately" exit.
        // in LOZMC, this effect can be seen with sleep mode
        // however the screen begins to fade completly and gets about
        // 50% of the way there (depends how quick you release A button).
        // this fade means the blanked lcd takes effect very quickly
        key::check_key_interrupt(gba);
        return;
    }

    gba.frame_end = false;
    gba.scheduler.add(scheduler::ID::FRAME, gba.delta.get(scheduler::ID::FRAME, cycles), on_frame_end_event, &gba);

    if (gba.cpu.halted) [[unlikely]]
    {
        arm7tdmi::on_halt_event(&gba);

        // lets say the gba needs to run for 100 cycles and is halted somewhere
        // in those cycles.
        // the gba is then ticked again for 100 cycles, as its in halt, the if(halted)
        // will enter and stop after 100 cycles, possibly still within halt.
        // without the below if, the for(;;) loop will enter and tick the cpu
        // at least once even though it's halted!
        if (gba.frame_end)
        {
            return;
        }
    }

    if (!gba.frame_end)
    {
        for (;;)
        {
            arm7tdmi::run(gba);

            if (gba.scheduler.should_fire())
            {
                gba.scheduler.fire();

                if (gba.frame_end) [[unlikely]]
                {
                    break;
                }
            }
        }
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
        std::printf("[GBA-HEADER] failed to validate checksum\n");
        return false;
    }

    if (!validate_fixed_value())
    {
        std::printf("[GBA-HEADER] failed to validate fixed value\n");
        return false;
    }

    return true;
}

Gba::Gba()
{
    // log_type = 0;
    // log_type |= log::FLAG_TYPE_ALL_APU;
    // log_type |= log::FLAG_TYPE_ALL_ARM;
    // log_type |= log::FLAG_TYPE_ALL_BACKUP;
    // log_type |= log::FLAG_TYPE_ALL_FAT;
    // log_type |= log::FLAG_TYPE_ALL_GB;
    // log_type |= log::FLAG_GAME;
}

Gba::~Gba()
{
    delete fat_device.mpcf;
    delete fat_device.m3cf;
    delete fat_device.sccf;
    delete fat_device.ezflash;
}

auto Gba::reset() -> void
{
    scheduler.reset(0, on_scheduler_reset_cb, this);
    delta.reset();

    if (is_gb())
    {
        reset_gb(*this);
    }
    else
    {
        reset_gba(*this);
    }
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
        gb::init(*this);
        // reset the sram
        std::memset(mem.ewram, 0xFF, sizeof(mem.ewram));

        if (gb::loadrom(*this, new_rom))
        {
            system = System::GB;
            return true;
        }

        return false;
    }

    system = System::GBA;

    // todo: handle if the user has already set / loaded sram for the game
    // or maybe it should always be like this, load game, then load backup
    const auto backup_type = backup::find_type(new_rom);

    if (!this->backup.init(*this, backup_type))
    {
        // return false;
    }

    // pre-calc the OOB rom read values, which is addr >> 1
    fill_rom_oob_values(this->rom, new_rom.size());

    std::memcpy(this->rom, new_rom.data(), new_rom.size());

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

    std::memcpy(this->bios, new_bios.data(), new_bios.size());
    this->has_bios = true;

    this->reset();

    return true;
}

auto Gba::setkeys(u16 buttons, bool down) -> void
{
    if (is_gb())
    {
        set_buttons_gb(*this, buttons, down);
    }
    else
    {
        set_buttons_gba(*this, buttons, down);
    }
}

auto Gba::set_audio_callback(AudioCallback cb, std::span<s16> data, u32 _sample_rate)-> void
{
    this->sample_rate = _sample_rate;
    this->audio_callback = cb;
    this->sample_data = data;
    this->sample_count = 0;

    if (sample_rate)
    {
        if (is_gb())
        {
            this->sample_rate_calculated = gb::CPU_CYCLES / sample_rate;
        }
        else
        {
            this->sample_rate_calculated = CYCLES_PER_FRAME * 60 / sample_rate;
        }
    }

    if (this->audio_callback && !this->sample_data.empty() && this->sample_rate_calculated)
    {
        scheduler.add(scheduler::ID::APU_SAMPLE, this->sample_rate_calculated, apu::on_sample_event, this);
    }
    else
    {
        scheduler.remove(scheduler::ID::APU_SAMPLE);
    }
}

auto Gba::set_pixels(void* _pixels, u32 _stride, u8 _bpp) -> void
{
    this->pixels = _pixels;
    this->stride = _stride;
    this->bpp = _bpp;
}

auto Gba::get_render_mode() -> u8
{
    return ppu::get_mode(*this);
}

auto Gba::render_mode(std::span<u16> _pixels, u8 mode, u8 layer) -> u8
{
    if (is_gb())
    {
        return gb::render_layer(*this, mode, layer, _pixels);
    }
    else
    {
        return ppu::render_bg_mode(*this, mode, layer, _pixels);
    }
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

    if (is_gb())
    {
        gb::loadstate(*this, &state.gb_state);
    }

    mem::setup_tables(*this);
    state.scheduler.on_loadstate(*this);

    return true;
}

auto Gba::savestate(State& state) const -> bool
{
    state.magic = StateMeta::MAGIC;
    state.version = StateMeta::VERSION;
    state.size = StateMeta::SIZE;
    state.crc = 0;

    state.scheduler.on_savestate(*this);
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

    if (is_gb())
    {
        gb::savestate(*this, &state.gb_state);
    }

    return true;
}

auto Gba::loadsave(std::span<const u8> new_save) -> bool
{
    if (is_gb())
    {
        return loadsave_gb(*this, new_save);
    }
    else
    {
        return loadsave_gba(*this, new_save);
    }
}

auto Gba::is_save_dirty(bool auto_clear) -> bool
{
    if (is_gb())
    {
        return is_save_dirty_gb(*this, auto_clear);
    }
    else
    {
        return is_save_dirty_gba(*this, auto_clear);
    }
}

auto Gba::getsave() const -> SaveData
{
    if (is_gb())
    {
        return get_save_gb(*this);
    }
    else
    {
        return get_save_gba(*this);
    }
}

auto Gba::get_rom_name() const -> RomName
{
    RomName name{};

    if (is_gb())
    {
        gb::CartName gb_name;
        gb::get_rom_name(*this, &gb_name);
        std::strcpy(name.str, gb_name.name);
    }
    else
    {
        const Header header{this->rom};
        std::strcpy(name.str, header.game_title);
    }

    return name;
}

void Gba::set_fat_device_type(fat::Type type)
{
    fat::init(*this, type);
}

auto Gba::create_fat32_image(std::span<u8> data) -> bool
{
    return fat::create_image(data);
}

auto Gba::set_fat32_data(std::span<u8> data) -> bool
{
    if (data.size() == 512 * 1024 * 1024)
    {
        fat32_data = data;
        return true;
    }

    return false;
}

auto Gba::run(u32 _cycles) -> void
{
    if (is_gb())
    {
        run_gb(*this, _cycles);
    }
    else
    {
        run_gba(*this, _cycles);
    }
}

} // namespace gba

namespace scheduler {

void State::on_savestate(const gba::Gba& gba)
{
    for (s32 i = 0; i < ID::END; i++)
    {
        // see if we have this event in queue, if we do, it's enabled
        if (gba.scheduler.has_event(i))
        {
            entries[i].enabled = true;
            entries[i].cycles = gba.scheduler.get_event_cycles_absolute(i);
        }
        else
        {
            entries[i].enabled = false;
            entries[i].cycles = 0;
        }
    }

    std::memcpy(&delta, &gba.delta, sizeof(delta));
    scheduler_cycles = gba.scheduler.get_ticks();
}

void State::on_loadstate(gba::Gba& gba) const
{
    gba.scheduler.reset(scheduler_cycles, gba::on_scheduler_reset_cb, &gba);
    std::memcpy(&gba.delta, &delta, sizeof(gba.delta));

    for (s32 i = 0; i < ID::END; i++)
    {
        if (entries[i].enabled)
        {
            if (gba.is_gba())
            {
                switch (i)
                {
                    case ID::PPU: gba.scheduler.add_absolute(i, entries[i].cycles, gba::ppu::on_event, &gba); break;
                    case ID::APU_FRAME_SEQUENCER: gba.scheduler.add_absolute(i, entries[i].cycles, gba::apu::on_frame_sequencer_event, &gba); break;
                    case ID::TIMER0: [[fallthrough]];
                    case ID::TIMER1: [[fallthrough]];
                    case ID::TIMER2: [[fallthrough]];
                    case ID::TIMER3: gba.scheduler.add_absolute(i, entries[i].cycles, gba::timer::on_timer_event, &gba); break;
                    case ID::DMA: gba.scheduler.add_absolute(i, entries[i].cycles, gba::dma::on_event, &gba); break;
                    case ID::INTERRUPT: gba.scheduler.add_absolute(i, entries[i].cycles, gba::arm7tdmi::on_interrupt_event, &gba); break;
                    case ID::HALT: gba.scheduler.add_absolute(i, entries[i].cycles, gba::arm7tdmi::on_halt_event, &gba); break;
                }
            }
            else
            {
                switch (i)
                {
                    case ID::TIMER0: gba.scheduler.add_absolute(i, entries[i].cycles, gba::gb::on_timer_event, &gba); break;
                    case ID::TIMER1: gba.scheduler.add_absolute(i, entries[i].cycles, gba::gb::on_div_event, &gba); break;
                    case ID::TIMER2: gba.scheduler.add_absolute(i, entries[i].cycles, gba::gb::on_timer_reload_event, &gba); break;
                }
            }
        }
    }

    // special case for sample event
    // SEE: https://github.com/ITotalJustice/notorious_beeg/issues/85
    if (gba.sample_data.empty() || !gba.sample_rate_calculated)
    {
        gba.scheduler.remove(ID::APU_SAMPLE);
    }
    else
    {
        gba.scheduler.add(ID::APU_SAMPLE, gba.sample_rate_calculated, gba::apu::on_sample_event, &gba);
    }
}

} // namespace scheduler
