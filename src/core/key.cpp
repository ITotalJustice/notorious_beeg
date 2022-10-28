// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "key.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include <utility>

namespace gba::key {

void check_key_interrupt(Gba& gba)
{
    const auto key_buttons = ~bit::get_range<0, 9>(REG_KEY);
    const auto interrupt_buttons = bit::get_range<0, 9>(REG_KEYCNT);
    const auto irq_enable = bit::is_set<14>(REG_KEYCNT);
    const auto mode = bit::is_set<15>(REG_KEYCNT);
    const auto already_has_irq = REG_IF & std::to_underlying(arm7tdmi::Interrupt::Key);

    // only check if we want and irq.
    // also check if we already have an irq as not to spam irq requests.
    if (irq_enable && !already_has_irq)
    {
        if (mode == 0) // logical OR (any buttons)
        {
            assert(!"untested key irq logical OR mode");
            if (key_buttons & interrupt_buttons)
            {
                arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::Key);
            }
        }
        else // logical AND (only specific buttons)
        {
            if ((key_buttons & interrupt_buttons) == interrupt_buttons)
            {
                arm7tdmi::fire_interrupt(gba, arm7tdmi::Interrupt::Key);
            }
        }
    }
}

void set_key(Gba& gba, u16 buttons, bool down)
{
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

    check_key_interrupt(gba);
}

} // namespace gba::key
