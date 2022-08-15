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

#include <frontend_base.hpp>
#include <icon.hpp>
#include <SDL.h>

namespace {

struct App final : frontend::Base
{
    App(int argc, char** argv);
    ~App() override;

public:
    auto loop() -> void override;

private:
    auto poll_events() -> void;
    auto run() -> void;
    auto render() -> void;

    auto on_key_event(const SDL_KeyboardEvent& e) -> void;
    auto on_display_event(const SDL_DisplayEvent& e) -> void;
    auto on_window_event(const SDL_WindowEvent& e) -> void;
    auto on_dropfile_event(SDL_DropEvent& e) -> void;
    auto on_controlleraxis_event(const SDL_ControllerAxisEvent& e) -> void;
    auto on_controllerbutton_event(const SDL_ControllerButtonEvent& e) -> void;
    auto on_controllerdevice_event(const SDL_ControllerDeviceEvent& e) -> void;

    auto is_fullscreen() const -> bool;
    auto toggle_fullscreen() -> void;
    auto get_window_size() const -> std::pair<int, int>;
    auto set_window_size(std::pair<int, int> new_size) const -> void;
    auto resize_emu_screen() -> void;
    auto open_url(const char* url) -> void;

public:
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};

    SDL_Rect emu_rect{};

    SDL_AudioDeviceID audio_device{};
    SDL_AudioStream* audio_stream{};
    SDL_AudioSpec aspec_wnt{};
    SDL_AudioSpec aspec_got{};
    std::mutex audio_mutex{};
    int sample_rate{65536};
    bool has_focus{true};

    std::unordered_map<Sint32, SDL_GameController*> controllers{};
};

auto sdl2_audio_callback(void* user, Uint8* data, int len) -> void
{
    auto app = static_cast<App*>(user);
    std::scoped_lock lock{app->audio_mutex};

    // this shouldn't be needed, however it causes less pops on startup
    if (SDL_AudioStreamAvailable(app->audio_stream) < len * 2)
    {
        std::memset(data, app->aspec_got.silence, len);
        return;
    }

    SDL_AudioStreamGet(app->audio_stream, data, len);
}

auto on_vblank_callback(void* user) -> void
{
    auto app = static_cast<App*>(user);
    void* texture_pixels{};
    int pitch{};

    SDL_LockTexture(app->texture, nullptr, &texture_pixels, &pitch);
        SDL_ConvertPixels(
            App::width, App::height,
            SDL_PIXELFORMAT_BGR555, app->gameboy_advance.ppu.pixels, App::width * sizeof(std::uint16_t), // src
            SDL_PIXELFORMAT_BGR555, texture_pixels, pitch // dst
        );
    SDL_UnlockTexture(app->texture);
}

auto on_audio_callback(void* user, std::int16_t left, std::int16_t right) -> void
{
    auto app = static_cast<App*>(user);
    std::scoped_lock lock{app->audio_mutex};
    const std::int16_t samples[2] = {left, right};
    SDL_AudioStreamPut(app->audio_stream, samples, sizeof(samples));
}

