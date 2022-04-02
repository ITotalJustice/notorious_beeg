// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "system.hpp"
#include <algorithm>
#include <ranges>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>
#include <imgui_memory_editor.h>

namespace sys {

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

auto replace_extension(std::filesystem::path path, std::string new_ext = "") -> std::string
{
    return path.replace_extension(new_ext);
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
    if (!System::debug_mode)
    {
        return;
    }

    if (line >= 160)
    {
        return;
    }

    auto sys = static_cast<System*>(user);

    // on line 0, memset the layers
    if (line == 0)
    {
        std::memset(sys->bg_pixel_layers, 0, sizeof(sys->bg_pixel_layers));
    }

    for (auto i = 0; i < 4; i++)
    {
        sys->gameboy_advance.render_mode(sys->bg_pixel_layers[i][line], 0, i);
    }
}

auto System::render_layers() -> void
{
    if (!debug_mode)
    {
        return;
    }

    for (auto layer = 0; layer < 4; layer++)
    {
        if (show_layer[layer] == false)
        {
            continue;
        }

        void* pixels{};
        int pitch{};

        SDL_LockTexture(texture_bg_layer[layer], nullptr, &pixels, &pitch);
            SDL_ConvertPixels(
                width, height, // w,h
                SDL_PIXELFORMAT_BGR555, bg_pixel_layers[layer], width * sizeof(uint16_t), // src
                SDL_PIXELFORMAT_BGR555, pixels, pitch // dst
            );
        SDL_UnlockTexture(texture_bg_layer[layer]);

        const auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowSize(ImVec2(240, 160));
        ImGui::SetNextWindowSizeConstraints(ImVec2(240, 160), ImVec2(240, 160));

        const auto s = "bg layer: " + std::to_string(layer);
        ImGui::Begin(s.c_str(), &show_layer[layer], flags);
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
                case SDL_SCANCODE_L:
                    this->toggle_master_layer_enable();
                    break;

                default: break; // silence enum warning
            }
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
        case SDL_SCANCODE_X:      this->emu_set_button(gba::A, down);      break;
        case SDL_SCANCODE_Z:      this->emu_set_button(gba::B, down);      break;
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

    // setup debugger
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

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
    std::ranges::fill(show_layer, layer_enable_master);
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

    ImGui::MenuItem("Play", nullptr, &emu_run);
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

auto System::menubar_file_options() -> void
{
    if (ImGui::MenuItem("Configure...")) {}
    ImGui::Separator();

    if (ImGui::MenuItem("Graphics Settings")) {}
    if (ImGui::MenuItem("Audio Settings")) {}
    if (ImGui::MenuItem("Controller Settings")) {}
    if (ImGui::MenuItem("Hotkey Settings")) {}
}

auto System::menubar_file_tools() -> void
{
    ImGui::MenuItem("todo...");
}

auto System::menubar_file_view() -> void
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
    ImGui::MenuItem("Show Debug Window", nullptr, &show_debug_window, debug_mode);
    ImGui::Separator();

    if (ImGui::MenuItem("Enable Layers", "Ctrl+Shift+L", &layer_enable_master, debug_mode))
    {
        this->toggle_master_layer_enable();
    }

    if (ImGui::BeginMenu("Show Layer", debug_mode))
    {
        ImGui::MenuItem("Layer 0", nullptr, &show_layer[0]);
        ImGui::MenuItem("Layer 1", nullptr, &show_layer[1]);
        ImGui::MenuItem("Layer 2", nullptr, &show_layer[2]);
        ImGui::MenuItem("Layer 3", nullptr, &show_layer[3]);
        ImGui::EndMenu();
    }

    ImGui::MenuItem("todo...");
}

auto System::menubar_file_help() -> void
{
    if (ImGui::MenuItem("Info")) {}
    if (ImGui::MenuItem("Open On GitHub"))
    {
        SDL_OpenURL("https://github.com/ITotalJustice/notorious_beeg");
    }
}

auto System::menubar() -> void
{
    if (!debug_mode)
    {
        return;
    }

    if (!show_menubar)
    {
        return;
    }

    if (ImGui::BeginMainMenuBar())
    {
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
            this->menubar_file_options();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            this->menubar_file_tools();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            this->menubar_file_view();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            this->menubar_file_help();
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
            mem_viewer_entry<2>("1kb pram", gameboy_advance.mem.palette_ram);
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
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ImVec2(240*emu_scale, 160*emu_scale));
    ImGui::SetNextWindowSizeConstraints({0, 0}, ImVec2(240*emu_scale, 160*emu_scale));

    ImGui::Begin("emu window", nullptr, flags);
    {
        inside_emu_window = ImGui::IsWindowFocused();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

        ImGui::SetCursorPos({0, 0});

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::Image(texture, ImVec2(240*emu_scale, 160*emu_scale));
        ImGui::PopStyleVar(5);

        if (show_grid)
        {
            // 240/8 = 30
            draw_grid(240*emu_scale, 30, 1.0f, p.x, p.y);
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
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    this->emu_update_texture();
    this->emu_render();

    this->menubar(); // this should be child to emu screen
    this->im_debug_window();
    this->render_layers();

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

} // namespace sys