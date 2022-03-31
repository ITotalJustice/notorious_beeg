// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"

namespace gba::arm7tdmi::arm {

// T1 = final type (u64/i64)
// T2 = base type (u32/i32) this is used for sign extending
template<
    typename T1, // final type (u64/i64)
    typename T2, // base type (u32/i32) this is used for sign extending
    bool A,
    bool S
>
auto multiply_long(Gba& gba, u32 opcode) -> void
{
    const auto RdHi = bit::get_range<16, 19>(opcode); // dst Hi
    const auto RdLo = bit::get_range<12, 15>(opcode); // dst Lo
    const auto Rs   = bit::get_range<8, 11>(opcode); // oprand
    const auto Rm   = bit::get_range<0, 3>(opcode); // oprand

    const T1 oprand1 = static_cast<T1>(static_cast<T2>(get_reg(gba, Rs)));
    const T1 oprand2 = static_cast<T1>(static_cast<T2>(get_reg(gba, Rm)));

    u64 result = 0;

    if constexpr(A) // MLAL
    {
        const u64 add_a = get_reg(gba, RdHi);
        const u64 add_b = get_reg(gba, RdLo);
        const T1 add_value = (add_a << 32) | add_b;

        result = oprand1 * oprand2 + add_value;
    }
    else
    {
        result = oprand1 * oprand2;
    }

    if constexpr(S) // update flags
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<63>(result);
    }

    set_reg(gba, RdLo, result);
    set_reg(gba, RdHi, result >> 32);
}

// page 67 (4.8)
template<
    bool U, // 0=unsigned, 1=signed
    bool A, // 0=mull, 1=mlal and accumulate
    bool S  // 0=no flags, 1=mod flags
>
auto multiply_long(Gba& gba, u32 opcode) -> void
{
    if constexpr(U) // signed
    {
        multiply_long<s64, s32, A, S>(gba, opcode);
    }
    else // unsigned
    {
        multiply_long<u64, u32, A, S>(gba, opcode);
    }
}

} // namespace gba::arm7tdmi::arm
