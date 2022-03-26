// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <cstdint>

namespace gba::arm7tdmi::arm {

// I don't template this function because it's mostly very unnecessary
// because rlist=0 is not very common case, thus, its a waste of
// icache space.
auto block_data_transfer_empty_rlist(Gba& gba, uint32_t opcode) -> void
{
    auto P = bit::is_set<24>(opcode);
    const auto U = bit::is_set<23>(opcode);
    auto W = bit::is_set<21>(opcode);
    const auto L = bit::is_set<20>(opcode);
    const auto Rn = bit::get_range<16, 19>(opcode);

    auto addr = get_reg(gba, Rn);
    auto final_addr = addr;

    // if set we are incrementing, else, going down
    if (U)
    {
        final_addr = addr + 0x40;
    }
    else
    {
        final_addr = addr - 0x40;
        addr = final_addr;
        if (W)
        {
            set_reg(gba, Rn, final_addr);
        }
        P ^= 1;
        W = 0;
    }

    const auto pre = P ? 4 : 0;

    if (L)
    {
        addr += pre;
        const auto value = mem::read32(gba, addr);
        set_pc(gba, value);
    }
    else
    {
        const auto value = get_pc(gba);
        addr += pre;
        mem::write32(gba, addr, value+4);
    }

    if (W)
    {
        set_reg(gba, Rn, final_addr);
    }
}

// page 82
template<
    bool P2,
    bool U,
    bool S,
    bool W2,
    bool L  // 0=STM, 1=LDM
>
auto block_data_transfer(Gba& gba, uint32_t opcode) -> void
{
    auto P = P2;
    auto W = W2;
    const auto Rn = bit::get_range<16, 19>(opcode);
    auto Rlist = bit::get_range<0, 15>(opcode);

    if (!Rlist)
    {
        // this just simplifies stuff
        gba_log("\tempty rlist in block_data_transfer\n");
        block_data_transfer_empty_rlist(gba, opcode);
        return;
    }

    const auto r15_in_rlist = bit::is_set<PC_INDEX>(Rlist);

    bool did_swap_modes = false;
    auto old_mode = get_mode(gba);

    // this is where the pain begins (it never ends)
    if constexpr(S)
    {
        if (r15_in_rlist && L)
        {
            assert(0);
            // load mode from spsr
            load_spsr_mode_into_cpsr(gba);
        }
        else
        {
            // user bank transfer
            did_swap_modes = true;
            change_mode(gba, old_mode, MODE_USER);
        }
    }

    auto addr = get_reg(gba, Rn);
    auto base = get_reg(gba, Rn);
    auto final_addr = addr;

    // if set we are incrementing, else, going down
    if constexpr(U)
    {
        final_addr = addr + (std::popcount(Rlist) * 4);
    }
    else
    {
        final_addr = (addr - (std::popcount(Rlist) * 4));
        addr = final_addr;
        if (W)
        {
            set_reg(gba, Rn, final_addr);
        }
        P ^= 1;
        W = 0;
    }

    // these are used to avoid having a huge if else tree
    const auto pre = P ? 4 : 0;
    const auto post = P ? 0 : 4;

    // if set, load, else, store
    if constexpr(L)
    {
        while (Rlist)
        {
            const std::uint8_t reg_index = std::countr_zero(Rlist);
            if (reg_index == Rn)
            {
                W = false;
            }

            addr += pre;
            const auto value = mem::read32(gba, addr);
            gba_log("\treading reg: %u from: 0x%08X value: 0x%08X\n", reg_index, addr, value);

            set_reg(gba, reg_index, value);

            addr += post;
            Rlist &= ~(1 << reg_index);
        }
    }
    else
    {
        bool first = true;
        while (Rlist)
        {
            const uint8_t reg_index = std::countr_zero(Rlist);
            auto value = get_reg(gba, reg_index);

            if (reg_index == Rn)
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
            else if (reg_index == PC_INDEX)
            {
                value += 4;
            }

            addr += pre;
            gba_log("\twriting reg: %u to: 0x%08X value: 0x%08X\n", reg_index, addr, get_reg(gba, reg_index));
            mem::write32(gba, addr, value);
            addr += post;

            Rlist &= ~(1 << reg_index);
            first = false;
        }
    }

    // if set, write back
    if (W)
    {
        set_reg(gba, Rn, final_addr);
    }

    if (did_swap_modes)
    {
        change_mode(gba, MODE_USER, old_mode);
    }
}

} // namespace gba::arm7tdmi::arm
