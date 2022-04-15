// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "system.hpp"
#include "debugger/io.hpp"
#include "trim_font.hpp"
#include "icon.hpp"

#include <algorithm>
#include <ranges>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <minizip/unzip.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>
#include <imgui_memory_editor.h>

namespace sys {

auto audio_callback(void* user, Uint8* data, int len) -> void
{
    auto sys = static_cast<System*>(user);

    std::scoped_lock lock{sys->audio_mutex};

    // this shouldn't be needed, however it causes less pops on startup
    if (SDL_AudioStreamAvailable(sys->audio_stream) < len * 2)
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
    std::scoped_lock lock{sys->audio_mutex};
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

// basic rom loading from zip, will flesh this out more soon
auto loadzip(std::string path) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> data;
    auto zf = unzOpen(path.c_str());

    if (zf)
    {
        unz_global_info global_info;
        if (UNZ_OK == unzGetGlobalInfo(zf, &global_info))
        {
            bool found = false;

            for (auto i = 0; !found && i < global_info.number_entry; i++)
            {
                if (UNZ_OK == unzOpenCurrentFile(zf))
                {
                    unz_file_info file_info;
                    char name[0x304];

                    if (UNZ_OK == unzGetCurrentFileInfo(zf, &file_info, name, sizeof(name), nullptr, 0, nullptr, 0))
                    {
                        if (std::string_view{ name }.ends_with(".gba"))
                        {
                            data.resize(file_info.uncompressed_size);
                            unzReadCurrentFile(zf, data.data(), data.size());
                            found = true;
                        }
                    }

                    unzCloseCurrentFile(zf);
                }
            }
        }

        unzClose(zf);
    }

    return data;
}

auto loadfile(std::string path) -> std::vector<std::uint8_t>
{
    if (path.ends_with(".zip"))
    {
        // load zip
        return loadzip(path);
    }
    else
    {
        std::ifstream fs{ path.c_str(), std::ios_base::binary };

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
    }

    return {};
}

auto replace_extension(std::filesystem::path path, std::string new_ext = "") -> std::string
{
    return path.replace_extension(new_ext).string();
}

auto create_save_path(const std::string& path) -> std::string
{
    return replace_extension(path, ".sav");
}

auto create_state_path(const std::string& path, int slot = 0) -> std::string
{
    return replace_extension(path, ".state" + std::to_string(slot));
}

// note that this function is slow, should only ever be used for
// debugging gfx, never used in release builds.
auto draw_grid(int size, int count, float thicc, int x, int y)
{
    if (count == 0) return;

    for (int i = 1, j = size / count; i < count; i++)
    {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x + (j * i), y), ImVec2(x + (j * i), y + size), IM_COL32(40, 40, 40, 255), thicc);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x, y + (j * i)), ImVec2(x + size, y + (j * i)), IM_COL32(40, 40, 40, 255), thicc);
    }
}

auto on_hblank_callback(void* user, uint16_t line) -> void
{
    if constexpr (!System::debug_mode)
    {
        return;
    }

    if (line >= 160)
    {
        return;
    }

    auto sys = static_cast<System*>(user);

    for (auto i = 0; i < 4; i++)
    {
        if (sys->layers[i].enabled)
        {
            sys->layers[i].priority = sys->gameboy_advance.render_mode(sys->layers[i].pixels[line], 0, i);
        }
    }
}

auto System::render_layers() -> void
{
    if constexpr (!debug_mode)
    {
        return;
    }

    for (auto layer = 0; layer < 4; layer++)
    {
        if (this->layers[layer].enabled == false)
        {
            continue;
        }

        void* pixels{};
        int pitch{};

        SDL_LockTexture(texture_bg_layer[layer], nullptr, &pixels, &pitch);
            SDL_ConvertPixels(
                width, height, // w,h
                SDL_PIXELFORMAT_BGR555, this->layers[layer].pixels, width * sizeof(uint16_t), // src
                SDL_PIXELFORMAT_BGR555, pixels, pitch // dst
            );
        SDL_UnlockTexture(texture_bg_layer[layer]);

        const auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowSize(ImVec2(240, 160));
        ImGui::SetNextWindowSizeConstraints(ImVec2(240, 160), ImVec2(240, 160));

        const auto s = "bg layer: " + std::to_string(layer) + " priority: " + std::to_string(this->layers[layer].priority);
        ImGui::Begin(s.c_str(), &this->layers[layer].enabled, flags);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

            ImGui::SetCursorPos({0, 0});

            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::Image(texture_bg_layer[layer], ImVec2(240, 160));
            ImGui::PopStyleVar(5);

            if (show_grid)
            {
                // 240/8 = 30
                draw_grid(240, 30, 1.0f, p.x, p.y);
            }
        }
        ImGui::End();
    }
}

