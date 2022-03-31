// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <gba.hpp>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>
#include <SDL.h>
#include <chrono>

namespace {

struct System
{
    ~System();

    auto init(int argc, char** argv) -> bool;
    auto run() -> void;
    auto on_key_event(const SDL_KeyboardEvent& e) -> void;

    auto loadrom(std::string path) -> bool;

    auto loadsave(std::string path) -> bool;
    auto savegame(std::string path) const -> bool;

    auto loadstate(std::string path) -> bool;
    auto savestate(std::string path) const -> bool;

    static inline gba::Gba gameboy_advance{};
    static inline SDL_Window* window{};
    static inline SDL_Renderer* renderer{};
    static inline SDL_Texture* texture{};
    static inline SDL_AudioDeviceID audio_device{};
    static inline SDL_AudioStream* audio_stream{};
    static inline SDL_AudioSpec aspec_wnt{};
    static inline SDL_AudioSpec aspec_got{};
    static constexpr inline auto width = 240;
    static constexpr inline auto height = 160;
    static constexpr inline auto scale = 2;
    std::string rom_path{};
    bool has_rom{false};
    bool running{true};
};

auto audio_callback(void* user, Uint8* data, int len) -> void
{
    auto sys = static_cast<System*>(user);

    // this shouldn't be needed, however it causes less pops on startup
    if (SDL_AudioStreamAvailable(sys->audio_stream) < len * 4)
    {
        std::memset(data, sys->aspec_got.silence, len);
        return;
    }

    SDL_AudioStreamGet(sys->audio_stream, data, len);
}

auto push_sample_callback(void* user, std::int16_t left, std::int16_t right) -> void
{
#if SPEED_TEST == 0
    auto sys = static_cast<System*>(user);
    const int16_t samples[2] = {left, right};
    SDL_AudioStreamPut(sys->audio_stream, samples, sizeof(samples));
#endif
}

auto dumpfile(std::string path, std::span<const std::uint8_t> data) -> bool
{
    std::ofstream fs{path.c_str(), std::ios_base::binary};

    if (fs.good())
    {
        fs.write(reinterpret_cast<const char*>(data.data()), data.size());

        if (fs.good())
        {
            return true;
        }
    }

    return false;
}

auto loadfile(std::string path) -> std::vector<std::uint8_t>
{
    std::ifstream fs{path.c_str(), std::ios_base::binary};

    if (fs.good()) {
        fs.seekg(0, std::ios_base::end);
        const auto size = fs.tellg();
        fs.seekg(0, std::ios_base::beg);

        std::vector<std::uint8_t> data;
        data.resize(size);

        fs.read(reinterpret_cast<char*>(data.data()), data.size());

        if (fs.good())
        {
            return data;
        }
    }

    return {};
}

auto create_save_path(const std::string& path) -> std::string
{
    return path + ".sav";
}

auto create_state_path(const std::string& path) -> std::string
{
    return path  + ".state";
}

System::~System()
{
    // save game on exit
    if (this->has_rom)
    {
        this->savegame(this->rom_path);
    }
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    if (audio_stream) SDL_FreeAudioStream(audio_stream);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

auto System::loadrom(std::string path) -> bool
{
    if (this->has_rom)
    {
        this->savegame(this->rom_path);
        this->has_rom = false;
    }

    this->rom_path = path;
    const auto rom_data = loadfile(this->rom_path);
    if (rom_data.empty())
    {
        return false;
    }

    if (!gameboy_advance.loadrom(rom_data))
    {
        return false;
    }

    this->has_rom = true;
    this->loadsave(this->rom_path);

    return true;
}

auto System::loadsave(std::string path) -> bool
{
    const auto save_path = create_save_path(path);
    const auto save_data = loadfile(save_path);
    if (!save_data.empty())
    {
        std::printf("loading save from: %s\n", save_path.c_str());
        return gameboy_advance.loadsave(save_data);
    }

    return false;
}

auto System::savegame(std::string path) const -> bool
{
    const auto save_path = create_save_path(path);
    const auto save_data = gameboy_advance.getsave();
    if (!save_data.empty())
    {
        std::printf("dumping save to: %s\n", save_path.c_str());
        return dumpfile(save_path, save_data);
    }

    return false;
}

auto System::loadstate(std::string path) -> bool
{
    const auto state_path = create_state_path(path);
    const auto state_data = loadfile(state_path);
    if (!state_data.empty() && state_data.size() == sizeof(gba::State))
    {
        std::printf("loadstate from: %s\n", state_path.c_str());
        auto state = std::make_unique<gba::State>();
        std::memcpy(state.get(), state_data.data(), state_data.size());
        return this->gameboy_advance.loadstate(*state);
    }
    return false;
}

auto System::savestate(std::string path) const -> bool
{
    auto state = std::make_unique<gba::State>();
    if (this->gameboy_advance.savestate(*state))
    {
        const auto state_path = create_state_path(path);
        std::printf("savestate to: %s\n", state_path.c_str());
        return dumpfile(state_path, {reinterpret_cast<std::uint8_t*>(state.get()), sizeof(gba::State)});
    }
    return false;
}

auto System::on_key_event(const SDL_KeyboardEvent& e) -> void
{
    const auto down = e.type == SDL_KEYDOWN;
    const auto ctrl = (e.keysym.mod & KMOD_CTRL) > 0;
    const auto shift = (e.keysym.mod & KMOD_SHIFT) > 0;

    if (ctrl)
    {
        if (down)
        {
            return;
        }

        if (shift)
        {
        }
        else
        {
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_S:
                    this->savestate(this->rom_path);
                    break;

                case SDL_SCANCODE_L:
                    this->loadstate(this->rom_path);
                    break;

                default: break; // silence enum warning
            }
        }

        return;
    }

    switch (e.keysym.scancode)
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

} // namespace

auto System::init(int argc, char** argv) -> bool
{
    // enable to record audio
    #if DUMP_AUDIO
        SDL_setenv("SDL_AUDIODRIVER", "disk", 1);
    #endif

    if (argc < 2)
    {
        return false;
    }

    if (!this->loadrom(argv[1]))
    {
        return false;
    }

    if (argc == 3)
    {
        std::printf("loading bios\n");
        const auto bios = loadfile(argv[2]);
        if (bios.empty())
        {
            return false;
        }

        if (!gameboy_advance.loadbios(bios))
        {
            return false;
        }
    }

    // set audio callback and user data
    this->gameboy_advance.set_userdata(this);
    this->gameboy_advance.set_audio_callback(push_sample_callback);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    this->window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, 0);
    this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    this->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
    #if SPEED_TEST == 0
    SDL_RenderSetVSync(renderer, 1);
    #endif

