// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "sdl2_base.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include "stb_image.h"
#include <gameboy/gb.hpp>
#include <cstdio>
#include <cstring>

namespace frontend::sdl2 {

Sdl2Base::Sdl2Base(int argc, char** argv) : frontend::Base{argc, argv}
{
    // https://github.com/mosra/magnum/issues/184#issuecomment-425952900
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    if (SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK))
    {
        std::printf("[SDL_INIT_JOYSTICK] %s\n", SDL_GetError());
    }

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))
    {
        std::printf("[SDL_INIT_GAMECONTROLLER] %s\n", SDL_GetError());
    }

    if (SDL_InitSubSystem(SDL_INIT_TIMER))
    {
        std::printf("[SDL_INIT_TIMER] %s\n", SDL_GetError());
    }

    window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    pixel_format_enum = SDL_GetWindowPixelFormat(window);
    pixel_format = SDL_AllocFormat(pixel_format_enum);
    if (!pixel_format)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    texture = SDL_CreateTexture(renderer, pixel_format_enum, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return;
    }

    // setup the pixels
    frontbuffer.resize(pixel_format->BytesPerPixel * width * height);
    backbuffer.resize(pixel_format->BytesPerPixel * width * height);
    gameboy_advance.set_pixels(frontbuffer.data(), width, pixel_format->BytesPerPixel);

    SDL_SetWindowMinimumSize(window, width, height);

    // setup emu rect
    resize_emu_screen();

    SDL_RenderSetVSync(renderer, 1);

    running = true;
}

auto Sdl2Base::init_audio(void* user, SDL_AudioCallback sdl2_cb, gba::AudioCallback gba_cb, int sample_rate) -> bool
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        std::printf("[SDL_INIT_AUDIO] %s\n", SDL_GetError());
        return false;
    }

    aspec_wnt.freq = sample_rate;
    aspec_wnt.format = AUDIO_S16;
    aspec_wnt.channels = 2;
    aspec_wnt.silence = 0;
    aspec_wnt.samples = 2048;
    aspec_wnt.padding = 0;
    aspec_wnt.size = 0;
    aspec_wnt.userdata = user;
    aspec_wnt.callback = sdl2_cb;

    // allow all apsec to be changed if needed.
    // will be coverted and resampled by audiostream.
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &aspec_wnt, &aspec_got, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (audio_device == 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return false;
    }

    if (aspec_got.size <= 1)
    {
        return false;
    }

    // has to be power of 2
    sample_data.resize((aspec_got.samples * aspec_got.channels) & ~0x1);

    audio_stream = SDL_NewAudioStream(
        aspec_wnt.format, aspec_wnt.channels, aspec_wnt.freq,
        aspec_got.format, aspec_got.channels, aspec_got.freq
    );

    if (!audio_stream)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", SDL_GetError(), nullptr);
        return false;
    }

    std::printf("[SDL-AUDIO] format\twant: 0x%X \tgot: 0x%X\n", aspec_wnt.format, aspec_got.format);
    std::printf("[SDL-AUDIO] freq\twant: %d \tgot: %d\n", aspec_wnt.freq, aspec_got.freq);
    std::printf("[SDL-AUDIO] channels\twant: %d \tgot: %d\n", aspec_wnt.channels, aspec_got.channels);
    std::printf("[SDL-AUDIO] samples\twant: %d \tgot: %d\n", aspec_wnt.samples, aspec_got.samples);
    std::printf("[SDL-AUDIO] size\twant: %u \tgot: %u\n", aspec_wnt.size, aspec_got.size);

    gameboy_advance.set_audio_callback(gba_cb, sample_data, aspec_wnt.freq);

    return true;
}

Sdl2Base::~Sdl2Base()
{
    if (pixel_format)
    {
        SDL_FreeFormat(pixel_format);
    }

    for (auto& tex : gif_textures)
    {
        SDL_DestroyTexture(tex);
    }

    if (gif_delays)
    {
        STBI_FREE(gif_delays);
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

    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) { SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER); }
    if (SDL_WasInit(SDL_INIT_JOYSTICK)) { SDL_QuitSubSystem(SDL_INIT_JOYSTICK); }
    if (SDL_WasInit(SDL_INIT_TIMER)) { SDL_QuitSubSystem(SDL_INIT_TIMER); }
    if (SDL_WasInit(SDL_INIT_AUDIO)) { SDL_QuitSubSystem(SDL_INIT_AUDIO); }

    SDL_Quit();
}