System::~System()
{
    // save game on exit
    this->closerom();

    // Cleanup
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // destroy debug textures
    for (auto& text : texture_bg_layer)
    {
        SDL_DestroyTexture(text);
    }

    if (audio_device) SDL_CloseAudioDevice(audio_device);
    if (audio_stream) SDL_FreeAudioStream(audio_stream);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

auto System::closerom() -> void
{
    if (this->has_rom)
    {
        this->savegame(this->rom_path);
        this->has_rom = false;
    }

    this->emu_run = false;
}

auto System::loadrom(std::string path) -> bool
{
    // close any previous loaded rom
    this->closerom();

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

    this->emu_run = true;
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
    const auto state_path = create_state_path(path, this->state_slot);
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
        const auto state_path = create_state_path(path, this->state_slot);
        std::printf("savestate to: %s\n", state_path.c_str());
        return dumpfile(state_path, {reinterpret_cast<std::uint8_t*>(state.get()), sizeof(gba::State)});
    }
    return false;
}

auto System::emu_set_button(gba::Button button, bool down) -> void
{
    // only handle emu inputs if thats the current window
    if (inside_emu_window)
    {
        gameboy_advance.setkeys(button, down);
    }
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
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_I:
                    this->viewer_io ^= 1;
                    break;

                case SDL_SCANCODE_L:
                    this->toggle_master_layer_enable();
                    break;

                case SDL_SCANCODE_A:
                    this->gameboy_advance.bit_crushing ^= 1;
                    break;

                default: break; // silence enum warning
            }
        }
        else
        {
            switch (e.keysym.scancode)
            {
                case SDL_SCANCODE_P:
                    this->emu_run ^= 1;
                    break;

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
        case SDL_SCANCODE_X:      this->emu_set_button(gba::A, down);      break;
        case SDL_SCANCODE_Z:      this->emu_set_button(gba::B, down);      break;
        case SDL_SCANCODE_A:      this->emu_set_button(gba::L, down);      break;
        case SDL_SCANCODE_S:      this->emu_set_button(gba::R, down);      break;
        case SDL_SCANCODE_RETURN: this->emu_set_button(gba::START, down);  break;
        case SDL_SCANCODE_SPACE:  this->emu_set_button(gba::SELECT, down); break;
        case SDL_SCANCODE_UP:     this->emu_set_button(gba::UP, down);     break;
        case SDL_SCANCODE_DOWN:   this->emu_set_button(gba::DOWN, down);   break;
        case SDL_SCANCODE_LEFT:   this->emu_set_button(gba::LEFT, down);   break;
        case SDL_SCANCODE_RIGHT:  this->emu_set_button(gba::RIGHT, down);  break;

    #ifndef EMSCRIPTEN
        case SDL_SCANCODE_ESCAPE:
            running = false;
            break;
    #endif // EMSCRIPTEN

        default: break; // silence enum warning
    }
}

auto System::on_display_event(const SDL_DisplayEvent& e) -> void
{

}

auto System::on_window_event(const SDL_WindowEvent& e) -> void
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
            this->resize_emu_screen();
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

auto System::on_dropfile_event(SDL_DropEvent& e) -> void
{
    if (e.file != nullptr)
    {
        this->loadrom(e.file);
        SDL_free(e.file);
    }
}

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
    this->gameboy_advance.set_hblank_callback(on_hblank_callback);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER);
    this->window = SDL_CreateWindow("Notorious BEEG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width*scale, height*scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    this->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
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
    if (icon)
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

    // setup debugger
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    io.Fonts->AddFontFromMemoryCompressedTTF(trim_font_compressed_data, trim_font_compressed_size, 20);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    // create debug textures
    for (auto& text : texture_bg_layer)
    {
        text = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, width, height);
    }

    return true;
}

