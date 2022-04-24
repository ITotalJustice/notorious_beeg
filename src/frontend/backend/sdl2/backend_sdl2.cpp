// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "backend_sdl2.hpp"
#include "../../system.hpp"
#include "../../icon.hpp"
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <SDL.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>

namespace sys::backend::sdl2 {

namespace {
SDL_Window* window{};
SDL_Renderer* renderer{};
SDL_Texture* texture{};
SDL_Texture* texture_bg_layer[4]{};

SDL_AudioDeviceID audio_device{};
SDL_AudioStream* audio_stream{};
SDL_AudioSpec aspec_wnt{};
SDL_AudioSpec aspec_got{};
std::mutex audio_mutex{};
auto sample_rate{65536};

std::unordered_map<Sint32, SDL_GameController*> controllers;

constexpr auto width = 240;
constexpr auto height = 160;
constexpr auto scale = 4;

auto audio_callback(void* user, Uint8* data, int len) -> void
{
    std::scoped_lock lock{audio_mutex};

    // this shouldn't be needed, however it causes less pops on startup
    if (SDL_AudioStreamAvailable(audio_stream) < len * 2)
    {
        std::memset(data, aspec_got.silence, len);
        return;
    }

    SDL_AudioStreamGet(audio_stream, data, len);
}

auto push_sample_callback(void* user, std::int16_t left, std::int16_t right) -> void
{
#if SPEED_TEST == 0
    std::scoped_lock lock{audio_mutex};
    const std::int16_t samples[2] = {left, right};
    SDL_AudioStreamPut(audio_stream, samples, sizeof(samples));
#endif
}

auto on_key_event(const SDL_KeyboardEvent& e) -> void
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
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_I:
                    System::viewer_io ^= 1;
                    break;

                case SDL_SCANCODE_L:
                    System::toggle_master_layer_enable();
                    break;

                case SDL_SCANCODE_A:
                    System::gameboy_advance.bit_crushing ^= 1;
                    break;

                default: break; // silence enum warning
            }
        }
        else
        {
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_P:
                    System::emu_run ^= 1;
                    break;

                case SDL_SCANCODE_S:
                    System::savestate(System::rom_path);
                    break;

                case SDL_SCANCODE_L:
                    System::loadstate(System::rom_path);
                    break;

                default: break; // silence enum warning
            }
        }

        return;
    }

    switch (e.keysym.scancode)
    {
        case SDL_SCANCODE_X:      System::emu_set_button(gba::A, down);      break;
        case SDL_SCANCODE_Z:      System::emu_set_button(gba::B, down);      break;
        case SDL_SCANCODE_A:      System::emu_set_button(gba::L, down);      break;
        case SDL_SCANCODE_S:      System::emu_set_button(gba::R, down);      break;
        case SDL_SCANCODE_RETURN: System::emu_set_button(gba::START, down);  break;
        case SDL_SCANCODE_SPACE:  System::emu_set_button(gba::SELECT, down); break;
        case SDL_SCANCODE_UP:     System::emu_set_button(gba::UP, down);     break;
        case SDL_SCANCODE_DOWN:   System::emu_set_button(gba::DOWN, down);   break;
        case SDL_SCANCODE_LEFT:   System::emu_set_button(gba::LEFT, down);   break;
        case SDL_SCANCODE_RIGHT:  System::emu_set_button(gba::RIGHT, down);  break;

    #ifndef EMSCRIPTEN
        case SDL_SCANCODE_ESCAPE:
            System::running = false;
            break;
    #endif // EMSCRIPTEN

        default: break; // silence enum warning
    }
}

auto on_display_event(const SDL_DisplayEvent& e) -> void
{

}

auto on_window_event(const SDL_WindowEvent& e) -> void
{
    switch (e.event)
    {
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_HIDDEN:
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_MOVED:
        case SDL_WINDOWEVENT_RESIZED:
            break;

        case SDL_WINDOWEVENT_SIZE_CHANGED:
            System::resize_emu_screen();
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_ENTER:
        case SDL_WINDOWEVENT_LEAVE:
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST:
        case SDL_WINDOWEVENT_CLOSE:
        case SDL_WINDOWEVENT_TAKE_FOCUS:
        case SDL_WINDOWEVENT_HIT_TEST:
            break;
    }
}

auto on_dropfile_event(SDL_DropEvent& e) -> void
{
    if (e.file != nullptr)
    {
        System::loadrom(e.file);
        SDL_free(e.file);
    }
}