auto Sdl2Base::set_button(gba::Button button, bool down) -> void
{
    std::scoped_lock lock{core_mutex};
    frontend::Base::set_button(button, down);
}

auto Sdl2Base::loop() -> void
{
    while (running)
    {
        step();
    }
}

auto Sdl2Base::step() -> void
{
    static Uint64 start = 0;
    static Uint64 now = 0;

    constexpr auto div_60 = 1000.0 / 60.0;
    static auto delta = div_60;

    if (start == 0) [[unlikely]] // only happens on startup
    {
        start = SDL_GetPerformanceCounter();
    }

    poll_events();
    update_audio_device_pause_status(); // todo: remove this!
    run(delta / div_60);
    render();

    now = SDL_GetPerformanceCounter();
    const auto freq = static_cast<double>(SDL_GetPerformanceFrequency());
    delta = static_cast<double>((now - start) * 1000.0) / freq;
    start = now;
}

auto Sdl2Base::poll_events() -> void
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
                break;

            case SDL_APP_TERMINATING:
                std::printf("[SDL_APP_TERMINATING]\n");
                break;

            case SDL_APP_LOWMEMORY:
                std::printf("[SDL_APP_LOWMEMORY]\n");
                break;

            case SDL_APP_WILLENTERBACKGROUND:
                std::printf("[SDL_APP_WILLENTERBACKGROUND]\n");
                break;

            case SDL_APP_DIDENTERBACKGROUND:
                std::printf("[SDL_APP_DIDENTERBACKGROUND]\n");
                break;

            case SDL_APP_WILLENTERFOREGROUND:
                std::printf("[SDL_APP_WILLENTERFOREGROUND]\n");
                break;

            case SDL_APP_DIDENTERFOREGROUND:
                std::printf("[SDL_APP_DIDENTERFOREGROUND]\n");
                break;

            case SDL_LOCALECHANGED:
            case SDL_SYSWMEVENT:
            case SDL_TEXTEDITING:
            case SDL_TEXTINPUT:
            case SDL_KEYMAPCHANGED:
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                on_mousebutton_event(e.button);
                break;

            case SDL_MOUSEMOTION:
                on_mousemotion_event(e.motion);
                break;

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
                break;

            case SDL_FINGERDOWN:
            case SDL_FINGERUP:
            case SDL_FINGERMOTION:
                on_touch_event(e.tfinger);
                break;

            case SDL_DOLLARGESTURE:
            case SDL_DOLLARRECORD:
            case SDL_MULTIGESTURE:
            case SDL_CLIPBOARDUPDATE:
            case SDL_AUDIODEVICEADDED:
            case SDL_AUDIODEVICEREMOVED:
            case SDL_SENSORUPDATE:
            case SDL_RENDER_TARGETS_RESET:
            case SDL_RENDER_DEVICE_RESET:
                break;

            // handled below
            case SDL_USEREVENT:
                break;
        }

        if (e.type >= SDL_USEREVENT)
        {
            on_user_event(e.user);
        }
    }
}

auto Sdl2Base::run(double delta) -> void
{
    // todo: handle menu != Menu::ROM
    if (!emu_run || !has_focus || !has_rom)
    {
        return;
    }

    std::scoped_lock lock{core_mutex};

    if (gameboy_advance.is_gb())
    {
        if (gba::gb::has_rtc(gameboy_advance))
        {
            const auto the_time = time(nullptr);
            const auto tm = localtime(&the_time);

            if (tm)
            {
                gba::gb::Rtc rtc;
                rtc.S = tm->tm_sec;
                rtc.M = tm->tm_min;
                rtc.H = tm->tm_hour;
                rtc.DL = tm->tm_yday & 0xFF;
                rtc.DH = tm->tm_yday > 0xFF;
                gba::gb::set_rtc(gameboy_advance, rtc);
            }
        }
    }

    // just in case something sends the main thread to sleep
    // ie, filedialog, then cap the max delta to something reasonable!
    // maybe keep track of deltas here to get an average?
    delta = std::min(delta, 1.333333);
    auto cycles = gba::CYCLES_PER_FRAME * delta;
    if (emu_fast_forward)
    {
        cycles *= 2;
    }
    gameboy_advance.run(cycles);
}

auto Sdl2Base::on_key_event(const SDL_KeyboardEvent& e) -> void
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
                case SDL_SCANCODE_L:
                    rom_file_picker();
                    break;

                default: break; // silence enum warning
            }
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

