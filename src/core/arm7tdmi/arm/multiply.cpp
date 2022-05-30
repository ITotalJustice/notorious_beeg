// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <cassert>

namespace gba::arm7tdmi::arm {
namespace {

// page 65 (4.7)
template<
    bool A, // 0=mul, 1=mul and accumulate
    bool S  // 0=no flags, 1=mod flags
>
auto multiply(Gba& gba, u32 opcode) -> void
{
    const auto Rd = bit::get_range<16, 19>(opcode); // dst
    const auto Rn = bit::get_range<12, 15>(opcode); // oprand
    const auto Rs = bit::get_range<8, 11>(opcode); // oprand
    const auto Rm = bit::get_range<0, 3>(opcode); // oprand

    assert(Rd != PC_INDEX && "Rd cannot be r15!");
    assert(PC_INDEX != Rn && PC_INDEX != Rs && PC_INDEX != Rm && "oprand cannot be r15!");

    const auto a = get_reg(gba, Rm);
    const auto b = get_reg(gba, Rs);
    const auto c = A ? get_reg(gba, Rn) : 0;

    const u32 result = a * b + c;

    if constexpr(S)
    {
        // todo: carry is set to random value???
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<31>(result);
    }

    set_reg(gba, Rd, result);
}

} // namespace
} // namespace gba::arm7tdmi::arm