auto on_controlleraxis_event(const SDL_ControllerAxisEvent& e) -> void
{
    // sdl recommends deadzone of 8000
    constexpr auto DEADZONE = 8000;
    constexpr auto LEFT     = -DEADZONE;
    constexpr auto RIGHT    = +DEADZONE;
    constexpr auto UP       = -DEADZONE;
    constexpr auto DOWN     = +DEADZONE;

    switch (e.axis)
    {
        case SDL_CONTROLLER_AXIS_LEFTX:
        case SDL_CONTROLLER_AXIS_RIGHTX:
            if (e.value < LEFT)
            {
                System::emu_set_button(gba::LEFT, true);
            }
            else if (e.value > RIGHT)
            {
                System::emu_set_button(gba::RIGHT, true);
            }
            else
            {
                System::emu_set_button(gba::LEFT, false);
                System::emu_set_button(gba::RIGHT, false);
            }
            break;

        case SDL_CONTROLLER_AXIS_LEFTY:
        case SDL_CONTROLLER_AXIS_RIGHTY:
            if (e.value < UP)
            {
                System::emu_set_button(gba::UP, true);
            }
            else if (e.value > DOWN)
            {
                System::emu_set_button(gba::DOWN, true);
            }
            else
            {
            {
                System::emu_set_button(gba::UP, false);
                System::emu_set_button(gba::DOWN, false);
            }
            }
            break;

        // don't handle yet
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            return;

        default: return; // silence enum warning
    }
}

