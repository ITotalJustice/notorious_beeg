// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
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
    // this is set in init_audio()
    // gameboy_advance.set_audio_callback(on_audio_callback, sample_data);

    // need a rom to run atm
    if (has_rom)
    {
        running = true;

        char buf[100];
        gba::Header header{gameboy_advance.rom};
        std::sprintf(buf, "%s - [%.*s]", "Notorious BEEG", 12, header.game_title);
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
        SDL_RenderCopy(renderer, texture, nullptr, &emu_rect);
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