    aspec_wnt.freq = 65536;
    aspec_wnt.format = AUDIO_S16;
    aspec_wnt.channels = 2;
    aspec_wnt.silence = 0;
    aspec_wnt.samples = 2048;
    aspec_wnt.padding = 0;
    aspec_wnt.size = 0;
    aspec_wnt.callback = audio_callback;
    aspec_wnt.userdata = this;

    // allow all apsec to be changed if needed.
    // will be coverted and resampled by audiostream.
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &aspec_wnt, &aspec_got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (audio_device == 0)
    {
        return false;
    }

    audio_stream = SDL_NewAudioStream(
        aspec_wnt.format, aspec_wnt.channels, aspec_wnt.freq,
        aspec_got.format, aspec_got.channels, aspec_got.freq
    );

    std::printf("[SDL-AUDIO] format\twant: 0x%X \tgot: 0x%X\n", aspec_wnt.format, aspec_got.format);
    std::printf("[SDL-AUDIO] freq\twant: %d \tgot: %d\n", aspec_wnt.freq, aspec_got.freq);
    std::printf("[SDL-AUDIO] channels\twant: %d \tgot: %d\n", aspec_wnt.channels, aspec_got.channels);
    std::printf("[SDL-AUDIO] samples\twant: %d \tgot: %d\n", aspec_wnt.samples, aspec_got.samples);
    std::printf("[SDL-AUDIO] size\twant: %u \tgot: %u\n", aspec_wnt.size, aspec_got.size);

    SDL_PauseAudioDevice(audio_device, 0);

    return true;
}

auto System::run() -> void
{
    #if SPEED_TEST == 1
    auto start_time = std::chrono::high_resolution_clock::now();
    auto start_frame_time = std::chrono::high_resolution_clock::now();
    auto fps = 0;
    #endif

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
                    on_key_event(e.key);
                    break;

                default: break; // silence enum warning
            }
        }

        gameboy_advance.run();

        void* pixels = nullptr; int pitch = 0;

        #if SPEED_TEST == 1
        const auto current_time = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_frame_time).count() >= 16) [[unlikely]]
        #else
        #endif
        {
            SDL_LockTexture(texture, nullptr, &pixels, &pitch);
                SDL_ConvertPixels(
                    width, height, // w,h
                    SDL_PIXELFORMAT_BGR555, gameboy_advance.ppu.pixels, width * 2, // src
                    SDL_PIXELFORMAT_BGR555, pixels, pitch // dst
                );
            SDL_UnlockTexture(texture);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            #if SPEED_TEST == 1
            start_frame_time = current_time;//std::chrono::high_resolution_clock::now();
            #endif
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
}

auto main(int argc, char** argv) -> int
{
    auto system = std::make_unique<System>();

    if (system->init(argc, argv))
    {
        system->run();
    }

    return 0;
}
