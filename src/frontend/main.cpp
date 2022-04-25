// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "system.hpp"
#include <cstdio>
#include <memory>

auto main(int argc, char** argv) -> int
{
    #if !defined(__SWITCH__)
    if (argc < 2)
    {
        std::printf("- args: exe rom\n");
        std::printf("- optionally, pass bios as thrid param\n");
        std::printf("- args: exe rom bios\n");
        return 1;
    }
    #endif

    auto system = std::make_unique<sys::System>();

    if (system->init(argc, argv))
    {
        system->run();
    }

    return 0;
}