auto Sdl2Base::on_display_event(const SDL_DisplayEvent& e) -> void
{
    switch (e.event)
    {
        case SDL_DISPLAYEVENT_NONE:
            std::printf("SDL_DISPLAYEVENT_NONE\n");
            break;

        case SDL_DISPLAYEVENT_ORIENTATION:
            std::printf("SDL_DISPLAYEVENT_ORIENTATION\n");
            break;

        case SDL_DISPLAYEVENT_CONNECTED:
            std::printf("SDL_DISPLAYEVENT_CONNECTED\n");
            break;

        case SDL_DISPLAYEVENT_DISCONNECTED:
            std::printf("SDL_DISPLAYEVENT_DISCONNECTED\n");
            break;
    }
}

auto Sdl2Base::on_window_event(const SDL_WindowEvent& e) -> void
{
    switch (e.event)
    {
        case SDL_WINDOWEVENT_SHOWN:
            // std::printf("SDL_WINDOWEVENT_SHOWN\n");
            has_focus = true;
            break;

        case SDL_WINDOWEVENT_HIDDEN:
            // std::printf("SDL_WINDOWEVENT_HIDDEN\n");
            has_focus = false;
            break;

        case SDL_WINDOWEVENT_EXPOSED:
            // std::printf("SDL_WINDOWEVENT_EXPOSED\n");
            // has_focus = true;
            break;

        case SDL_WINDOWEVENT_MOVED:
            // std::printf("SDL_WINDOWEVENT_MOVED\n");
            break;

        case SDL_WINDOWEVENT_RESIZED:
            // std::printf("SDL_WINDOWEVENT_RESIZED\n");
            break;

        case SDL_WINDOWEVENT_SIZE_CHANGED:
            // std::printf("SDL_WINDOWEVENT_SIZE_CHANGED\n");
            resize_emu_screen();
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
            // std::printf("SDL_WINDOWEVENT_MINIMIZED\n");
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
            // std::printf("SDL_WINDOWEVENT_MAXIMIZED\n");
            break;

        case SDL_WINDOWEVENT_RESTORED:
            // std::printf("SDL_WINDOWEVENT_RESTORED\n");
            break;

        case SDL_WINDOWEVENT_ENTER:
            // std::printf("SDL_WINDOWEVENT_ENTER\n");
            break;

        case SDL_WINDOWEVENT_LEAVE:
            // std::printf("SDL_WINDOWEVENT_LEAVE\n");
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            // std::printf("SDL_WINDOWEVENT_FOCUS_GAINED\n");
            has_focus = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            // std::printf("SDL_WINDOWEVENT_FOCUS_LOST\n");
            has_focus = false;
            break;

        case SDL_WINDOWEVENT_CLOSE:
            // std::printf("SDL_WINDOWEVENT_CLOSE\n");
            break;

        case SDL_WINDOWEVENT_TAKE_FOCUS:
            // std::printf("SDL_WINDOWEVENT_TAKE_FOCUS\n");
            break;

        case SDL_WINDOWEVENT_HIT_TEST:
            // std::printf("SDL_WINDOWEVENT_HIT_TEST\n");
            break;
    }
}

auto Sdl2Base::on_dropfile_event(SDL_DropEvent& e) -> void
{
    if (e.file != nullptr)
    {
        loadrom(e.file);
        SDL_free(e.file);
    }
}

auto Sdl2Base::on_controlleraxis_event(const SDL_ControllerAxisEvent& e) -> void
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

