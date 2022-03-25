// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

// page 122 (5.6)
auto pc_relative_load(Gba& gba, uint16_t opcode) -> void
{
    const auto Rd = bit::get_range<8, 10>(opcode);
    // shifted to 10-bit value (unsigned)
    const auto Word8 = bit::get_range<0, 7>(opcode) << 2;
    const auto pc = get_pc(gba) & ~0x3;

    const auto result = mem::read32(gba, pc + Word8);
    set_reg_thumb(gba, Rd, result);

    gba_log("\tpc_relative_load Rd: %u Word8: 0x%04X pc: 0x%08X new_pc: 0x%08X\n", Rd, Word8, pc, result);
}

} // namespace gba::arm7tdmi::thumb
