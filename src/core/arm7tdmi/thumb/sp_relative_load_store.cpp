// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>

namespace gba::arm7tdmi::thumb {

// page 132 (5.11)
template<
    bool L // 0=STR, 1=LDR
>
auto sp_relative_load_store(Gba& gba, u16 opcode) -> void
{
    const auto Rd = bit::get_range<8, 10>(opcode);
    // shifted to 10-bit value (unsigned)
    const auto Word8 = bit::get_range<0, 7>(opcode) << 2;

    const auto addr = get_sp(gba) + Word8;

    if constexpr (L == 0) // STR Rd,[SP, #Imm]
    {
        const auto value = get_reg(gba, Rd);
        mem::write32(gba, addr & ~0x3, value);
    }
    else // LDR Rd,[SP, #Imm]
    {
        auto result = mem::read32(gba, addr & ~0x3);
        result = std::rotr(result, (addr & 0x3) * 8);
        set_reg(gba, Rd, result);
    }
}

} // namespace gba::arm7tdmi::thumb