auto System::run_events() -> void
{
    SDL_Event e{};
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        switch (e.type)
        {
            case SDL_QUIT: running = false; break;

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
            case SDL_CONTROLLERAXISMOTION:
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEREMAPPED:
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

auto System::run_emu() -> void
{
    if (emu_run)
    {
        gameboy_advance.run();
    }
}

auto System::toggle_master_layer_enable() -> void
{
    this->layer_enable_master ^= 1;
    for (auto& layer : this->layers)
    {
        layer.enabled = layer_enable_master;
    }
}

auto System::menubar_tab_file() -> void
{
    if (ImGui::MenuItem("Open", "Ctrl+O"))
    {
        std::printf("todo: open file picker\n");
    }
    if (ImGui::BeginMenu("Open Recent"))
    {
        ImGui::MenuItem("example_game1.gba");
        ImGui::MenuItem("example_game2.gba");
        ImGui::MenuItem("example_game3.gba");
        if (ImGui::BeginMenu("More.."))
        {
            ImGui::MenuItem("MORE");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Save State", "Ctrl+S", false, has_rom))
    {
        this->savestate(this->rom_path);
    }
    if (ImGui::MenuItem("Load State", "Ctrl+L", false, has_rom))
    {
        this->loadstate(this->rom_path);
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Save State Slot", has_rom))
    {
        if (ImGui::MenuItem("Slot 0", nullptr, state_slot == 0)) { state_slot = 0; }
        if (ImGui::MenuItem("Slot 1", nullptr, state_slot == 1)) { state_slot = 1; }
        if (ImGui::MenuItem("Slot 2", nullptr, state_slot == 2)) { state_slot = 2; }
        if (ImGui::MenuItem("Slot 3", nullptr, state_slot == 3)) { state_slot = 3; }
        if (ImGui::MenuItem("Slot 4", nullptr, state_slot == 4)) { state_slot = 4; }
        if (ImGui::MenuItem("Slot 5", nullptr, state_slot == 5)) { state_slot = 5; }
        if (ImGui::MenuItem("Slot 6", nullptr, state_slot == 6)) { state_slot = 6; }
        if (ImGui::MenuItem("Slot 7", nullptr, state_slot == 7)) { state_slot = 7; }
        if (ImGui::MenuItem("Slot 8", nullptr, state_slot == 8)) { state_slot = 8; }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Quit", "Alt+F4")) { running = false; }
}

auto System::menubar_tab_emulation() -> void
{
    assert(has_rom);

    ImGui::MenuItem("Play", "Ctrl+P", &emu_run);
    if (ImGui::MenuItem("Stop"))
    {
        this->closerom();
    }
    if (ImGui::MenuItem("Reset"))
    {
        gameboy_advance.reset();
    }
    ImGui::Separator();

    if (ImGui::MenuItem("Fast Forward")) {}
    if (ImGui::MenuItem("Rewind")) {}
}

auto System::menubar_tab_options() -> void
{
    if (ImGui::MenuItem("Configure...")) {}
    ImGui::Separator();

    if (ImGui::MenuItem("Graphics Settings")) {}
    if (ImGui::MenuItem("Audio Settings")) {}
    if (ImGui::MenuItem("Controller Settings")) {}
    if (ImGui::MenuItem("Hotkey Settings")) {}
}

auto System::menubar_tab_tools() -> void
{
    ImGui::MenuItem("todo...");
    ImGui::MenuItem("bit crushing", "Ctrl+A", &gameboy_advance.bit_crushing);
}

auto System::menubar_tab_view() -> void
{
    if (ImGui::MenuItem("Fullscreen", "Ctrl+F", is_fullscreen()))
    {
        this->toggle_fullscreen();
    }

    if (ImGui::BeginMenu("Scale"))
    {
        if (ImGui::MenuItem("x1", nullptr, emu_scale == 1)) { emu_scale = 1; }
        if (ImGui::MenuItem("x2", nullptr, emu_scale == 2)) { emu_scale = 2; }
        if (ImGui::MenuItem("x3", nullptr, emu_scale == 3)) { emu_scale = 3; }
        if (ImGui::MenuItem("x4", nullptr, emu_scale == 4)) { emu_scale = 4; }
        ImGui::EndMenu();
    }
    ImGui::Separator();

    ImGui::MenuItem("Show Grid", nullptr, &show_grid, debug_mode);
    ImGui::Separator();
    ImGui::MenuItem("Show Demo Window", nullptr, &show_demo_window, debug_mode);
    ImGui::MenuItem("Show Debug Window", nullptr, &show_debug_window, debug_mode);
    ImGui::MenuItem("Show IO viewer", "Ctrl+Shift+I", &viewer_io, debug_mode);
    ImGui::Separator();

    if (ImGui::MenuItem("Enable Layers", "Ctrl+Shift+L", &layer_enable_master, debug_mode))
    {
        this->toggle_master_layer_enable();
    }

    if (ImGui::BeginMenu("Show Layer", debug_mode))
    {
        ImGui::MenuItem("Layer 0", nullptr, &layers[0].enabled);
        ImGui::MenuItem("Layer 1", nullptr, &layers[1].enabled);
        ImGui::MenuItem("Layer 2", nullptr, &layers[2].enabled);
        ImGui::MenuItem("Layer 3", nullptr, &layers[3].enabled);
        ImGui::EndMenu();
    }

    ImGui::MenuItem("todo...");
}

auto System::menubar_tab_help() -> void
{
    if (ImGui::MenuItem("Info")) {}
    if (ImGui::MenuItem("Open On GitHub"))
    {
        SDL_OpenURL("https://github.com/ITotalJustice/notorious_beeg");
    }
    if (ImGui::MenuItem("Open An Issue"))
    {
        SDL_OpenURL("https://github.com/ITotalJustice/notorious_beeg/issues/new");
    }
}

auto System::menubar() -> void
{
    if (!show_menubar)
    {
        return;
    }

    if (ImGui::BeginMainMenuBar())
    {
        menubar_height = ImGui::GetWindowSize().y;

        if (ImGui::BeginMenu("File"))
        {
            this->menubar_tab_file();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation", has_rom))
        {
            this->menubar_tab_emulation();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            this->menubar_tab_options();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            this->menubar_tab_tools();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            this->menubar_tab_view();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            this->menubar_tab_help();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

template<int num>
auto mem_viewer_entry(const char* name, std::span<uint8_t> data) -> void
{
    if (ImGui::BeginTabItem(name))
    {
        static MemoryEditor editor;
        editor.DrawContents(data.data(), data.size());
        ImGui::EndTabItem();
    }
}

auto System::im_debug_window() -> void
{
    if (show_debug_window == false)
    {
        return;
    }

    ImGui::Begin("Debug Tab", &show_debug_window);
    {
        if (ImGui::Button("Run")) { emu_run = true; }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) { emu_run = false; }

        ImGui::Text("Opcode 0x%08X", gameboy_advance.cpu.pipeline[0]);
        ImGui::Separator();

        ImGui::Text("PC: 0x%08X", gameboy_advance.cpu.registers[gba::arm7tdmi::PC_INDEX]);
        ImGui::SameLine();
        ImGui::Text("LR: 0x%08X", gameboy_advance.cpu.registers[gba::arm7tdmi::LR_INDEX]);
        ImGui::SameLine();
        ImGui::Text("SP: 0x%08X", gameboy_advance.cpu.registers[gba::arm7tdmi::SP_INDEX]);
        ImGui::Separator();

        ImGui::Separator();
        ImGui::Text("Flags: C:%u N:%u V:%u Z:%u",
            gameboy_advance.cpu.cpsr.C, gameboy_advance.cpu.cpsr.N,
            gameboy_advance.cpu.cpsr.V, gameboy_advance.cpu.cpsr.Z);

        ImGui::Text("Control: I:%u F:%u T:%u M:%u",
            gameboy_advance.cpu.cpsr.I, gameboy_advance.cpu.cpsr.F,
            gameboy_advance.cpu.cpsr.T, gameboy_advance.cpu.cpsr.M);

        ImGui::BeginTabBar("Mem editor");
        {
            mem_viewer_entry<0>("256kb ewram", gameboy_advance.mem.ewram);
            mem_viewer_entry<1>("32kb iwram", gameboy_advance.mem.iwram);
            mem_viewer_entry<2>("1kb pram", gameboy_advance.mem.pram);
            mem_viewer_entry<3>("96kb vram", gameboy_advance.mem.vram);
            mem_viewer_entry<4>("1kb oam", gameboy_advance.mem.oam);
            mem_viewer_entry<5>("1kb io", gameboy_advance.mem.io);
            mem_viewer_entry<6>("32mb rom", gameboy_advance.rom);
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

auto System::emu_update_texture() -> void
{
    if (!emu_run)
    {
        return;
    }

    void* pixels{};
    int pitch{};

    SDL_LockTexture(texture, nullptr, &pixels, &pitch);
        SDL_ConvertPixels(
            width, height, // w,h
            SDL_PIXELFORMAT_BGR555, gameboy_advance.ppu.pixels, width * sizeof(uint16_t), // src
            SDL_PIXELFORMAT_BGR555, pixels, pitch // dst
        );
    SDL_UnlockTexture(texture);
}

auto System::emu_render() -> void
{
    const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, emu_rect.y));
    ImGui::SetNextWindowSize(ImVec2(emu_rect.w, emu_rect.h));
    ImGui::SetNextWindowSizeConstraints({0, 0}, ImVec2(emu_rect.w, emu_rect.h));

    ImGui::Begin("emu window", nullptr, flags);
    {
        inside_emu_window = ImGui::IsWindowFocused();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

        ImGui::SetCursorPos(ImVec2(0, 0));

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::Image(texture, ImVec2(emu_rect.w, emu_rect.h));
        ImGui::PopStyleVar(5);

        if (show_grid)
        {
            // 240/8 = 30
            draw_grid(emu_rect.w, 30, 1.0f, p.x, p.y);
        }
    }
    ImGui::End();
}

auto System::run_render() -> void
{
    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if constexpr(System::debug_mode)
    {
        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        if (viewer_io)
        {
            debugger::io::render(gameboy_advance, &viewer_io);
        }
    }

    this->emu_update_texture();
    this->emu_render();

    this->menubar(); // this should be child to emu screen
    this->im_debug_window();
    this->render_layers();

    resize_to_menubar();

    // Rendering (REMEMBER TO RENDER IMGUI STUFF [BEFORE] THIS LINE)
    ImGui::Render();
    SDL_RenderClear(renderer);

    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
}

auto System::run() -> void
{
    while (running)
    {
        this->run_events();
        this->run_emu();

        #if SPEED_TEST == 1
        const auto current_time = std::chrono::high_resolution_clock::now();
        fps++;
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >= 1) [[unlikely]]
        {
            std::string title = "Notorious BEEG - fps: " + std::to_string(fps);
            SDL_SetWindowTitle(window, title.c_str());
            start_time = current_time;//std::chrono::high_resolution_clock::now();
            fps = 0;
        }
        #endif

        this->run_render();
    }
}

auto System::is_fullscreen() -> bool
{
    const auto flags = SDL_GetWindowFlags(window);
    return flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
}

auto System::toggle_fullscreen() -> void
{
    if (this->is_fullscreen())
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    }
    else
    {
        SDL_SetWindowFullscreen(window, 0);
    }
}

auto System::resize_to_menubar() -> void
{
    if (!should_resize)
    {
        return;
    }

    should_resize = false;

    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_SetWindowSize(window, w, h + menubar_height);

    this->resize_emu_screen();
}

auto System::resize_emu_screen() -> void
{
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    // update rect
    emu_rect.x = 0;
    emu_rect.y = menubar_height;
    emu_rect.w = w;
    emu_rect.h = h-menubar_height;
}

} // namespace sys
