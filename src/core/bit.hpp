// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <cassert>
#include <concepts>
#include <bit>

namespace gba::bit {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

template <typename T>
concept IntV = std::is_integral_v<T>;

template <IntV T> [[nodiscard]]
consteval auto get_mask() -> T {
    if constexpr(sizeof(T) == sizeof(u8)) {
        return 0xFF;
    }
    if constexpr(sizeof(T) == sizeof(u16)) {
        return 0xFF'FF;
    }
    if constexpr(sizeof(T) == sizeof(u32)) {
        return 0xFF'FF'FF'FF;
    }
    if constexpr(sizeof(T) == sizeof(u64)) {
        return 0xFF'FF'FF'FF'FF'FF'FF'FF;
    }
}

// [[nodiscard]]
// constexpr u32 rotr(const u32 value, const u32 shift) {
//     return std::rotr(value, shift);
// }

// static_assert(
//     rotr(0b1111'1111'1111'1111'1111'1111'1111'1111, 2) == 0b1111'1111'1111'1111'1111'1111'1111'1111 &&
//     rotr(0b0011'1111'1111'1111'1111'1111'1111'1111, 2) == 0b1100'1111'1111'1111'1111'1111'1111'1111 &&
//     // all 1's
//     rotr(0b1111'1111'1111'1111'1111'1111'1111'1111, 69) == 0b1111'1111'1111'1111'1111'1111'1111'1111 &&
//     // same as rotr(x, 2)
//     rotr(0b0011'1111'1111'1111'1111'1111'1111'1111, 34) == 0b1100'1111'1111'1111'1111'1111'1111'1111,
//     "bit::rotr is broken!"
// );

// bit value should probably be either masked or, in debug mode,
// check if bit is ever >= 32, if so, throw.
template <IntV T> [[nodiscard]]
constexpr auto is_set(const T value, const u32 bit) -> bool {
    assert(bit < (sizeof(T) * 8) && "bit value out of bounds!");
    return !!(value & (1ULL << bit));
}

// these are broken!! no mask range is applied!
// template <IntV T> [[nodiscard]]
// constexpr T set(const T value, const u32 bit, const bool on) {
//     assert(bit < (sizeof(T) * 8) && "bit value out of bounds!");
//     return value | (on << bit);
// }

// template <IntV T> [[nodiscard]]
// constexpr T unset(const T value, const u32 bit) {
//     assert(bit < (sizeof(T) * 8) && "bit value out of bounds!");
//     return value & (~(1ULL << bit));
// }

// compile-time bit-size checked checked alternitives
template <u8 bit, IntV T> [[nodiscard]]
constexpr auto is_set(const T value) -> bool {
    static_assert(bit < (sizeof(T) * 8), "bit value out of bounds!");
    constexpr auto test_bit = 1ULL << bit;
    return !!(value & test_bit);
}

template <u8 bit, IntV T> [[nodiscard]]
constexpr auto set(const T value, const bool on) -> T {
    constexpr auto bit_width = sizeof(T) * 8;

    static_assert(bit < bit_width, "bit value out of bounds!");

    // create a mask with all bits set apart from THE `bit`
    // this allows toggling the bit.
    // if on = true, the bit is set, else nothing is set
    constexpr auto mask = get_mask<T>() & (~(1ULL << bit));

    return (value & mask) | (static_cast<T>(on) << bit);
}

static_assert(
    set<0, uint16_t>(0b00, true) == 0b01 &&
    set<0, uint16_t>(0b01, false) == 0b00
);

template <u8 bit, IntV T> [[nodiscard]]
constexpr auto unset(const T value) -> T {
    constexpr auto bit_width = sizeof(T) * 8;
    static_assert(bit < bit_width, "bit value out of bounds!");

    constexpr auto mask = ~(1ULL << bit);

    return value & mask;
}

static_assert(
    bit::set<0, u8>(0b1100, true) == 0b1101 &&
    bit::set<3, u8>(0b1101, false) == 0b0101,
    "bit::set is broken!"
);

// NOTE: the bit<> starts at 1!
// so for bit0, use bit<1> instead!
template <u8 start_size> [[nodiscard]]
constexpr auto sign_extend(const u32 value) -> s32 {
    static_assert(start_size != 0, "bad start size");
    static_assert(start_size <= 31, "bit start size is out of bounds!");

    const u8 bits = 32 - start_size;
    return static_cast<s32>(value << bits) >> bits;
}

template <u8 start_size, u32 value> [[nodiscard]]
consteval auto sign_extend() -> s32 {
    static_assert(start_size <= 31, "bit start size is out of bounds!");

    const u8 bits = 32 - start_size;
    return static_cast<s32>(value << bits) >> bits;
}

static_assert(
    // simple 24-bit asr
    bit::sign_extend<24>(0b1100'1111'1111'1111'1111'1111) == (s32)0b1111'1111'1100'1111'1111'1111'1111'1111 &&
    // set the sign-bit to bit 1, then asr 31-bits
    bit::sign_extend<1>(0b0001) == (s32)0b1111'1111'1111'1111'1111'1111'1111'1111 &&
    // this is used in thumb ldr halword sign
    bit::sign_extend<16>(0b0000'0000'1110'0000'1111'1111'1111'1111) == (s32)0b1111'1111'1111'1111'1111'1111'1111'1111 &&
    // same as above but no sign
    bit::sign_extend<16>(0b0000'0000'1110'0000'0111'1111'1111'1111) == (s32)0b0000'0000'0000'0000'0111'1111'1111'1111,
    "sign_extend is broken!"
);

static_assert(
    bit::sign_extend<1>(0b1) < 0 &&
    bit::sign_extend<2>(0b10) < 0,
    "sign_extend does result not negative"
);

template <u8 start, u8 end, IntV T> [[nodiscard]]
consteval auto get_mask() -> T {
    static_assert(start < end);
    static_assert(end < (sizeof(T) * 8));

    T result = 0;
    for (auto bit = start; bit <= end; ++bit) {
        result |= (1 << bit);
    }

    return result;
}

template <u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto mask(T value) -> T {
    return value & get_mask<start, end, T>();
}

static_assert(
    bit::get_mask<3, 5, u32>() == 0b111'000 &&
    bit::get_mask<0, 2, u32>() == 0b000'111 &&
    bit::get_mask<1, 5, u32>() == 0b111'110 &&
    bit::get_mask<4, 5, u32>() == 0b110'000,
    "bit::get_mask is broken!"
);

template <u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto get_range(const T value) {
    static_assert(start < end, "range is inverted! remember its lo, hi");
    static_assert(end < (sizeof(T) * 8));

    constexpr auto mask = get_mask<start, end, T>() >> start;

    return (value >> start) & mask;
}

static_assert(
    bit::get_range<3, 5, u32>(0b111'000) == 0b000'111 &&
    bit::get_range<0, 2, u32>(0b000'010) == 0b000'010 &&
    bit::get_range<1, 5, u32>(0b111'110) == 0b011'111 &&
    bit::get_range<4, 5, u32>(0b110'000) == 0b000'011,
    "bit::get_range is broken!"
);

// template <u8 start, u8 end, IntV Int, IntV Slice> [[nodiscard]]
// constexpr auto set_range(const Int value, const Slice slice) {
//     static_assert(start < end, "range is invalid!");
//     static_assert(end < (sizeof(Int) * 8));

//     // invert them as we want to set the range
//     constexpr auto mask = get_mask<start, end, Int>();
//     constexpr auto inverted_mask = ~mask;

//     return (value & inverted_mask) | ((slice << start) & mask);
// }

// constexpr auto TTT = bit::set_range<6, 7, u8, u8>(0b11111111, 7);

// static_assert(
//     bit::set_range<3, 5, u8, u8>(0b0, 0b1) == 0xFF &&
//     "bit::set_range is broken!"
// );

} // namespace gba::bit
