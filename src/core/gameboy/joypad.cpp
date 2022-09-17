#include "gb.hpp"
#include "internal.hpp"

#include "gba.hpp"

namespace gba::gb {
namespace {

inline auto is_directional(const Gba& gba) -> bool
{
    return !(IO_JYP & 0x10);
}

inline auto is_button(const Gba& gba) -> bool
{
    return !(IO_JYP & 0x20);
}

} // namespace

// [API]
void set_buttons(Gba& gba, u8 buttons, bool is_down)
{
    // the pins go LOW when pressed!
    if (is_down)
    {
        if (gba.gameboy.joypad.var != (gba.gameboy.joypad.var & ~buttons))
        {
            // this isn't correct impl, it needs to fire when going hi to lo.
            // but it's good enough for now.
            enable_interrupt(gba, INTERRUPT_JOYPAD);
        }

        gba.gameboy.joypad.var &= ~buttons;
    }
    else
    {
        gba.gameboy.joypad.var |= buttons;
    }

    // the direction keys were made so that the opposite key could not
    // be pressed at the same time, ie, left and right cannot both be pressed.
    // allowing this probably doesnt break any games, but does cause strange
    // effects in some games, such as zelda, pressing up and down will cause
    // link to walk in-place whilst holding a shield, even if link doesn't
    // yet have the shield...

    // this can be better optimised at some point
    if (is_down && (buttons & BUTTON_DIRECTIONAL))
    {
        if (buttons & BUTTON_RIGHT)
        {
            gba.gameboy.joypad.var |= BUTTON_LEFT;
        }
        if (buttons & BUTTON_LEFT)
        {
            gba.gameboy.joypad.var |= BUTTON_RIGHT;
        }
        if (buttons & BUTTON_UP)
        {
            gba.gameboy.joypad.var |= BUTTON_DOWN;
        }
        if (buttons & BUTTON_DOWN)
        {
            gba.gameboy.joypad.var |= BUTTON_UP;
        }
    }
}

auto get_buttons(const Gba& gba) -> u8
{
    return gba.gameboy.joypad.var;
}

auto is_button_down(const Gba& gba, enum Button button) -> bool
{
    return (gba.gameboy.joypad.var & button) == 0;
}

void joypad_write(Gba& gba, u8 value)
{
    // unset p14 and p15
    IO_JYP &= ~0x30;
    // only p14 and p15 are writable, also OR in unused bits
    IO_JYP |= 0xCF | (0x30 & value);

    // CREDIT: thanks to Calindro for the below code.
    // turns out that both p14 and p15 can be low (selected),
    // in which case, reading pulls from both button and directional
    // lines.

    // for example, if A is low and RIGHT is high, reading will be low.
    // this was noticed in [bomberman GB] where both p14 and p15 were low.
    // SEE: https://github.com/ITotalJustice/TotalGB/issues/41

    if (is_button(gba))
    {
        if (is_button_down(gba, BUTTON_A))
        {
            IO_JYP &= ~0x01;
        }
        if (is_button_down(gba, BUTTON_B))
        {
            IO_JYP &= ~0x02;
        }
        if (is_button_down(gba, BUTTON_SELECT))
        {
            IO_JYP &= ~0x04;
        }
        if (is_button_down(gba, BUTTON_START))
        {
            IO_JYP &= ~0x08;
        }
    }

    if (is_directional(gba))
    {
        if (is_button_down(gba, BUTTON_RIGHT))
        {
            IO_JYP &= ~0x01;
        }
        if (is_button_down(gba, BUTTON_LEFT))
        {
            IO_JYP &= ~0x02;
        }
        if (is_button_down(gba, BUTTON_UP))
        {
            IO_JYP &= ~0x04;
        }
        if (is_button_down(gba, BUTTON_DOWN))
        {
            IO_JYP &= ~0x08;
        }
    }
}

} // namespace gba::gb
