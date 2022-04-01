// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "system.hpp"
#include <memory>

auto main(int argc, char** argv) -> int
{
    auto system = std::make_unique<sys::System>();

    if (system->init(argc, argv))
    {
        system->run();
    }

    return 0;
}
