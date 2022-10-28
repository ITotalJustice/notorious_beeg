// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>

namespace gba::arm7tdmi::thumb {
namespace {

// page 124 (5.7)
template<
    bool B, // 0=word, 1=byte
    bool L  // 0=STR, 1=LDR
>
auto load_store_with_immediate_offset(Gba& gba, const u16 opcode) -> void
{
    const auto Rb = bit::get_range<3, 5>(opcode);
    const auto Rd = bit::get_range<0, 2>(opcode);
    const auto offset = bit::get_range<6, 10>(opcode);

    const auto base = get_reg(gba, Rb);

    if constexpr(L) // LDR
    {
        u32 result = 0;

        if constexpr(B) // byte
        {
            const auto addr = base + offset;
            result = mem::read8(gba, addr);
        }
        else // word
        {
            const auto addr = base + (offset << 2);
            result = mem::read32(gba, addr);
            result = std::rotr(result, (addr & 0x3) * 8);
        }

        set_reg_thumb(gba, Rd, result);

        gba.scheduler.tick(1); // todo: verify
    }
    else // STR
    {
        const auto value = get_reg(gba, Rd);

        if constexpr(B) // byte
        {
            const auto addr = base + offset;
            mem::write8(gba, addr, value);
        }
        else // word
        {
            const auto addr = base + (offset << 2);
            mem::write32(gba, addr, value);
        }
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
