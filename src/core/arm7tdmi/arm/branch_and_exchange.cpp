// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include <cstdint>

namespace gba::arm7tdmi::arm {

// page 48 (4.3)
auto branch_and_exchange(Gba& gba, uint32_t opcode) -> void
{
    const auto Rn = bit::get_range<0, 3>(opcode);
    const auto addr = get_reg(gba, Rn);

    // if bit0 == 0, switch to arm, else thumb
    if (addr & 1)
    {
        gba_log("[ARM-BX] switching to THUMB Rn: %u addr: 0x%08X\n", Rn, addr);
        CPU.cpsr.T = static_cast<bool>(gba::arm7tdmi::State::THUMB);
        set_pc(gba, addr & ~0x1);
    }
    else
    {
        gba_log("[ARM-BX] switching to ARM Rn: %u addr: 0x%08X\n", Rn, addr);
        CPU.cpsr.T = static_cast<bool>(gba::arm7tdmi::State::ARM);
        set_pc(gba, addr & ~0x3);
    }
}

} // namespace gba::arm7tdmi::arm
