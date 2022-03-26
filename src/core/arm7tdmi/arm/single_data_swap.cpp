// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>

namespace gba::arm7tdmi::arm {

// page 89 (4.12)
template<
    bool B // 0=word, 1=byte
>
auto single_data_swap(Gba& gba, uint32_t opcode) -> void
{
    const auto Rn = bit::get_range<16, 19>(opcode); // base address
    const auto Rd = bit::get_range<12, 15>(opcode); // dst
    const auto Rm = bit::get_range<0, 3>(opcode); // src value

    const auto base_address = get_reg(gba, Rn);
    const auto to_mem = get_reg(gba, Rm);

    if constexpr(B) // byte
    {
        const auto to_reg = mem::read8(gba, base_address);
        mem::write8(gba, base_address, to_mem);
        set_reg(gba, Rd, to_reg);
    }
    else // word
    {
        auto to_reg = mem::read32(gba, base_address);
        // rotate missaligned address
        to_reg = std::rotr(to_reg, (base_address & 0x3) * 8);
        mem::write32(gba, base_address, to_mem);
        set_reg(gba, Rd, to_reg);
    }
}

} // namespace gba::arm7tdmi::arm
