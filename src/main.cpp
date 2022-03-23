// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include <cstddef>
#include <cstdio>
#include <gba.hpp>
#include <cstdint>
#include <fstream>
#include <vector>
#include <optional>
#include <SDL.h>
#include <chrono>

namespace {

#define SPEED_TEST 0

static struct gba::Gba gameboy_advance{};
static SDL_AudioDeviceID audio_device{};
auto running = true;

auto OnKeyEvent(const SDL_KeyboardEvent* e) -> void {
    const auto down = e->type == SDL_KEYDOWN;
    const auto ctrl = (e->keysym.mod & KMOD_CTRL) > 0;
    const auto shift = (e->keysym.mod & KMOD_SHIFT) > 0;

    if (ctrl)
    {
        if (shift)
        {
        }
        else
        {
            switch (e->keysym.scancode)
            {
                case SDL_SCANCODE_S:
                    gameboy_advance.savestate("rom.state");
                    break;

                case SDL_SCANCODE_L:
                    gameboy_advance.loadstate("rom.state");
                    break;

                default: break; // silence enum warning
            }
        }

        return;
    }

    switch (e->keysym.scancode)
    {
        case SDL_SCANCODE_X:        gameboy_advance.setkeys(gba::A, down);        break;
        case SDL_SCANCODE_Z:        gameboy_advance.setkeys(gba::B, down);        break;
        case SDL_SCANCODE_RETURN:   gameboy_advance.setkeys(gba::START, down);    break;
        case SDL_SCANCODE_SPACE:    gameboy_advance.setkeys(gba::SELECT, down);   break;
        case SDL_SCANCODE_UP:       gameboy_advance.setkeys(gba::UP, down);       break;
        case SDL_SCANCODE_DOWN:     gameboy_advance.setkeys(gba::DOWN, down);     break;
        case SDL_SCANCODE_LEFT:     gameboy_advance.setkeys(gba::LEFT, down);     break;
        case SDL_SCANCODE_RIGHT:    gameboy_advance.setkeys(gba::RIGHT, down);    break;

    #ifndef EMSCRIPTEN
        case SDL_SCANCODE_ESCAPE:
            running = false;
            break;
    #endif // EMSCRIPTEN

        default: break; // silence enum warning
    }
}

auto load(std::string_view path) -> std::optional<std::vector<std::uint8_t>> {
    std::ifstream fs{path.data(), std::ios_base::binary};

    if (fs.good()) {
        fs.seekg(0, std::ios_base::end);
        const auto size = fs.tellg();
        fs.seekg(0, std::ios_base::beg);

        std::vector<std::uint8_t> data;
        data.resize(size);

        fs.read(reinterpret_cast<char*>(data.data()), data.size());
        return data;
    }

    return std::nullopt;
}

} // namespace

auto push_sample(std::int8_t left, std::int8_t right) -> void
{
    static std::int8_t samples[1024*2]{};
    static std::size_t index = 0;

    samples[index++] = left;
    samples[index++] = right;

    if (index == sizeof(samples))
    {
        #if SPEED_TEST == 0
        while (SDL_GetQueuedAudioSize(audio_device) >= 4096 * 2)
        {
            SDL_Delay(4);
        }

        SDL_QueueAudio(audio_device, &samples, sizeof(samples));
        #endif
        index = 0;
    }
}

auto main(int argc, char** argv) -> int
{
    // enable to record audio
    #if 0
        SDL_setenv("SDL_AUDIODRIVER", "disk", 1);
    #endif

    std::printf("argc: %d argv[0]: %s argv[1]: %s\n", argc, argv[0], argv[1]);

    if (argc < 2)
    {
        return -1;
    }

    const auto rom = load(argv[1]);
    if (rom->empty())
    {
        return -1;
    }

    if (!gameboy_advance.loadrom(rom.value()))
    {
        return -1;
    }

    if (argc == 3)
    {
        std::printf("loading bios\n");
        const auto bios = load(argv[2]);
        if (bios->empty())
        {
            return -1;
        }

        gameboy_advance.loadbios(bios.value());
    }

    constexpr auto cycles_per_frame = 280896;
    constexpr auto width = 240;
    constexpr auto height = 160;
    constexpr auto scale = 2;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    auto window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, 0);
    // auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
    SDL_RenderSetVSync(renderer, 0);

    auto start_time = std::chrono::high_resolution_clock::now();
    // auto current_time = std::chrono::high_resolution_clock::now();
    auto fps = 0;

    const SDL_AudioSpec wanted =
    {
        .freq = 32768,
        .format = AUDIO_S8,
        .channels = 2,
        .silence = 0,
        .samples = 2048,
        .padding = 0,
        .size = 0,
        .callback = NULL,
        .userdata = NULL,
    };

    SDL_AudioSpec aspec_got = {0};

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &aspec_got, 0);
    if (audio_device == 0)
    {
        return -1;
    }

    std::printf("[SDL-AUDIO] freq: %d\n", aspec_got.freq);
    std::printf("[SDL-AUDIO] channels: %d\n", aspec_got.channels);
    std::printf("[SDL-AUDIO] samples: %d\n", aspec_got.samples);
    std::printf("[SDL-AUDIO] size: %u\n", aspec_got.size);

    SDL_PauseAudioDevice(audio_device, 0);

    auto start_frame_time = std::chrono::high_resolution_clock::now();

    while (running)
    {
        SDL_Event e{};
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
                case SDL_QUIT: running = false; break;

                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    OnKeyEvent(&e.key);
                    break;
            }
        }

        gameboy_advance.run(cycles_per_frame);

        void* pixels = NULL; int pitch = 0;

        auto current_time = std::chrono::high_resolution_clock::now();
        #if SPEED_TEST == 1
        if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_frame_time).count() >= 16) [[unlikely]]
        #else
        if (1)
        #endif
        {
            SDL_LockTexture(texture, NULL, &pixels, &pitch);
                SDL_ConvertPixels(
                    width, height, // w,h
                    SDL_PIXELFORMAT_BGR555, gameboy_advance.ppu.pixels, width * 2, // src
                    SDL_PIXELFORMAT_BGR555, pixels, pitch // dst
                );
            SDL_UnlockTexture(texture);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            start_frame_time = current_time;//std::chrono::high_resolution_clock::now();
        }

        #if SPEED_TEST == 1
        fps++;
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 1) [[unlikely]]
        {
            std::printf("fps: %d\n", fps);
            start_time = current_time;//std::chrono::high_resolution_clock::now();
            fps = 0;

        }
        #endif
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
