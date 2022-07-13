// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <cassert>
#include <concepts>
#include <bit>

namespace bit {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

template<typename T>
concept IntV = std::is_integral_v<T>;

template<IntV T> [[nodiscard]]
consteval auto get_mask() -> T
{
    if constexpr(sizeof(T) == sizeof(u8))
    {
        return 0xFF;
    }
    else if constexpr(sizeof(T) == sizeof(u16))
    {
        return 0xFF'FF;
    }
    else if constexpr(sizeof(T) == sizeof(u32))
    {
        return 0xFF'FF'FF'FF;
    }
    else if constexpr(sizeof(T) == sizeof(u64))
    {
        return 0xFF'FF'FF'FF'FF'FF'FF'FF;
    }
}

static_assert(
    bit::get_mask<u8>() == 0xFF &&
    bit::get_mask<u16>() == 0xFF'FF &&
    bit::get_mask<u32>() == 0xFF'FF'FF'FF &&
    bit::get_mask<u64>() == 0xFF'FF'FF'FF'FF'FF'FF'FF,
    "bit::get_mask is broken!"
);

template<u8 start, u8 end, IntV T> [[nodiscard]]
consteval auto get_mask() -> T
{
    static_assert(start <= end);
    static_assert(end < (sizeof(T) * 8));

    T result = 0;
    for (auto bit = start; bit <= end; ++bit)
    {
        result |= (1 << bit);
    }

    return result;
}

static_assert(
    bit::get_mask<3, 5, u32>() == 0b111'000 &&
    bit::get_mask<0, 2, u32>() == 0b000'111 &&
    bit::get_mask<1, 5, u32>() == 0b111'110 &&
    bit::get_mask<4, 5, u32>() == 0b110'000,
    "bit::get_mask is broken!"
);

// bit value should probably be either masked or, in debug mode,
// check if bit is ever >= 32, if so, throw.
template<IntV T> [[nodiscard]]
constexpr auto is_set(const T value, const u8 bit) -> bool
{
    assert(bit < (sizeof(T) * 8) && "bit value out of bounds!");
    return !!(value & (1ULL << bit));
}

// compile-time bit-size checked checked alternitives
template<u8 bit, IntV T> [[nodiscard]]
constexpr auto is_set(const T value) -> bool
{
    static_assert(bit < (sizeof(T) * 8), "bit value out of bounds!");
    constexpr auto test_bit = 1ULL << bit;
    return !!(value & test_bit);
}

static_assert(
    bit::is_set<0>(0b1) &&
    !bit::is_set<1>(0b0) &&
    bit::is_set<1>(0b10) &&
    !bit::is_set<0>(0b10) &&
    "bit::is_set is broken!"
);

template<u8 bit, IntV T> [[nodiscard]]
constexpr auto set(const T value, const bool on = true) -> T
{
    constexpr auto bit_width = sizeof(T) * 8;

    static_assert(bit < bit_width, "bit value out of bounds!");

    // create a mask with all bits set apart from THE `bit`
    // this allows toggling the bit.
    // if on = true, the bit is set, else nothing is set
    constexpr auto mask = bit::get_mask<T>() & (~(1ULL << bit));

    return (value & mask) | (static_cast<T>(on) << bit);
}

// unused
// template<IntV T> [[nodiscard]]
// constexpr auto set(const T value, const u8 bit, const bool on = true) -> T
// {
//     constexpr auto bit_width = sizeof(T) * 8;

//     assert(bit < bit_width && "bit value out of bounds!");

//     // create a mask with all bits set apart from THE `bit`
//     // this allows toggling the bit.
//     // if on = true, the bit is set, else nothing is set
//     const auto mask = bit::get_mask<T>() & (~(1ULL << bit));

//     return (value & mask) | (static_cast<T>(on) << bit);
// }

static_assert(
    bit::set<0>(0b1100, true) == 0b1101 &&
    bit::set<3>(0b1101, false) == 0b0101 &&

    bit::set<0>(0b00, true) == 0b01 &&
    bit::set<0>(0b01, false) == 0b00,

    "bit::set is broken!"
);

template<u8 bit, IntV T> [[nodiscard]]
constexpr auto unset(const T value) -> T
{
    return bit::set<bit>(value, false);
}

// unused
// template<IntV T> [[nodiscard]]
// constexpr auto unset(const T value, const u8 bit) -> T
// {
//     return bit::set(value, bit, false);
// }

static_assert(
    bit::unset<0>(0b1101) == 0b1100 &&
    bit::unset<3>(0b1101) == 0b0101 &&

    bit::unset<0>(0b01) == 0b00 &&
    bit::unset<1>(0b01) == 0b01,

    "bit::unset is broken!"
);

template<u8 bit_width> [[nodiscard]]
constexpr auto sign_extend(const s32 value) -> s32
{
    static_assert(bit_width <= 31, "bit_width is out of bounds!");

    constexpr u8 bits = 31 - bit_width;
    return (value << bits) >> bits;
}

static_assert(
    // ensure that sign extending does work!
    static_cast<s8>(0xFF) == -1 &&
    static_cast<s32>(static_cast<s8>(0xFF)) == static_cast<s32>(0xFFFFFFFF) &&

    // same as above but using my
    bit::sign_extend<7>(0xFF) == -1 &&
    bit::sign_extend<7>(0xFF) == static_cast<s32>(0xFFFFFFFF) &&

    // simple 24-bit asr
    bit::sign_extend<23>(0b1100'1111'1111'1111'1111'1111) == static_cast<s32>(0b1111'1111'1100'1111'1111'1111'1111'1111) &&
    // set the sign-bit to bit 1, then asr 31-bits
    bit::sign_extend<0>(1) == static_cast<s32>(0b1111'1111'1111'1111'1111'1111'1111'1111) &&
    // this is used in thumb ldr halword sign
    bit::sign_extend<15>(0b0000'0000'1110'0000'1111'1111'1111'1111) == static_cast<s32>(0b1111'1111'1111'1111'1111'1111'1111'1111) &&
    // same as above but no sign
    bit::sign_extend<15>(0b0000'0000'1110'0000'0111'1111'1111'1111) == static_cast<s32>(0b0000'0000'0000'0000'0111'1111'1111'1111),
    "sign_extend is broken!"
);

static_assert(
    bit::sign_extend<0>(0b1) < 0 &&
    bit::sign_extend<1>(0b10) < 0,
    "sign_extend does result not negative"
);

template<u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto get_range(const T value)
{
    static_assert(start <= end, "range is inverted! remember its lo, hi");
    static_assert(end < (sizeof(T) * 8));

    constexpr auto mask = bit::get_mask<start, end, T>() >> start;

    return (value >> start) & mask;
}

static_assert(
    bit::get_range<3, 5>(0b111'000) == 0b000'111 &&
    bit::get_range<0, 2>(0b000'010) == 0b000'010 &&
    bit::get_range<1, 5>(0b111'110) == 0b011'111 &&
    bit::get_range<4, 5>(0b110'000) == 0b000'011,
    "bit::get_range is broken!"
);

// the below are new functions, not fully tested
// also, the name "set" can now set a range of values
// maybe i should replace get_range<>() with just get<>().
template<u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto set(const T value, const u32 new_v) -> T
{
    static_assert(start <= end, "range is inverted! remember its lo, hi");
    static_assert(end < (sizeof(T) * 8));

    constexpr auto value_mask = bit::get_mask<start, end, T>();
    constexpr auto new_v_mask = bit::get_mask<start, end, T>() >> start;

    return (value & ~value_mask) | ((new_v & new_v_mask) << start);
}

template<u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto unset(const T value) -> T
{
    return bit::set<start, end>(value, 0);
}

static_assert(
    // simple unset test
    bit::unset<0, 0>(0x1) == 0x0 &&
    bit::unset<0, 0>(0x2) == 0x2 &&

    // same as above, different range
    bit::unset<6, 7>(0xC0) == 0x0 &&
    bit::unset<6, 7>(0xC1) == 0x1 &&

    bit::unset<0, 15>(0xFFFF) == 0x0 &&
    bit::unset<0, 14>(0xFFFF) == 0x8000 &&
    bit::unset<1, 14>(0xFFFF) == 0x8001 &&
    bit::unset<2, 14>(0xFFFF) == 0x8003,

    "bit::unset is broken!"
);

static_assert(
    // simple bit set test
    bit::set<0, 0>(0, 0x1) == 0x1 &&
    bit::set<0, 0>(0, 0x0) == 0x0 &&

    // same as above, different range
    bit::set<6, 7>(0, 0x3) == 0xC0 &&
    bit::set<6, 7>(1, 0x3) == 0xC1 &&

    // tests the the new value mask is working
    bit::set<0, 0>(0, 0xF) == 0x1 &&
    bit::set<0, 0>(0, 0xE) == 0x0,

    "bit::set is broken!"
);

// reverses the bits
template<IntV T> [[nodiscard]]
constexpr auto reverse(const T data)
{
    constexpr auto bit_width = sizeof(T) * 8;
    T output{};

    for (std::size_t i = 0; i < bit_width; i++)
    {
        output |= bit::is_set(data, bit_width - 1 - i) << i;
    }

    return output;
}

// same as above but without a loop (recursion is compile-time only)
template<IntV T, u8 Counter = 0> [[nodiscard]]
constexpr auto reverse2(const T data, T output = 0)
{
    constexpr auto bit_width = sizeof(T) * 8;

    if constexpr (Counter == bit_width)
    {
        return output;
    }
    else
    {
        const auto result = bit::is_set<bit_width - 1 - Counter>(data);
        output = bit::set<Counter>(output, result);
        return reverse2<T, Counter + 1>(data, output);
    }
}

static_assert(
    bit::reverse<u8>(0b0000'0001) == 0b1000'0000 &&
    bit::reverse<u8>(0b1000'0000) == 0b0000'0001 &&

    bit::reverse<u8>(0b0000'1111) == 0b1111'0000 &&
    bit::reverse<u8>(0b1111'0000) == 0b0000'1111 &&

    bit::reverse<u8>(0b0000'0110) == 0b0110'0000 &&
    bit::reverse<u8>(0b0110'0000) == 0b0000'0110,

    "bit::reverse is broken!"
);

static_assert(
    bit::reverse2<u8>(0b0000'0001) == 0b1000'0000 &&
    bit::reverse2<u8>(0b1000'0000) == 0b0000'0001 &&

    bit::reverse2<u8>(0b0000'1111) == 0b1111'0000 &&
    bit::reverse2<u8>(0b1111'0000) == 0b0000'1111 &&

    bit::reverse2<u8>(0b0000'0110) == 0b0110'0000 &&
    bit::reverse2<u8>(0b0110'0000) == 0b0000'0110,

    "bit::reverse2 is broken!"
);
} // namespace bit
