// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include <gba.hpp>
#include <frontend_base.hpp>
#include <chrono>
#include <memory>
#include <cstdio>

namespace {

struct App final : frontend::Base
{
    using Base::Base;

    auto loop() -> void override
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        auto fps = 0;

        // set the apu callback so that apu is benchmarked
        // comment this out to disable apu benching
        const auto dummy_apu_callback = [](void* user)        {};
        static gba::s16 dummy_apu_data[2048];
        gameboy_advance.set_audio_callback(dummy_apu_callback, dummy_apu_data, 65536);

        while (has_rom)
        {
            gameboy_advance.run();
            fps++;

            const auto current_time = std::chrono::high_resolution_clock::now();

            if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 1) [[unlikely]]
            {
                std::printf("fps: %d\n", fps);
                start_time = current_time;//std::chrono::high_resolution_clock::now();
                fps = 0;
            }
        }
    }
};

} // namespace

auto main(int argc, char** argv) -> int
{
    auto app = std::make_unique<App>(argc, argv);
    app->loop();
    return 0;
}
