// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "gba.hpp"
#include "arm7tdmi/arm7tdmi.hpp"

namespace gba::arm7tdmi {

template<bool S>
auto internal_add(Gba& gba, uint32_t a, uint32_t b, bool carry = false) -> uint32_t
{
    const uint32_t result = a + b + carry;

    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = (static_cast<uint64_t>(a) + static_cast<uint64_t>(b) + carry) > UINT32_MAX;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, b, result);
    }

    return result;
}

template<bool S>
auto internal_sub(Gba& gba, uint32_t a, uint32_t b, bool carry = false) -> uint32_t
{
    const uint32_t result = a - (b + carry);

    // only update cpsr if S bit was set
    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = !(a < (b + carry));
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, ~b, result);
    }

    return result;
}

template<bool S>
auto set_logical_flags(Gba& gba, std::uint32_t result, bool carry) -> void
{
    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = carry;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
}

// same as above but doesnt modify carry
template<bool S>
auto set_logical_flags2(Gba& gba, std::uint32_t result) -> void
{
    if constexpr(S)
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
}

#if RELEASE_BUILD_ARM == 1
#define set_logical_flags_arm(gba, S, result, new_carry) set_logical_flags<S>(gba, result, new_carry)
#define internal_add_arm(gba, S, a, b) internal_add<S>(gba, a, b)
#define internal_addc_arm(gba, S, a, b, c) internal_add<S>(gba, a, b, c)
#define internal_sub_arm(gba, S, a, b) internal_sub<S>(gba, a, b)
#define internal_subc_arm(gba, S, a, b, c) internal_sub<S>(gba, a, b, c)
#else
#define set_logical_flags_arm(gba, S, result, new_carry) set_logical_flags(gba, S, result, new_carry)
#define internal_add_arm(gba, S, a, b) internal_add(gba, S, a, b)
#define internal_addc_arm(gba, S, a, b, c) internal_add(gba, S, a, b, c)
#define internal_sub_arm(gba, S, a, b) internal_sub(gba, S, a, b)
#define internal_subc_arm(gba, S, a, b, c) internal_sub(gba, S, a, b, c)
#endif
} // namespace gba::arm7tdmi
