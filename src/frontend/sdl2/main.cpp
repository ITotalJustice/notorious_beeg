// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "gba.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

#include <sdl2_base.hpp>
#include <icon.hpp>
#include <SDL.h>

namespace {

struct App final : frontend::sdl2::Sdl2Base
{
    App(int argc, char** argv);

private:
    auto render() -> void override;
};

auto sdl2_audio_callback(void* user, Uint8* data, int len) -> void
{
    auto app = static_cast<App*>(user);
    app->fill_audio_data_from_stream(data, len, false);
}

auto on_vblank_callback(void* user) -> void
{
    auto app = static_cast<App*>(user);
    app->update_pixels_from_gba();
}

auto on_audio_callback(void* user) -> void
{
    auto app = static_cast<App*>(user);
    app->fill_stream_from_sample_data();
}

auto on_colour_callback(void* user, gba::Colour c) -> std::uint32_t
{
    auto app = static_cast<App*>(user);

    if (app->gameboy_advance.is_gb())
    {
        auto r = c.r();
        auto g = c.g();
        auto b = c.b();

        auto R = (r * 26 + g * 4 + b * 2);
        auto G = (g * 24 + b * 8);
        auto B = (r * 6 + g * 4 + b * 22);

        R = std::min(960, R) >> 2;
        G = std::min(960, G) >> 2;
        B = std::min(960, B) >> 2;

        R = (R - (R >> 2)) + 8;
        G = (G - (G >> 2)) + 8;
        B = (B - (B >> 2)) + 8;

        return SDL_MapRGB(app->pixel_format, R, G, B);
    }
    else
    {
        // SOURCE: https://gbdev.io/pandocs/Palettes.html
        auto r = (c.r8() - (c.r8() >> 2)) + 8;
        auto g = (c.g8() - (c.g8() >> 2)) + 8;
        auto b = (c.b8() - (c.b8() >> 2)) + 8;

        r = std::min(r, 255);
        g = std::min(g, 255);
        b = std::min(b, 255);

        return SDL_MapRGB(app->pixel_format, r, g, b);
    }
}

App::App(int argc, char** argv) : frontend::sdl2::Sdl2Base(argc, argv)
{
    if (!running)
    {
        printf("not running\n");
        return;
    }

    running = false;

    auto icon = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint32_t*>(app_icon_data), 32, 32, 32, 4*32, SDL_PIXELFORMAT_RGBA32);
    if (icon != nullptr)
    {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }

    if (!init_audio(this, sdl2_audio_callback, on_audio_callback))
    {
        return;
    }

    gameboy_advance.set_userdata(this);
    gameboy_advance.set_vblank_callback(on_vblank_callback);
    gameboy_advance.set_colour_callback(on_colour_callback);

    // need a rom to run atm
    if (has_rom)
    {
        if (gameboy_advance.is_gb())
        {
            // hack for loading palette for dmg games for now
            loadrom(argv[1]);
        }

        running = true;

        char buf[100];
        const auto title = gameboy_advance.get_rom_name();
        std::sprintf(buf, "%s - [%s]", "Notorious BEEG", title.str);
        SDL_SetWindowTitle(window, buf);
    }
    else
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to loadrom!", nullptr);
    }
}

auto App::render() -> void
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    update_texture_from_pixels();

    if (has_rom)
    {
        SDL_Rect src = { .x = 0, .y = 0, .w = 240, .h = 160 };

        if (gameboy_advance.is_gb() && gameboy_advance.stretch)
        {
            src.x = 40;
            src.w = 160;
        }

        SDL_RenderCopy(renderer, texture, &src, &emu_rect);
    }

    SDL_RenderPresent(renderer);
}

} // namespace

auto main(int argc, char** argv) -> int
{
    auto app = std::make_unique<App>(argc, argv);
    app->loop();
    return 0;
}