App::App(int argc, char** argv) : frontend::Base(argc, argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    SDL_DisplayMode display = {};
    if (!SDL_GetCurrentDisplayMode(0, &display))
    {
        // if the current scale would scale the screen bigger than
        // the display region, then change the scale variable
        if (width * scale > display.w || height * scale > display.h)
        {
            update_scale(display.w, display.h);
        }
    }

    window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    SDL_SetWindowMinimumSize(window, width, height);

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

    // setup emu rect
    resize_emu_screen();

    SDL_RenderSetVSync(renderer, 1);

    aspec_wnt.freq = sample_rate;
    aspec_wnt.format = AUDIO_S16;
    aspec_wnt.channels = 2;
    aspec_wnt.silence = 0;
    aspec_wnt.samples = 2048;
    aspec_wnt.padding = 0;
    aspec_wnt.size = 0;
    aspec_wnt.userdata = this;
    aspec_wnt.callback = sdl2_audio_callback;

    // allow all apsec to be changed if needed.
    // will be coverted and resampled by audiostream.
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &aspec_wnt, &aspec_got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (audio_device == 0)
    {
        return;
    }

    audio_stream = SDL_NewAudioStream(
        aspec_wnt.format, aspec_wnt.channels, aspec_wnt.freq,
        aspec_got.format, aspec_got.channels, aspec_got.freq
    );

    if (!audio_stream)
    {
        return;
    }

    std::printf("[SDL-AUDIO] format\twant: 0x%X \tgot: 0x%X\n", aspec_wnt.format, aspec_got.format);
    std::printf("[SDL-AUDIO] freq\twant: %d \tgot: %d\n", aspec_wnt.freq, aspec_got.freq);
    std::printf("[SDL-AUDIO] channels\twant: %d \tgot: %d\n", aspec_wnt.channels, aspec_got.channels);
    std::printf("[SDL-AUDIO] samples\twant: %d \tgot: %d\n", aspec_wnt.samples, aspec_got.samples);
    std::printf("[SDL-AUDIO] size\twant: %u \tgot: %u\n", aspec_wnt.size, aspec_got.size);

    SDL_PauseAudioDevice(audio_device, 0);

    gameboy_advance.set_userdata(this);
    gameboy_advance.set_vblank_callback(on_vblank_callback);
    gameboy_advance.set_audio_callback(on_audio_callback);

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
};

App::~App()
{
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

auto App::loop() -> void
{
    while (running)
    {
        poll_events();
        run();
        render();
    }
}

auto App::poll_events() -> void
{
    SDL_Event e{};

    while (SDL_PollEvent(&e) != 0)
    {
        switch (e.type)
        {
            case SDL_QUIT:
                running = false;
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
        }
    }
}

auto App::run() -> void
{
    if (emu_run && has_focus)
    {
        gameboy_advance.run();
    }
}

auto App::render() -> void
{
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, &emu_rect);
    SDL_RenderPresent(renderer);
}

auto App::on_key_event(const SDL_KeyboardEvent& e) -> void
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
                case SDL_SCANCODE_F:
                    toggle_fullscreen();
                    break;

                case SDL_SCANCODE_P:
                    emu_run ^= 1;
                    break;

                case SDL_SCANCODE_R:
                    if (enabled_rewind)
                    {
                        emu_rewind ^= 1;
                    }
                    break;

                case SDL_SCANCODE_S:
                    savestate(rom_path);
                    break;

                case SDL_SCANCODE_L:
                    loadstate(rom_path);
                    break;

                case SDL_SCANCODE_EQUALS:
                case SDL_SCANCODE_KP_PLUS:
                    scale++;
                    set_window_size({width * scale, height * scale});
                    break;

                case SDL_SCANCODE_MINUS:
                case SDL_SCANCODE_KP_MINUS:
                    if (scale > 1)
                    {
                        scale--;
                        set_window_size({width * scale, height * scale});
                    }
                    break;


                default: break; // silence enum warning
            }
        }

        return;
    }

    switch (e.keysym.scancode)
    {
        case SDL_SCANCODE_X:      set_button(gba::A, down);      break;
        case SDL_SCANCODE_Z:      set_button(gba::B, down);      break;
        case SDL_SCANCODE_A:      set_button(gba::L, down);      break;
        case SDL_SCANCODE_S:      set_button(gba::R, down);      break;
        case SDL_SCANCODE_RETURN: set_button(gba::START, down);  break;
        case SDL_SCANCODE_SPACE:  set_button(gba::SELECT, down); break;
        case SDL_SCANCODE_UP:     set_button(gba::UP, down);     break;
        case SDL_SCANCODE_DOWN:   set_button(gba::DOWN, down);   break;
        case SDL_SCANCODE_LEFT:   set_button(gba::LEFT, down);   break;
        case SDL_SCANCODE_RIGHT:  set_button(gba::RIGHT, down);  break;

    #ifndef EMSCRIPTEN
        case SDL_SCANCODE_ESCAPE:
            running = false;
            break;
    #endif // EMSCRIPTEN

        default: break; // silence enum warning
    }
}

