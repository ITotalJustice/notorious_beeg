// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/helper.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

enum class hi_register_operations_op
{
    ADD,
    CMP,
    MOV,
    BX,
};

// page 119 (5.5)
thumb_instruction_template
auto hi_register_operations(Gba& gba, uint16_t opcode) -> void
{
    CONSTEXPR const auto Op = bit_decoded_get_range(8, 9);
    #if RELEASE_BUILD == 3
    CONSTEXPR const auto H1 = bit_decoded_is_set(7) ? 8 : 0;
    CONSTEXPR const auto H2 = bit_decoded_is_set(6) ? 8 : 0;
    #else
    const auto H1 = bit::is_set<7>(opcode) ? 8 : 0;
    const auto H2 = bit::is_set<6>(opcode) ? 8 : 0;
    #endif
    const auto Rs = bit::get_range<3, 5>(opcode) | H2;
    const auto Rd = bit::get_range<0, 2>(opcode) | H1;
    CONSTEXPR const auto Decoded_Op = static_cast<hi_register_operations_op>(Op);

    const auto oprand1 = get_reg(gba, Rd);
    const auto oprand2 = get_reg(gba, Rs);

    // note: only CMP sets flags
    if CONSTEXPR (Decoded_Op == hi_register_operations_op::ADD)
    {
        const auto result = internal_add<false>(gba, oprand1, oprand2);
        set_reg(gba, Rd, result);
    }
    else if CONSTEXPR(Decoded_Op == hi_register_operations_op::CMP)
    {
        internal_sub<true>(gba, oprand1, oprand2);
    }
    else if CONSTEXPR(Decoded_Op == hi_register_operations_op::MOV)
    {
        set_reg(gba, Rd, oprand2);
    }
    else if CONSTEXPR(Decoded_Op == hi_register_operations_op::BX)
{
        // if bit0 == 0, switch to arm, else thumb
        if (oprand2 & 1)
        {
            gba_log("[THUMB-BX] switching to THUMB\n");
            CPU.cpsr.T = static_cast<bool>(arm7tdmi::State::THUMB);
            set_pc(gba, oprand2 & ~0x1);
        }
        else
        {
            gba_log("[THUMB-BX] switching to ARM\n");
            CPU.cpsr.T = static_cast<bool>(arm7tdmi::State::ARM);
            set_pc(gba, oprand2 & ~0x3);
        }
    }
}

} // namespace gba::arm7tdmi::thumb
