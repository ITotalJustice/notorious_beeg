// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cassert>

// https://problemkaputt.de/gbatek.htm#armopcodespsrtransfermrsmsr
// gbatek documents a few extra options, such as only setting
// control flags (cpsr_c).
namespace gba::arm7tdmi::arm {

// page 61
template<
    bool I, // 0=reg, 1=imm
    bool P  // 0=cpsr, 1=spsr
>
static auto msr(Gba& gba, u32 opcode) -> void
{
    const auto F = bit::is_set<19>(opcode); // write to flags
    const auto C = bit::is_set<16>(opcode); // write to control

    const auto oprand = [&]()
    {
        if constexpr(I == 0) // reg
        {
            const auto Rm = bit::get_range<0, 3>(opcode);
            return get_reg(gba, Rm);
        }
        else
        {
            const auto imm = bit::get_range<0, 7>(opcode);
            // rotated by twice the value
            const auto rotate = bit::get_range<8, 11>(opcode) * 2;
            return std::rotr(imm, rotate);
        }
    }();

    if constexpr(P)
    {
        set_spsr_from_u32(gba, oprand, F, C);
    }
    else
    {
        set_cpsr_from_u32(gba, oprand, F, C);
    }
}
} // namespace gba::arm7tdmi::arm
