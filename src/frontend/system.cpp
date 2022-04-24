// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "system.hpp"
#include "debugger/io.hpp"
#include "gba.hpp"
#include "trim_font.hpp"
#include "icon.hpp"

#include <algorithm>
#include <ranges>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <filesystem>
#include <minizip/unzip.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include "backend/sdl2/backend_sdl2.hpp"

namespace bend = sys::backend::sdl2;

namespace sys {

auto dumpfile(const std::string& path, std::span<const std::uint8_t> data) -> bool
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
auto loadzip(const std::string& path) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> data;
    auto zf = unzOpen(path.c_str());

    if (zf != nullptr)
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

auto loadfile(const std::string& path) -> std::vector<std::uint8_t>
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

auto replace_extension(std::filesystem::path path, const std::string& new_ext = "") -> std::string
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
    if (count == 0)
    {
        return;
    }

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

    for (auto i = 0; i < 4; i++)
    {
        if (System::layers[i].enabled)
        {
            System::layers[i].priority = System::gameboy_advance.render_mode(System::layers[i].pixels[line], 0, i);
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
        if (!System::layers[layer].enabled)
        {
            continue;
        }

        bend::update_texture(layers[layer].id, layers[layer].pixels);

        const auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowSize(ImVec2(240, 160));
        ImGui::SetNextWindowSizeConstraints(ImVec2(240, 160), ImVec2(240, 160));

        const auto s = "bg layer: " + std::to_string(layer) + " priority: " + std::to_string(System::layers[layer].priority);
        ImGui::Begin(s.c_str(), &System::layers[layer].enabled, flags);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0F);

            ImGui::SetCursorPos({0, 0});

            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::Image(bend::get_texture(layers[layer].id), ImVec2(240, 160));
            ImGui::PopStyleVar(5);

            if (show_grid)
            {
                // 240/8 = 30
                draw_grid(240, 30, 1.0F, p.x, p.y);
            }
        }
        ImGui::End();
    }
}

System::~System()
{
    // save game on exit
    System::closerom();

    bend::quit();

    // Cleanup
    ImGui::DestroyContext();
}

auto System::closerom() -> void
{
    if (System::has_rom)
    {
        System::savegame(System::rom_path);
        System::has_rom = false;
    }

    System::emu_run = false;
}

auto System::loadrom(const std::string& path) -> bool
{
    // close any previous loaded rom
    System::closerom();

    System::rom_path = path;
    const auto rom_data = loadfile(System::rom_path);
    if (rom_data.empty())
    {
        return false;
    }

    if (!gameboy_advance.loadrom(rom_data))
    {
        return false;
    }

    System::emu_run = true;
    System::has_rom = true;
    System::loadsave(System::rom_path);

    return true;
}

auto System::loadsave(const std::string& path) -> bool
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

auto System::savegame(const std::string& path) -> bool
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

auto System::loadstate(const std::string& path) -> bool
{
    const auto state_path = create_state_path(path, System::state_slot);
    const auto state_data = loadfile(state_path);
    if (!state_data.empty() && state_data.size() == sizeof(gba::State))
    {
        std::printf("loadstate from: %s\n", state_path.c_str());
        auto state = std::make_unique<gba::State>();
        std::memcpy(state.get(), state_data.data(), state_data.size());
        return System::gameboy_advance.loadstate(*state);
    }
    return false;
}

auto System::savestate(const std::string& path) -> bool
{
    auto state = std::make_unique<gba::State>();
    if (System::gameboy_advance.savestate(*state))
    {
        const auto state_path = create_state_path(path, System::state_slot);
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

auto System::init(int argc, char** argv) -> bool
{
    if (argc < 2)
    {
        return false;
    }

    if (!System::loadrom(argv[1]))
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
    // System::gameboy_advance.set_userdata(this);
    // System::gameboy_advance.set_audio_callback(push_sample_callback);
    System::gameboy_advance.set_hblank_callback(on_hblank_callback);

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

    return bend::init();
}

auto System::run_events() -> void
{
    bend::poll_events();
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
    System::layer_enable_master ^= 1;
    for (auto& layer : System::layers)
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
        System::savestate(System::rom_path);
    }
    if (ImGui::MenuItem("Load State", "Ctrl+L", false, has_rom))
    {
        System::loadstate(System::rom_path);
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
        System::closerom();
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
        System::toggle_fullscreen();
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
        System::toggle_master_layer_enable();
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
        bend::open_url("https://github.com/ITotalJustice/notorious_beeg");
    }
    if (ImGui::MenuItem("Open An Issue"))
    {
        bend::open_url("https://github.com/ITotalJustice/notorious_beeg/issues/new");
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
            System::menubar_tab_file();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation", has_rom))
        {
            System::menubar_tab_emulation();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            System::menubar_tab_options();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            System::menubar_tab_tools();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            System::menubar_tab_view();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            System::menubar_tab_help();
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
    if (!show_debug_window)
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

    bend::update_texture(TextureID::emu, gameboy_advance.ppu.pixels);
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0F);

        ImGui::SetCursorPos(ImVec2(0, 0));

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::Image(bend::get_texture(TextureID::emu), ImVec2(emu_rect.w, emu_rect.h));
        ImGui::PopStyleVar(5);

        if (show_grid)
        {
            // 240/8 = 30
            draw_grid(emu_rect.w, 30, 1.0F, p.x, p.y);
        }
    }
    ImGui::End();
}

auto System::run_render() -> void
{
    // Start the Dear ImGui frame
    bend::render_begin();
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

    System::emu_update_texture();
    System::emu_render();

    System::menubar(); // this should be child to emu screen
    System::im_debug_window();
    System::render_layers();

    resize_to_menubar();

    // Rendering (REMEMBER TO RENDER IMGUI STUFF [BEFORE] THIS LINE)
    ImGui::Render();
    bend::render_end();
}

auto System::run() -> void
{
    while (running)
    {
        System::run_events();
        System::run_emu();

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

        System::run_render();
    }
}

auto System::is_fullscreen() -> bool
{
    return bend::is_fullscreen();
}

auto System::toggle_fullscreen() -> void
{
    bend::toggle_fullscreen();
}

auto System::resize_to_menubar() -> void
{
    if (!should_resize)
    {
        return;
    }

    should_resize = false;

    const auto [w, h] = bend::get_window_size();
    bend::set_window_size({w, h + menubar_height});

    System::resize_emu_screen();
}

auto System::resize_emu_screen() -> void
{
    const auto [w, h] = bend::get_window_size();

    // update rect
    emu_rect.x = 0;
    emu_rect.y = menubar_height;
    emu_rect.w = w;
    emu_rect.h = h-menubar_height;
}

} // namespace sys
