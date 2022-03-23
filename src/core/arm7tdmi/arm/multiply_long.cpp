// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace gba::arm7tdmi::arm {

// T1 = final type (u64/i64)
// T2 = base type (u32/i32) this is used for sign extending
#if RELEASE_BUILD_ARM == 1
template<typename T1, typename T2, std::uint16_t decoded_opcode>
#else
template<typename T1, typename T2>
#endif
auto multiply_long(Gba& gba, std::uint32_t opcode) -> void
{
    CONSTEXPR_ARM const auto A = bit_decoded_is_set_arm(21); // 0=mull, 1=mlal and accumulate
    CONSTEXPR_ARM const auto S = bit_decoded_is_set_arm(20); // 0=no flags, 1=mod flags
    const auto RdHi = bit::get_range<16, 19>(opcode); // dst Hi
    const auto RdLo = bit::get_range<12, 15>(opcode); // dst Lo
    const auto Rs   = bit::get_range<8, 11>(opcode); // oprand
    const auto Rm   = bit::get_range<0, 3>(opcode); // oprand

    const T1 oprand1 = static_cast<T1>(static_cast<T2>(get_reg(gba, Rs)));
    const T1 oprand2 = static_cast<T1>(static_cast<T2>(get_reg(gba, Rm)));

    u64 result = 0;

    if CONSTEXPR_ARM (A) // MLAL
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

    // update flags
    if CONSTEXPR_ARM (S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<63>(result);
    }

    set_reg(gba, RdLo, result);
    set_reg(gba, RdHi, result >> 32);
}

// page 67 (4.8)
arm_instruction_template
auto multiply_long(Gba& gba, uint32_t opcode) -> void
{
    CONSTEXPR_ARM const auto U = bit_decoded_is_set_arm(22); // 0=unsigned, 1=signed

    #if RELEASE_BUILD_ARM == 1
    if CONSTEXPR_ARM (U) // signed
    {
        multiply_long<std::int64_t, std::int32_t, decoded_opcode>(gba, opcode);
    }
    else // unsigned
    {
        multiply_long<u64, std::uint32_t, decoded_opcode>(gba, opcode);
    }
    #else
    if (U) // signed
    {
        multiply_long<std::int64_t, std::int32_t>(gba, opcode);
    }
    else // unsigned
    {
        multiply_long<u64, std::uint32_t>(gba, opcode);
    }
    #endif
}

} // namespace gba::arm7tdmi::arm
