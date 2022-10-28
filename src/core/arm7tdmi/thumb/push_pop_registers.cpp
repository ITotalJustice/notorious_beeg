// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cassert>

namespace gba::arm7tdmi::thumb {
namespace {

// page 138 (5.14)
template<
    bool L, // 0=push, 1=pop
    bool R  // 0=non, 1=store lr/load pc
>
auto push_pop_registers(Gba& gba, const u16 opcode) -> void
{
    u16 Rlist = bit::get_range<0, 7>(opcode);
    auto addr = get_sp(gba);

    assert((Rlist || R) && "empty rlist edge case");

    if constexpr(L) // pop
    {
        if constexpr(R)
        {
            Rlist = bit::set<PC_INDEX>(Rlist);
        }

        while (Rlist)
        {
            const auto reg_index = std::countr_zero(Rlist);
            const auto value = mem::read32(gba, addr);

            set_reg(gba, reg_index, value);

            addr += 4;
            Rlist &= ~(1 << reg_index);
        }

        set_sp(gba, addr);

        gba.scheduler.tick(1); // todo: verify
    }
    else // push
    {
        if constexpr(R)
        {
            Rlist = bit::set<LR_INDEX>(Rlist);
        }

        // because pop decrements but loads lowest addr first
        // we subract the addr now and count up.
        // SEE: https://github.com/jsmolka/gba-tests/blob/a6447c5404c8fc2898ddc51f438271f832083b7e/thumb/memory.asm#L374
        addr -= std::popcount(Rlist) * 4;
        set_sp(gba, addr);

        while (Rlist)
        {
            const auto reg_index = std::countr_zero(Rlist);
            const auto value = get_reg(gba, reg_index);

            mem::write32(gba, addr, value);

            addr += 4;
            Rlist &= ~(1 << reg_index);
        }
    }
}

} // namespace
} // namespace gba::arm7tdmi::thumb
