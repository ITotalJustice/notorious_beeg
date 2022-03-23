// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "arm7tdmi/barrel_shifter.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

// page 113 (5.2)
thumb_instruction_template
auto add_subtract(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto I = bit_decoded_is_set(10); // 0=reg, 1=imm
    CONSTEXPR const auto Op = bit_decoded_is_set(9); // 0=ADD, 1=SUB
    #if RELEASE_BUILD == 3
    CONSTEXPR const auto Rn = bit_decoded_get_range(6, 8); // either reg or imm value
    #else
    const auto Rn = bit::get_range<6, 8>(opcode); // either reg or imm value
    #endif
    const auto Rs = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);

    const auto oprand1 = get_reg(gba, Rs);
    const auto oprand2 = I ? Rn : get_reg(gba, Rn);

    // std::printf("[%s] Rd: %u Rn: %u Rs: %u oprand1: 0x%04X oprand2: 0x%04X\n", I ? "SUB" : "ADD", Rd, Rn, Rs, oprand1, oprand2);
    std::uint32_t result = 0;

    if CONSTEXPR (Op) // SUB
    {
        result = internal_sub<true>(gba, oprand1, oprand2);
    }
    else // ADD
    {
        result = internal_add<true>(gba, oprand1, oprand2);
    }

    set_reg_thumb(gba, Rd, result);
}

} // namespace gba::arm7tdmi::thumb
