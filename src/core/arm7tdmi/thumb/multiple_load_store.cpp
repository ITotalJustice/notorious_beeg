// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace gba::arm7tdmi::thumb {

template<
    bool L // 0=store, 1=load
>
auto multiple_load_store_empty_rlist(Gba& gba, uint16_t opcode) -> void
{
    const auto Rb = bit::get_range<8, 10>(opcode); // base
    auto addr = get_reg(gba, Rb);

    if constexpr (L)
    {
        const auto value = mem::read32(gba, addr);
        set_pc(gba, value);
    }
    else
    {
        // pc is ahead by 6
        const auto value = get_pc(gba) + 0x2;
        mem::write32(gba, addr, value);
    }

    set_reg_thumb(gba, Rb, addr + 0x40);
}

// page 140 (5.15)
template<
    bool L // 0=store, 1=load
>
auto multiple_load_store(Gba& gba, uint16_t opcode) -> void
{
    const auto Rb = bit::get_range<8, 10>(opcode); // base
    std::uint16_t Rlist = bit::get_range<0, 7>(opcode);
    auto addr = get_reg(gba, Rb);

    if (!Rlist)
    {
        multiple_load_store_empty_rlist<L>(gba, opcode);
        return;
    }

    if constexpr (L) // load
    {
        bool write_back = true;

        while (Rlist)
        {
            const auto reg_index = std::countr_zero(Rlist);
            const auto value = mem::read32(gba, addr);

            if (reg_index == Rb)
            {
                write_back = false;
            }

            set_reg_thumb(gba, reg_index, value);

            addr += 4;
            Rlist &= ~(1 << reg_index);
        }

        if (write_back)
        {
            set_reg(gba, Rb, addr);
        }
    }
    else // store
    {
        const auto base = get_reg(gba, Rb);
        const auto final_addr = base + (std::popcount(Rlist) * 4);
        bool first = true;

        while (Rlist)
        {
            const auto reg_index = std::countr_zero(Rlist);
            auto value = get_reg(gba, reg_index);

            if (reg_index == Rb)
            {
                if (first)
                {
                    value = base;
                }
                else
                {
                    value = final_addr;
                }
            }

            mem::write32(gba, addr, value);

            addr += 4;
            Rlist &= ~(1 << reg_index);
            first = false;
        }

        set_reg_thumb(gba, Rb, addr);
    }
}

} // namespace gba::arm7tdmi::thumb
