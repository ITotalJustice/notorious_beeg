// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include <cstddef>
#include <cstdio>
#include <gba.hpp>
#include <mem.hpp>
#include <memory>

namespace {

using namespace gba;

struct TestData
{
    s32 ticks;
    u16 counter;
    u16 expected;
};

constexpr TestData TEST_DATA[] =
{
    {}, // ignore index 0
    {
        .ticks = 0,
        .counter = 0x0000,
        .expected = 0x0000,
    },
    {
        .ticks = 1,
        .counter = 0x0000,
        .expected = 0x0000,
    },
    {
        .ticks = 2,
        .counter = 0x0000,
        .expected = 0x0001,
    },
    {
        .ticks = 4,
        .counter = 0x0000,
        .expected = 0x0003,
    },
    {
        .ticks = 0,
        .counter = 0xFFF0,
        .expected = 0xFFF0,
    },
    {
        .ticks = 10,
        .counter = 0xFFF0,
        .expected = 0xFFF9,
    },
    {
        .ticks = 18,
        .counter = 0xFFF0,
        .expected = 0xFFF1, // overflow
    },
    {
        .ticks = 34,
        .counter = 0xFFF0,
        .expected = 0xFFF1, // overflow twice
    },
};

auto test(Gba& gba, u16 counter, s32 ticks) -> u16
{
    gba.reset();
    printf("ticks: %d\n", gba.scheduler.get_ticks());
    mem::write16(gba, mem::IO_TM3D, counter); // set counter
    printf("ticks: %d\n", gba.scheduler.get_ticks());
    mem::write16(gba, mem::IO_TM3CNT, 0x00); // disable timer
    printf("ticks: %d\n", gba.scheduler.get_ticks());
    mem::write16(gba, mem::IO_TM3CNT, 0x80); // enable timer
    gba.scheduler.tick(ticks);
    gba.scheduler.fire();
    printf("ticks: %d\n", gba.scheduler.get_ticks());
    const auto result = mem::read16(gba, mem::IO_TM3D); // +1 tick
    printf("ticks: %d\n", gba.scheduler.get_ticks());
    return result;
}

} // namespace

auto main() -> int
{
    auto gba = std::make_unique<Gba>();

    for (std::size_t i = 1; i < sizeof(TEST_DATA)/sizeof(TEST_DATA[0]); i++)
    {
        const auto result = test(*gba, TEST_DATA[i].counter, TEST_DATA[i].ticks);
        if (result != TEST_DATA[i].expected)
        {
            std::printf("failed timer test: %zu result: 0x%04X expected: 0x%04X\n", i, result, TEST_DATA[i].expected);
            return i;
        }
    }

    return 0; // passed!
}
