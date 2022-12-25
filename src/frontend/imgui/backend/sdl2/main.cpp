// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdint>
#include <icon.hpp>
#include <imgui_base.hpp>

#include <cstdio>
#include <cstring>
#include <span>
#include <mutex>
#include <unordered_map>

#include <SDL.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>
#include <trim_font.hpp>

#include "imgui.h"
#include "sdl2_renderer.hpp"
#include "sdl2_gl_renderer.hpp"

namespace {

struct RendererApi
{
    const char* name;
    bool (*init_pre_window)();
    bool (*init_post_window)(SDL_Window* window);
    void (*quit)();
    bool (*render_pre)(SDL_Window* window);
    bool (*render_post)(SDL_Window* window);
    bool (*create_texture)(int id, int w, int h);
    void* (*get_texture)(int id);
    bool (*update_texture)(int id, int x, int y, int w, int h, void* pixels);
    std::pair<int, int> (*get_render_size)(SDL_Window* window);
};

constexpr RendererApi RENDERER[] =
{
    {
        .name = "SDL2_Renderer",
        .init_pre_window = renderer::sdl2::init_pre_window,
        .init_post_window = renderer::sdl2::init_post_window,
        .quit = renderer::sdl2::quit,
        .render_pre = renderer::sdl2::render_pre,
        .render_post = renderer::sdl2::render_post,
        .create_texture = renderer::sdl2::create_texture,
        .get_texture = renderer::sdl2::get_texture,
        .update_texture = renderer::sdl2::update_texture,
        .get_render_size = renderer::sdl2::get_render_size,
    },
    {
        .name = "OpenGL1.2",
        .init_pre_window = renderer::sdl2_gl1::init_pre_window,
        .init_post_window = renderer::sdl2_gl1::init_post_window,
        .quit = renderer::sdl2_gl1::quit,
        .render_pre = renderer::sdl2_gl1::render_pre,
        .render_post = renderer::sdl2_gl1::render_post,
        .create_texture = renderer::sdl2_gl1::create_texture,
        .get_texture = renderer::sdl2_gl1::get_texture,
        .update_texture = renderer::sdl2_gl1::update_texture,
        .get_render_size = renderer::sdl2_gl1::get_render_size,
    },
};

SDL_Window* window{};
int renderer_index{0};

auto create_window(int scale, int width, int height) -> bool
{
    window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
    {
        std::printf("%s\n", SDL_GetError());
        return false;
    }
    return true;
}

void destroy_window()
{
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

struct App final : ImguiBase
{
    App(int argc, char** argv);
    ~App() override;

    void init_renderer();
    void quit_renderer();
    auto loop() -> void override;

    auto poll_events() -> void override;
    auto render_begin() -> void override;
    auto render_end() -> void override;

    auto get_texture(TextureID id) -> void* override;
    auto update_texture(TextureID id, std::uint16_t pixels[160][240]) -> void override;

    // misc stuff
    auto get_window_size() -> std::pair<int, int> override;
    auto set_window_size(std::pair<int, int> new_size) -> void override;

    auto is_fullscreen() -> bool override;
    auto toggle_fullscreen() -> void override;

    auto open_url(const char* url) -> void override;

private:
    auto run() -> void;
    auto render() -> void;

    auto on_key_event(const SDL_KeyboardEvent& e) -> void;
    auto on_display_event(const SDL_DisplayEvent& e) -> void;
    auto on_window_event(const SDL_WindowEvent& e) -> void;
    auto on_dropfile_event(SDL_DropEvent& e) -> void;
    auto on_controlleraxis_event(const SDL_ControllerAxisEvent& e) -> void;
    auto on_controllerbutton_event(const SDL_ControllerButtonEvent& e) -> void;
    auto on_controllerdevice_event(const SDL_ControllerDeviceEvent& e) -> void;

public:
    SDL_AudioDeviceID audio_device{};
    SDL_AudioStream* audio_stream{};
    SDL_AudioSpec aspec_wnt{};
    SDL_AudioSpec aspec_got{};
    std::mutex audio_mutex{};
    std::vector<std::int16_t> sample_data;
    int sample_rate{65536};

    std::unordered_map<Sint32, SDL_GameController*> controllers;
};

auto audio_callback(void* user, Uint8* data, int len) -> void
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

auto push_sample_callback(void* user) -> void
{
    auto app = static_cast<App*>(user);
    std::scoped_lock lock{app->audio_mutex};
    SDL_AudioStreamPut(app->audio_stream, app->sample_data.data(), app->sample_data.size() * 2);
}

void App::init_renderer()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // docking is kinda broken on my distro. havent tested with examples though
    // so maybe its my code thats broken.
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    io.Fonts->AddFontFromMemoryCompressedTTF(trim_font_compressed_data, trim_font_compressed_size, 20);

    RENDERER[renderer_index].init_pre_window();
    create_window(scale, width, height);
    RENDERER[renderer_index].init_post_window(window);

    RENDERER[renderer_index].create_texture((int)TextureID::emu, 240, 160);
    RENDERER[renderer_index].create_texture((int)TextureID::layer0, 240, 160);
    RENDERER[renderer_index].create_texture((int)TextureID::layer1, 240, 160);
    RENDERER[renderer_index].create_texture((int)TextureID::layer2, 240, 160);
    RENDERER[renderer_index].create_texture((int)TextureID::layer3, 240, 160);

    auto icon = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint32_t*>(app_icon_data), 32, 32, 32, 4*32, SDL_PIXELFORMAT_RGBA32);
    if (icon != nullptr)
    {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }

    // setup emu rect
    resize_emu_screen();
}

void App::quit_renderer()
{
    RENDERER[renderer_index].quit();
    destroy_window();
    ImGui::DestroyContext();
}

App::App(int argc, char** argv) : ImguiBase{argc, argv}
{
    // https://github.com/mosra/magnum/issues/184#issuecomment-425952900
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER))
    {
        return;
    }

    init_renderer();

    aspec_wnt.freq = sample_rate;
    aspec_wnt.format = AUDIO_S16;
    aspec_wnt.channels = 2;
    aspec_wnt.silence = 0;
    aspec_wnt.samples = 2048;
    aspec_wnt.padding = 0;
    aspec_wnt.size = 0;
    aspec_wnt.userdata = this;
    aspec_wnt.callback = audio_callback;

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

    sample_data.resize((aspec_got.samples * aspec_got.channels) & ~0x1);

    std::printf("[SDL-AUDIO] format\twant: 0x%X \tgot: 0x%X\n", aspec_wnt.format, aspec_got.format);
    std::printf("[SDL-AUDIO] freq\twant: %d \tgot: %d\n", aspec_wnt.freq, aspec_got.freq);
    std::printf("[SDL-AUDIO] channels\twant: %d \tgot: %d\n", aspec_wnt.channels, aspec_got.channels);
    std::printf("[SDL-AUDIO] samples\twant: %d \tgot: %d\n", aspec_wnt.samples, aspec_got.samples);
    std::printf("[SDL-AUDIO] size\twant: %u \tgot: %u\n", aspec_wnt.size, aspec_got.size);

    SDL_PauseAudioDevice(audio_device, 0);

    gameboy_advance.set_userdata(this);
    // gameboy_advance.set_hblank_callback(on_hblank_callback);
    gameboy_advance.set_audio_callback(push_sample_callback, sample_data);

    // need a rom to run atm
    //if (has_rom)
    {
        running = true;
    }
};

App::~App()
{
    quit_renderer();
    if (audio_device != 0) { SDL_CloseAudioDevice(audio_device); }
    if (audio_stream != nullptr) { SDL_FreeAudioStream(audio_stream); }
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
        ImGui_ImplSDL2_ProcessEvent(&e);
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
    if (emu_run && has_rom)
    {
        gameboy_advance.run();
    }
}

auto App::render() -> void
{
    run_render();
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
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_I:
                    viewer_io ^= 1;
                    break;

                case SDL_SCANCODE_L:
                    toggle_master_layer_enable();
                    break;

                case SDL_SCANCODE_A:
                    gameboy_advance.bit_crushing ^= 1;
                    break;

                case SDL_SCANCODE_P:
                    show_log_window ^= 1;
                    break;

                case SDL_SCANCODE_K:
                    show_perf_window ^= 1;
                    break;

                case SDL_SCANCODE_D:
                    quit_renderer();
                    renderer_index ^= 1;
                    init_renderer();
                    break;

                default: break; // silence enum warning
            }
        }
        else
        {
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_O:
                    if (auto path = filepicker(); !path.empty())
                    {
                        loadrom(path);
                    }
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
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST:
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

auto App::render_begin() -> void
{
    RENDERER[renderer_index].render_pre(window);
}

auto App::render_end() -> void
{
    RENDERER[renderer_index].render_post(window);
}

auto App::get_texture(TextureID id) -> void*
{
    return RENDERER[renderer_index].get_texture((int)id);
}

auto App::update_texture(TextureID id, std::uint16_t _pixels[160][240]) -> void
{
    RENDERER[renderer_index].update_texture((int)id, 0, 0, 240, 160, _pixels);
}

auto App::get_window_size() -> std::pair<int, int>
{
    return RENDERER[renderer_index].get_render_size(window);
}

auto App::set_window_size(std::pair<int, int> new_size) -> void
{
    const auto [w, h] = new_size;
    SDL_SetWindowSize(window, w, h);
}

auto App::is_fullscreen() -> bool
{
    const auto flags = SDL_GetWindowFlags(window);
    return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

auto App::toggle_fullscreen() -> void
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
