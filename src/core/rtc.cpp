// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// SOURCE:
// https://problemkaputt.de/gbatek-ds-real-time-clock-rtc.htm
// https://problemkaputt.de/gbatek-gba-cart-real-time-clock-rtc.htm
// https://problemkaputt.de/gbatek-gba-cart-i-o-port-gpio.htm
// https://github.com/pret/pokeemerald/blob/677b4fc394516deab5b5c86c94a2a1443cb52151/src/rtc.c
// https://github.com/pret/pokeemerald/blob/78b0c207388d8915c7fb4a509334abbeb4680d0d/src/siirtc.c
// https://beanmachine.alt.icu/post/rtc/

#include "rtc.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include <cstdio>
#include <cassert>
#include <bit>
#include <utility>
#include <ctime>

namespace gba::rtc {
namespace {

constexpr auto COMMAND_MAGIC = 0b0110;

constexpr u8 COMMAND_LENGTH[]
{
    1 * 8,
    1 * 8,
    7 * 8,
    3 * 8,
    3 * 8,
    3 * 8,
    0 * 8,
    0 * 8,
};

constexpr auto bcd(u8 value) -> u64
{
    // bcd works like this:
    // the upper nibble are the tens (10, 20, 30).
    // the lower nibble are the ones (1...9).
    // so, the biggest number that can be represented is 99 (0b1001'1001)
    assert(value <= 99 && "max value for BCD, what happens here? (mask?)");

    const auto tens = value / 10;
    const auto ones = value % 10;

    return (tens << 4) | ones;
}

} // namespace

auto Rtc::init() -> void
{
    this->bits = 0; // bits shifted in / out
    this->bit_counter = 0; // bit position
    this->pending_bit = false; // pending bit that's been written

    this->control = 0b0100'0000; // enable 24h, thats it

    this->state = State::init1;
    this->command = Command::unused;
}

auto Rtc::write(Gba& gba, u32 addr, u8 value) -> void
{
    const auto SCK = bit::is_set<0>(value);
    const auto SIO = bit::is_set<1>(value);
    const auto CS = bit::is_set<2>(value);

    switch (this->state)
    {
        case State::init1: {
            assert(!CS && SCK && "wrong command sequence!");
            if (!CS && SCK)
            {
                this->bits = 0;
                this->bit_counter = 0;
                this->state = State::init2;
            }
        } break;

        case State::init2: {
            if (CS && SCK)
            {
                this->state = State::command;
            }
        } break;

        // CS has to remian 1 for the whole transfer
        // when SCK=0, write a single bit to pending bit
        // when SCK=1, write the pending bit into bits
        case State::command:
            if (!CS)
            {
                // error (what happens here)
                assert(CS && "CS went low, not sure why");
            }
            else if (SCK == 0)
            {
                this->pending_bit = SIO;
            }
            else if (SCK == 1)
            {
                // "read" bit into bits (aka, write the data)
                this->bits |= static_cast<u64>(this->pending_bit) << this->bit_counter;
                this->bit_counter++;

                // check if we have a full byte
                if (this->bit_counter == 8)
                {
                    // bits 4-7 must be 0b0110, if not then byte swap
                    if (COMMAND_MAGIC != bit::get_range<4, 7>(this->bits))
                    {
                        assert(COMMAND_MAGIC == (bit::get_range<0, 4>(this->bits)) && "invalid command magic");
                        this->bits = bit::reverse2<u8>(this->bits);
                    }

                    // if bit0 = 0, this is a write command
                    // if bit0 = 1, this is a read command
                    if (bit::is_set<0>(this->bits))
                    {
                        this->state = State::read;
                    }
                    else
                    {
                        this->state = State::write;
                    }

                    this->command = static_cast<Command>(bit::get_range<1, 3>(this->bits));
                    this->bits = 0;
                    this->bit_counter = 0;

                    switch (this->command)
                    {
                        case Command::reset:
                            this->control = 0;
                            this->state = State::init1;
                            break;

                        case Command::control:
                            if (this->state == State::read)
                            {
                                this->bits = this->control;
                            }
                            break;

                        case Command::date:
                            if (this->state == State::read)
                            {
                                std::time_t timer;
                                std::time(&timer);
                                const auto tm = std::localtime(&timer);

                                // year range is 2000-2099
                                // tm represents the year since 1900
                                // so for 2022, the value will be 122
                                this->bits |= bcd(tm->tm_year % 100) << 0;
                                // month range is 1-12
                                // tm represents the month as 0-11
                                this->bits |= bcd(tm->tm_mon + 1) << 8;
                                // day range is 1-31
                                // this is the same as rtc
                                this->bits |= bcd(tm->tm_mday) << 16;
                                // day range is 0-11
                                // this is the same as rtc
                                this->bits |= bcd(tm->tm_wday) << 24;

                                this->bits |= bcd(tm->tm_hour) << 32;
                                this->bits |= bcd(tm->tm_min) << 40;
                                this->bits |= bcd(tm->tm_sec) << 48;
                            }
                            break;

                        case Command::time:
                            if (this->state == State::read)
                            {
                                std::time_t timer;
                                std::time(&timer);
                                const auto tm = std::localtime(&timer);

                                this->bits |= bcd(tm->tm_hour) << 0;
                                this->bits |= bcd(tm->tm_min) << 8;
                                this->bits |= bcd(tm->tm_sec) << 16;
                            }
                            break;

                        case Command::alarm1: // (dsi only?)
                            assert(!"invalid command! alarm1");
                            break;

                        case Command::alarm2: // (dsi only?)
                            assert(!"invalid command! alarm1");
                            break;

                        case Command::irq:
                            assert(!"unhandled command [irq]");
                            break;

                        case Command::unused:
                            assert(!"invalid command! unused");
                            break;
                    }
                }
            }
            break;

        case State::read:
            if (!CS)
            {
                this->state = State::init1;
                assert(bit_counter == COMMAND_LENGTH[std::to_underlying(this->command)] && "this will break read as not the full byte is read");
            }
            else if (SCK == 0)
            {
                // what do i do here????????????
                // this->pending_bit = bit::is_set(this->bits, this->bit_counter);
            }
            else if (SCK == 1)
            {
                this->pending_bit = bit::is_set(this->bits, this->bit_counter);
                gba.gpio.data = bit::set<1>(gba.gpio.data, this->pending_bit);
                this->bit_counter++;

                if (bit_counter == COMMAND_LENGTH[std::to_underlying(this->command)])
                {
                    this->state = State::init1;
                }
            }
            break;

        case State::write:
            if (!CS)
            {
                this->state = State::init1;
                assert(bit_counter == COMMAND_LENGTH[std::to_underlying(this->command)] && "this will break writes as not the full byte is written");
            }
            else if (SCK == 0)
            {
                this->pending_bit = SIO;
            }
            else if (SCK == 1)
            {
                // "read" bit into bits (aka, write the data)
                this->bits |= static_cast<u64>(this->pending_bit) << this->bit_counter;
                this->bit_counter++;

                // check if we have a full byte
                if (bit_counter >= COMMAND_LENGTH[std::to_underlying(this->command)])
                {
                    this->state = State::init1;

                    switch (this->command)
                    {
                        case Command::control: {
                            auto data = this->bits;
                            data = bit::unset<0>(data); // unused
                            data = bit::unset<2>(data); // unused
                            data = bit::unset<4>(data); // unused
                            data = bit::unset<7>(data); // r
                            this->control = data;
                            std::printf("control is now: 0x%02X\n", this->control);
                        } break;

                        case Command::reset: assert(!"[WRITE] Command::reset"); break;
                        case Command::date: assert(!"[WRITE] Command::date"); break;
                        case Command::time: assert(!"[WRITE] Command::time"); break;
                        case Command::alarm1: assert(!"[WRITE] Command::alarm1"); break;
                        case Command::alarm2: assert(!"[WRITE] Command::alarm2"); break;
                        case Command::irq: assert(!"[WRITE] Command::irq"); break;
                        case Command::unused: assert(!"[WRITE] Command::unused"); break;
                    }
                }
            }
            break;
    }
}

} // namespace gba::rtc