auto Sdl2Base::on_controllerbutton_event(const SDL_ControllerButtonEvent& e) -> void
{
    const auto down = e.type == SDL_CONTROLLERBUTTONDOWN;

    switch (e.button)
    {
        case SDL_CONTROLLER_BUTTON_A: set_button(gba::Button::A, down); break;
        case SDL_CONTROLLER_BUTTON_B: set_button(gba::Button::B, down); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: set_button(gba::Button::L, down); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: set_button(gba::Button::R, down); break;
        case SDL_CONTROLLER_BUTTON_START: set_button(gba::Button::START, down); break;
        case SDL_CONTROLLER_BUTTON_GUIDE: set_button(gba::Button::SELECT, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: set_button(gba::Button::UP, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: set_button(gba::Button::DOWN, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: set_button(gba::Button::LEFT, down); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: set_button(gba::Button::RIGHT, down); break;

        default: break; // silence enum warning
    }
}

auto Sdl2Base::on_controllerdevice_event(const SDL_ControllerDeviceEvent& e) -> void
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
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Controller", SDL_GetError(), nullptr);
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

auto Sdl2Base::get_window_to_render_scale(int mx, int my) const -> std::pair<int, int>
{
    const auto [win_w, win_h] = get_window_size();
    const auto [ren_w, ren_h] = get_renderer_size();

    // we need to un-normalise x, y
    const int x = static_cast<float>(mx) * (static_cast<float>(ren_w) / static_cast<float>(win_w));
    const int y = static_cast<float>(my) * (static_cast<float>(ren_h) / static_cast<float>(win_h));

    return {x, y};
}

auto Sdl2Base::get_render_to_window_scale(int mx, int my) const -> std::pair<int, int>
{
    const auto [win_w, win_h] = get_window_size();
    const auto [ren_w, ren_h] = get_renderer_size();

    // we need to un-normalise x, y
    const int x = static_cast<float>(mx) * (static_cast<float>(win_w) / static_cast<float>(ren_w));
    const int y = static_cast<float>(my) * (static_cast<float>(win_h) / static_cast<float>(ren_h));

    return {x, y};
}

auto Sdl2Base::is_fullscreen() const -> bool
{
    const auto flags = SDL_GetWindowFlags(window);
    return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

auto Sdl2Base::toggle_fullscreen() -> void
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

auto Sdl2Base::get_window_size() const -> std::pair<int, int>
{
    int w;
    int h;
    SDL_GetWindowSize(window, &w, &h);

    return {w, h};
}


auto Sdl2Base::get_renderer_size() const -> std::pair<int, int>
{
    int w;
    int h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    return {w, h};
}

auto Sdl2Base::set_window_size(std::pair<int, int> new_size) const -> void
{
    const auto [w, h] = new_size;

    SDL_SetWindowSize(window, w, h);
}

auto Sdl2Base::set_window_size_from_renderer() -> void
{
    SDL_DisplayMode display{};
    if (!SDL_GetCurrentDisplayMode(0, &display))
    {
        set_window_size({display.w, display.h});
    }
}

auto Sdl2Base::resize_emu_screen() -> void
{
    const auto [w, h] = get_renderer_size();
    update_scale(w, h);

    if (maintain_aspect_ratio)
    {
        const auto [scx, scy, scw, sch] = Base::scale_with_aspect_ratio(w, h);

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

auto Sdl2Base::open_url(const char* url) -> void
{
    SDL_OpenURL(url);
}

auto Sdl2Base::fill_audio_data_from_stream(Uint8* data, int len, bool tick_rom) -> void
{
    audio_mutex.lock();

    const auto available = SDL_AudioStreamAvailable(audio_stream);

    bool not_enough_samples = false;

    // if theres too little samples and ticking gba isn't an option
    #if 0
    if (available < len / 2 || (available < len && !tick_rom))
    {
        not_enough_samples = true;
    }
    #else
    if (available < len && !tick_rom)
    {
        not_enough_samples = true;
    }
    #endif

    // too many samples behind so no point catching up, or emu isn't running.
    #if 0
    if (not_enough_samples || !emu_run || !has_focus || !has_rom || !running || emu_audio_disabled)
    #else
    if (not_enough_samples)
    #endif
    {
        audio_mutex.unlock();
        std::memset(data, aspec_got.silence, len);
        return;
    }

    // this shouldn't be needed, however it causes less pops on startup
    if (available < len)
    {
        // need to unlock this because gba callback locks this mutex
        audio_mutex.unlock();
        // with this locked, nothing else writes to audiostream
        std::scoped_lock lock{core_mutex};

        while (SDL_AudioStreamAvailable(audio_stream) < len)
        {
            gameboy_advance.run(1000);
        }

        // need to block otherwise race condition with get()
        audio_mutex.lock();
    }

    SDL_AudioStreamGet(audio_stream, data, len);
    audio_mutex.unlock();
}

auto Sdl2Base::fill_stream_from_sample_data() -> void
{
    std::scoped_lock lock{audio_mutex};

    const int max_latency = (aspec_got.size / 2) * 3;

    // safety net for if something strange happens with the sdl audio stream
    // or the callback code where we have way too many samples, specifically about
    // 3 frames worth. at that point, start discarding samples for a bit.
    // not the best solution at all, but it'll do for now
    if (!audio_paused && max_latency > SDL_AudioStreamAvailable(audio_stream))
    {
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size() * 2);
    }
}

auto Sdl2Base::update_pixels_from_gba() -> void
{
    if (has_new_frame)
    {
        // std::printf("[WARNING] dropping frame, vblank called before previous frame was displayed!\n");
        return;
    }
    std::memcpy(backbuffer.data(), frontbuffer.data(), backbuffer.size());
    has_new_frame = true;
}

auto Sdl2Base::update_texture_from_pixels() -> void
{
    core_mutex.lock();
    if (has_new_frame)
    {
        has_new_frame = false;

        void* texture_pixels{};
        int pitch{};

        SDL_LockTexture(texture, nullptr, &texture_pixels, &pitch);
            SDL_ConvertPixels(
                width, height,
                pixel_format_enum, backbuffer.data(), width * pixel_format->BytesPerPixel, // src
                pixel_format_enum, texture_pixels, pitch // dst
            );
        SDL_UnlockTexture(texture);
    }
    core_mutex.unlock();
}

auto Sdl2Base::update_audio_device_pause_status() -> void
{
    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        return;
    }

    #if 0
    const auto status = SDL_GetAudioDeviceStatus(audio_device);
    int pause_on = 0;

    if (!emu_run || !has_focus || !has_rom || !running || emu_audio_disabled)
    {
        pause_on = 1;
    }

    if (pause_on && status == SDL_AUDIO_PLAYING)
    {
        SDL_PauseAudioDevice(audio_device, pause_on);
    }
    else if (!pause_on && status != SDL_AUDIO_PLAYING)
    {
        std::scoped_lock lock1{core_mutex};
        std::scoped_lock lock2{audio_mutex};
        SDL_AudioStreamClear(audio_stream);
        std::memset(sample_data.data(), aspec_got.silence, sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_PauseAudioDevice(audio_device, pause_on);
    }
    #else
    auto new_paused = false;

    if (!emu_run || !has_focus || !has_rom || !running || emu_audio_disabled)
    {
        new_paused = true;
    }

    if (new_paused && !audio_paused)
    {
        SDL_PauseAudioDevice(audio_device, 1);
    }
    else if (!new_paused && audio_paused)
    {
        std::scoped_lock lock1{core_mutex};
        std::scoped_lock lock2{audio_mutex};
        SDL_AudioStreamClear(audio_stream);
        std::memset(sample_data.data(), aspec_got.silence, sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_AudioStreamPut(audio_stream, sample_data.data(), sample_data.size());
        SDL_PauseAudioDevice(audio_device, 0);
    }
    audio_paused = new_paused;
    #endif
}

auto Sdl2Base::load_gif(const char* path) -> bool
{
    if (has_gif)
    {
        std::printf("only 1 beeg gif allowed!\n");
        return false;
    }

    const auto gif_data = loadfile(path);
    if (gif_data.empty())
    {
        std::printf("failed to load gif: %s\n", path);
        return false;
    }

    auto data = stbi_load_gif_from_memory(gif_data.data(), gif_data.size(), &gif_delays, &gif_w, &gif_h, &gif_z, &gif_comp, 0);
    if (!data)
    {
        std::printf("failed to load gif into stbi\n");
        return false;
    }

    gif_textures.resize(gif_z);

    for (auto i = 0; i < gif_z; i++)
    {
        if (auto surface = SDL_CreateRGBSurfaceWithFormatFrom(data + (gif_w * gif_h * gif_comp * i), gif_w, gif_h, 32, gif_w * gif_comp, SDL_PIXELFORMAT_RGBA32))
        {
            gif_textures[i] = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
        else
        {
            std::printf("failed to create surface: %s\n", SDL_GetError());
            stbi_image_free(data);
            return false;
        }
    }

    stbi_image_free(data);

    has_gif = true;
    return true;
}

auto Sdl2Base::gif_render(SDL_Rect* src_rect, SDL_Rect* dst_rect) -> void
{
    if (!has_gif)
    {
        return;
    }

    SDL_Rect rect;

    if (dst_rect == nullptr)
    {
        const auto [screen_w, screen_h] = get_renderer_size();
        const auto scale_w = static_cast<double>(screen_w) / static_cast<double>(gif_w);
        const auto scale_h = static_cast<double>(screen_h) / static_cast<double>(gif_h);
        const auto gif_scale = std::min(scale_w, scale_h);

        rect.w = gif_w * gif_scale;
        rect.h = gif_h * gif_scale;
        rect.x = (screen_w - rect.w) / 2;
        rect.y = (screen_h - rect.h) / 2;

        dst_rect = &rect;
    }

    SDL_RenderCopy(renderer, gif_textures[gif_index], src_rect, dst_rect);

    const auto ticks = SDL_GetTicks();

    if (gif_delta + gif_delays[gif_index] <= ticks)
    {
        gif_index = (gif_index + 1) % gif_z;
        gif_delta = ticks;
    }
}

} // namespace frontend::sdl2
