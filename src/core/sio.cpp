// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "sio.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "log.hpp"
#include "mem.hpp"
#include <utility>

namespace gba::sio {
namespace {

enum ShiftClock
{
    SHIFT_CLOCK_EXTERNAL = 0,
    SHIFT_CLOCK_INTERNAL = 1,
};

enum class SiocntRWMode
{
    Normal_8bit,
    Normal_32bit,
    MultiPlayer,
    UART,
};

// some modes don't use siocnt.
// in those modes, r/w to siocnt are still allowed but they are
// based on the bits 12,13 in siocnt.
auto get_siocnt_rw_mode(Gba& gba) -> SiocntRWMode
{
    const auto b12 = bit::is_set<12>(REG_SIOCNT);
    const auto b13 = bit::is_set<13>(REG_SIOCNT);
    const auto b15 = bit::is_set<15>(REG_RCNT);

    if (b15 == 1 && b13 == 0 && b12 == 1) // UART
    {
        return SiocntRWMode::UART;
    }
    else if (b13 == 0 && b12 == 0) // Normal 8bit
    {
        return SiocntRWMode::Normal_8bit;
    }
    else if (b13 == 0 && b12 == 1) // Normal 32bit
    {
        return SiocntRWMode::Normal_32bit;
    }
    else if (b13 == 1 && b12 == 0) // MultiPlayer
    {
        return SiocntRWMode::MultiPlayer;
    }

    std::unreachable();
}

void on_normal_mode(Gba& gba)
{
    const auto shift_clock = bit::is_set<0>(REG_SIOCNT);
    // const auto internal_clock_shift = bit::is_set<1>(REG_SIOCNT);
    // const auto so = bit::is_set<3>(REG_SIOCNT);
    const auto start_bit = bit::is_set<7>(REG_SIOCNT);
    // const auto size_type = bit::is_set<12>(REG_SIOCNT);
    const auto irq = bit::is_set<14>(REG_SIOCNT);

    // this is stub for ags to pass, could use scheduler here
    // to get correct timing if needed.
    if (start_bit && shift_clock == SHIFT_CLOCK_INTERNAL)
    {
        // say it's finished the transfer
        REG_SIOCNT = bit::unset<7>(REG_SIOCNT);

        if (irq)
        {
            arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::Serial);
        }
    }
}

} // namespace

void on_rcnt_write(Gba& gba, u16 value)
{
    REG_RCNT = value;
    log::print_info(gba, log::Type::SIO, "RCNT write: 0x%08X mode: %s\n", REG_RCNT, get_mode_str(get_mode(gba)));
}

void on_siocnt_write(Gba& gba, u16 value)
{
    const auto b12 = bit::is_set<12>(value);
    const auto b13 = bit::is_set<13>(value);

    REG_SIOCNT = bit::set<12>(REG_SIOCNT, b12);
    REG_SIOCNT = bit::set<13>(REG_SIOCNT, b13);

    // which bits to write, invert to get read only bits
    u16 write_mask = 0;

    switch (get_siocnt_rw_mode(gba))
    {
        case SiocntRWMode::Normal_8bit:
        case SiocntRWMode::Normal_32bit:
            // NOTE: bit3 is read only when bit7 is set!
            write_mask = 0b01111'1111'1000'1011;
            break;

        case SiocntRWMode::MultiPlayer:
            // NOTE: bit7 is read only for slave!
            write_mask = 0b01111'111'1000'0011;
            break;

        case SiocntRWMode::UART:
            write_mask = 0b0111'1111'1000'1111;
            break;
    }

    // write new value to SIOCNT, keeping read only intact
    REG_SIOCNT = (REG_SIOCNT & ~write_mask) | (value & write_mask);

    log::print_info(gba, log::Type::SIO, "SIOCNT write: 0x%08X mode: %s\n", REG_SIOCNT, get_mode_str(get_mode(gba)));

    switch (get_mode(gba))
    {
        case Mode::Normal_8bit:
        case Mode::Normal_32bit:
            on_normal_mode(gba);
            break;

        case Mode::MultiPlayer:
            break;

        case Mode::UART:
            break;

        case Mode::JOY_BUS:
            break;

        case Mode::General:
            break;
    }
}

auto get_mode(Gba& gba) -> Mode
{
    /*
      SOURCE: https://problemkaputt.de/gbatek.htm#siocontrolregisterssummary

      R.15 R.14 S.13 S.12 Mode
        0    x    0    0    Normal 8bit
        0    x    0    1    Normal 32bit
        0    x    1    0    Multiplay 16bit
        0    x    1    1    UART (RS232)
        1    0    x    x    General Purpose
        1    1    x    x    JOY BUS
    */

    const auto b15 = bit::is_set<15>(REG_RCNT);
    const auto b14 = bit::is_set<14>(REG_RCNT);
    const auto b13 = bit::is_set<13>(REG_SIOCNT);
    const auto b12 = bit::is_set<12>(REG_SIOCNT);

    if (b15 == 0 && b13 == 0 && b12 == 0)
    {
        return Mode::Normal_8bit;
    }
    if (b15 == 0 && b13 == 0 && b12 == 1)
    {
        return Mode::Normal_32bit;
    }
    if (b15 == 0 && b13 == 1 && b12 == 0)
    {
        return Mode::MultiPlayer;
    }
    if (b15 == 1 && b13 == 0 && b12 == 1)
    {
        return Mode::UART;
    }
    if (b15 == 1 && b14 == 0)
    {
        return Mode::General;
    }
    if (b15 == 1 && b14 == 0)
    {
        return Mode::JOY_BUS;
    }

    std::unreachable();
}

auto get_mode_str(Mode mode) -> const char*
{
    switch (mode)
    {
        case Mode::Normal_8bit: return "Normal_8bit";
        case Mode::Normal_32bit: return "Normal_32bit";
        case Mode::MultiPlayer: return "MultiPlayer";
        case Mode::UART: return "UART";
        case Mode::JOY_BUS: return "JOY_BUS";
        case Mode::General: return "General";
    }

    std::unreachable();
}

} // namespace gba::sio