auto on_controllerbutton_event(const SDL_ControllerButtonEvent& e) -> void
{
    const auto down = e.type == SDL_CONTROLLERBUTTONDOWN;

    switch (e.button)
    {
        case SDL_CONTROLLER_BUTTON_A: System::emu_set_button(gba::A, down); break;
        case SDL_CONTROLLER_BUTTON_B: System::emu_set_button(gba::B, down); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: System::emu_set_button(gba::L, down); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: System::emu_set_button(gba::R, down); break;
        case SDL_CONTROLLER_BUTTON_START: System::emu_set_button(gba::START, down); break;
        case SDL_CONTROLLER_BUTTON_GUIDE: System::emu_set_button(gba::SELECT, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: System::emu_set_button(gba::UP, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: System::emu_set_button(gba::DOWN, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: System::emu_set_button(gba::LEFT, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: System::emu_set_button(gba::RIGHT, down); break;

        default: break; // silence enum warning
    }
}

auto on_controllerdevice_event(const SDL_ControllerDeviceEvent& e) -> void
{
    switch (e.type)
    {
        case SDL_CONTROLLERDEVICEADDED: {
            const auto itr = controllers.find(e.which);
            if (itr == controllers.end())
            {
                auto controller = SDL_GameControllerOpen(e.which);
                if (controller != nullptr)
                {
                    std::printf("[CONTROLLER] opened: %s\n", SDL_GameControllerNameForIndex(e.which));
                }
                else
                {
                    std::printf("[CONTROLLER] failed to open: %s error: %s\n", SDL_GameControllerNameForIndex(e.which), SDL_GetError());
                }
            }
            else
            {
               std::printf("[CONTROLLER] already added, ignoring: %s\n", SDL_GameControllerNameForIndex(e.which));
            }
        } break;

        case SDL_CONTROLLERDEVICEREMOVED: {
            const auto itr = controllers.find(e.which);
            if (itr != controllers.end())
            {
                std::printf("[CONTROLLER] removed controller\n");

                // have to manually close to free struct
                SDL_GameControllerClose(itr->second);
                controllers.erase(itr);
            }
        } break;

        case SDL_CONTROLLERDEVICEREMAPPED:
            std::printf("mapping updated for: %s\n", SDL_GameControllerNameForIndex(e.which));
            break;
    }
}
} // namespace

auto init() -> bool
{
    #if DUMP_AUDIO
        SDL_setenv("SDL_AUDIODRIVER", "disk", 1);
    #endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER);
    window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
    #if SPEED_TEST == 0
    SDL_RenderSetVSync(renderer, 1);
    #endif

    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
    const auto rmask = 0xff000000;
    const auto gmask = 0x00ff0000;
    const auto bmask = 0x0000ff00;
    const auto amask = 0xff;
    #else // little endian, like x86
    const auto rmask = 0x000000ff;
    const auto gmask = 0x0000ff00;
    const auto bmask = 0x00ff0000;
    const auto amask = 0xff000000;
    #endif

    auto icon = SDL_CreateRGBSurfaceFrom(const_cast<uint32_t*>(app_icon_data), 32, 32, 32, 4*32, rmask, gmask, bmask, amask);
    if (icon != nullptr)
    {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }

    aspec_wnt.freq = sample_rate;
    aspec_wnt.format = AUDIO_S16;
    aspec_wnt.channels = 2;
    aspec_wnt.silence = 0;
    aspec_wnt.samples = 2048;
    aspec_wnt.padding = 0;
    aspec_wnt.size = 0;
    aspec_wnt.callback = audio_callback;

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

    // create debug textures
    for (auto& text : texture_bg_layer)
    {
        text = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
    }

    System::gameboy_advance.set_audio_callback(push_sample_callback);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    return true;
}

auto quit() -> void
{
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    for (auto& text : texture_bg_layer)
    {
        SDL_DestroyTexture(text);
    }

    for (auto& [_, controller] : controllers)
    {
        SDL_GameControllerClose(controller);
    }

    if (audio_device != 0) { SDL_CloseAudioDevice(audio_device); }
    if (audio_stream != nullptr) { SDL_FreeAudioStream(audio_stream); }
    if (texture != nullptr) { SDL_DestroyTexture(texture); }
    if (renderer != nullptr) { SDL_DestroyRenderer(renderer); }
    if (window != nullptr) { SDL_DestroyWindow(window); }

    SDL_Quit();
}

auto poll_events() -> void
{
    SDL_Event e{};

    while (SDL_PollEvent(&e) != 0)
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        switch (e.type)
        {
            case SDL_QUIT:
                System::running = false;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                on_key_event(e.key);
                break;

            case SDL_DISPLAYEVENT:
                on_display_event(e.display);
                break;

            case SDL_WINDOWEVENT:
                on_window_event(e.window);
                break;

            case SDL_CONTROLLERAXISMOTION:
                on_controlleraxis_event(e.caxis);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                on_controllerbutton_event(e.cbutton);
                break;

            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEREMAPPED:
                on_controllerdevice_event(e.cdevice);
                break;

            case SDL_DROPFILE:
                on_dropfile_event(e.drop);
                break;

            case SDL_DROPTEXT:
            case SDL_DROPBEGIN:
            case SDL_DROPCOMPLETE:

            case SDL_APP_TERMINATING:
            case SDL_APP_LOWMEMORY:
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
            case SDL_APP_WILLENTERFOREGROUND:
            case SDL_APP_DIDENTERFOREGROUND:
            case SDL_LOCALECHANGED:
            case SDL_SYSWMEVENT:
            case SDL_TEXTEDITING:
            case SDL_TEXTINPUT:
            case SDL_KEYMAPCHANGED:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
            case SDL_JOYAXISMOTION:
            case SDL_JOYBALLMOTION:
            case SDL_JOYHATMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_JOYDEVICEADDED:
            case SDL_JOYDEVICEREMOVED:
            case SDL_CONTROLLERTOUCHPADDOWN:
            case SDL_CONTROLLERTOUCHPADMOTION:
            case SDL_CONTROLLERTOUCHPADUP:
            case SDL_CONTROLLERSENSORUPDATE:
            case SDL_FINGERDOWN:
            case SDL_FINGERUP:
            case SDL_FINGERMOTION:
            case SDL_DOLLARGESTURE:
            case SDL_DOLLARRECORD:
            case SDL_MULTIGESTURE:
            case SDL_CLIPBOARDUPDATE:
            case SDL_AUDIODEVICEADDED:
            case SDL_AUDIODEVICEREMOVED:
            case SDL_SENSORUPDATE:
            case SDL_RENDER_TARGETS_RESET:
            case SDL_RENDER_DEVICE_RESET:
            case SDL_USEREVENT:
                break;

            default: break; // silence enum warning
        }
    }
}

auto get_texture(TextureID id) -> void*
{
    switch (id)
    {
        case TextureID::emu: return texture;
        case TextureID::layer0: return texture_bg_layer[0];
        case TextureID::layer1: return texture_bg_layer[1];
        case TextureID::layer2: return texture_bg_layer[2];
        case TextureID::layer3: return texture_bg_layer[3];
    }

    return nullptr;
}

auto update_texture(TextureID id, std::uint16_t pixels[160][240]) -> void
{
    auto _texture = static_cast<SDL_Texture*>(get_texture(id));
    void* texture_pixels{};
    int pitch{};

    SDL_LockTexture(_texture, nullptr, &texture_pixels, &pitch);
        SDL_ConvertPixels(
            width, height, // w,h
            SDL_PIXELFORMAT_BGR555, pixels, width * sizeof(std::uint16_t), // src
            SDL_PIXELFORMAT_BGR555, texture_pixels, pitch // dst
        );
    SDL_UnlockTexture(_texture);
}

auto render_begin() -> void
{
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
}

auto render_end() -> void
{
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
}

auto get_window_size() -> std::pair<int, int>
{
    int w;
    int h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    return {w, h};
}

auto set_window_size(std::pair<int, int> new_size) -> void
{
    const auto [w, h] = new_size;

    SDL_SetWindowSize(window, w, h);
}

auto is_fullscreen() -> bool
{
    const auto flags = SDL_GetWindowFlags(window);
    return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

auto toggle_fullscreen() -> void
{
    if (is_fullscreen())
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }
    else
    {
        SDL_SetWindowFullscreen(window, 0);
    }
}

auto open_url(const char* url) -> void
{
    SDL_OpenURL(url);
}

} // sys::backend::sdl2