auto App::on_display_event(const SDL_DisplayEvent& e) -> void
{
    switch (e.event)
    {
        case SDL_DISPLAYEVENT_NONE:
        case SDL_DISPLAYEVENT_ORIENTATION:
        case SDL_DISPLAYEVENT_CONNECTED:
        case SDL_DISPLAYEVENT_DISCONNECTED:
            break;
    }
}

auto App::on_window_event(const SDL_WindowEvent& e) -> void
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
            resize_emu_screen();
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_ENTER:
        case SDL_WINDOWEVENT_LEAVE:
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            has_focus = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            has_focus = false;
            break;

        case SDL_WINDOWEVENT_CLOSE:
        case SDL_WINDOWEVENT_TAKE_FOCUS:
        case SDL_WINDOWEVENT_HIT_TEST:
            break;
    }
}

auto App::on_dropfile_event(SDL_DropEvent& e) -> void
{
    if (e.file != nullptr)
    {
        loadrom(e.file);
        SDL_free(e.file);
    }
}

auto App::on_controlleraxis_event(const SDL_ControllerAxisEvent& e) -> void
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
                set_button(gba::LEFT, true);
            }
            else if (e.value > RIGHT)
            {
                set_button(gba::RIGHT, true);
            }
            else
            {
                set_button(gba::LEFT, false);
                set_button(gba::RIGHT, false);
            }
            break;

        case SDL_CONTROLLER_AXIS_LEFTY:
        case SDL_CONTROLLER_AXIS_RIGHTY:
            if (e.value < UP)
            {
                set_button(gba::UP, true);
            }
            else if (e.value > DOWN)
            {
                set_button(gba::DOWN, true);
            }
            else
            {
            {
                set_button(gba::UP, false);
                set_button(gba::DOWN, false);
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

auto App::on_controllerbutton_event(const SDL_ControllerButtonEvent& e) -> void
{
    const auto down = e.type == SDL_CONTROLLERBUTTONDOWN;

    switch (e.button)
    {
        case SDL_CONTROLLER_BUTTON_A: set_button(gba::A, down); break;
        case SDL_CONTROLLER_BUTTON_B: set_button(gba::B, down); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: set_button(gba::L, down); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: set_button(gba::R, down); break;
        case SDL_CONTROLLER_BUTTON_START: set_button(gba::START, down); break;
        case SDL_CONTROLLER_BUTTON_GUIDE: set_button(gba::SELECT, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: set_button(gba::UP, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: set_button(gba::DOWN, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: set_button(gba::LEFT, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: set_button(gba::RIGHT, down); break;

        default: break; // silence enum warning
    }
}

auto App::on_controllerdevice_event(const SDL_ControllerDeviceEvent& e) -> void
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

auto App::is_fullscreen() const -> bool
{
    const auto flags = SDL_GetWindowFlags(window);
    return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

auto App::toggle_fullscreen() -> void
{
    if (is_fullscreen())
    {
        SDL_SetWindowFullscreen(window, 0);
    }
    else
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }
}

auto App::get_window_size() const -> std::pair<int, int>
{
    int w;
    int h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    return {w, h};
}

auto App::set_window_size(std::pair<int, int> new_size) const -> void
{
    const auto [w, h] = new_size;

    SDL_SetWindowSize(window, w, h);
}

auto App::resize_emu_screen() -> void
{
    const auto [w, h] = get_window_size();
    update_scale(w, h);

    if (maintain_aspect_ratio)
    {
        const auto [scx, scy, scw, sch] = scale_with_aspect_ratio(w, h);

        emu_rect.x = scx;
        emu_rect.y = scy;
        emu_rect.w = scw;
        emu_rect.h = sch;
    }
    else
    {
        emu_rect.x = 0;
        emu_rect.y = 0;
        emu_rect.w = w;
        emu_rect.h = h;
    }
}

auto App::open_url(const char* url) -> void
{
    SDL_OpenURL(url);
}

} // namespace

auto main(int argc, char** argv) -> int
{
    auto app = std::make_unique<App>(argc, argv);
    app->loop();
    return 0;
}
