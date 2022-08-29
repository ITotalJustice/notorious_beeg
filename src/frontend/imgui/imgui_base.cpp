// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "imgui_base.hpp"
#include "debugger_io.hpp"

#include <trim_font.hpp>
#include <imgui.h>
#include <imgui_memory_editor.h>

namespace {

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

auto on_hblank_callback(void* user, std::uint16_t line) -> void
{
    // this is UB because the actual ptr is whatever inherts the base
    auto app = static_cast<ImguiBase*>(user);

    if constexpr (!ImguiBase::debug_mode)
    {
        return;
    }

    if (line >= 160)
    {
        return;
    }

    for (auto i = 0; i < 4; i++)
    {
        if (app->layers[i].enabled)
        {
            app->layers[i].priority = app->gameboy_advance.render_mode(app->layers[i].pixels[line], 0, i);
        }
    }
}

template<int num, typename T>
auto mem_viewer_entry(const char* name, std::span<T> data) -> void
{
    if (ImGui::BeginTabItem(name))
    {
        static MemoryEditor editor;
        editor.DrawContents(data.data(), data.size_bytes());
        ImGui::EndTabItem();
    }
}

} // namespace

ImguiBase::ImguiBase(int argc, char** argv) : frontend::Base{argc, argv}
{
    // gameboy_advance.set_hblank_callback(on_hblank_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    io.Fonts->AddFontFromMemoryCompressedTTF(trim_font_compressed_data, trim_font_compressed_size, 20);

    gameboy_advance.set_hblank_callback(on_hblank_callback);
}

ImguiBase::~ImguiBase()
{
    ImGui::DestroyContext();
}

auto ImguiBase::set_button(gba::Button button, bool down) -> void
{
    // only handle emu inputs if thats the current window
    if (inside_emu_window)
    {
        Base::set_button(button, down);
    }
}

auto ImguiBase::run_render() -> void
{
    // Start the Dear ImGui frame
    render_begin();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if constexpr(ImguiBase::debug_mode)
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

    emu_update_texture();
    emu_render();

    menubar(); // this should be child to emu screen
    im_debug_window();
    render_layers();

    resize_to_menubar();

    // Rendering (REMEMBER TO RENDER IMGUI STUFF [BEFORE] THIS LINE)
    ImGui::Render();
    render_end();
}

auto ImguiBase::resize_to_menubar() -> void
{
    if (!should_resize)
    {
        return;
    }

    should_resize = false;

    const auto [w, h] = get_window_size();
    set_window_size({w, h + menubar_height});

    resize_emu_screen();
}

auto ImguiBase::resize_emu_screen() -> void
{
    const auto [w, h] = get_window_size();

    // update rect
    emu_rect.x = 0;
    emu_rect.y = menubar_height;
    emu_rect.w = w;
    emu_rect.h = h-menubar_height;
}

auto ImguiBase::emu_update_texture() -> void
{
    if (!emu_run)
    {
        return;
    }

    update_texture(TextureID::emu, gameboy_advance.ppu.pixels);
}

auto ImguiBase::emu_render() -> void
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
        ImGui::Image(get_texture(TextureID::emu), ImVec2(emu_rect.w, emu_rect.h));
        ImGui::PopStyleVar(5);

        if (show_grid)
        {
            // 240/8 = 30
            draw_grid(emu_rect.w, 30, 1.0F, p.x, p.y);
        }
    }
    ImGui::End();
}

auto ImguiBase::menubar_tab_file() -> void
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
        savestate(rom_path);
    }
    if (ImGui::MenuItem("Load State", "Ctrl+L", false, has_rom))
    {
        loadstate(rom_path);
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

auto ImguiBase::menubar_tab_emulation() -> void
{
    assert(has_rom);

    ImGui::MenuItem("Play", "Ctrl+P", &emu_run);
    if (ImGui::MenuItem("Stop"))
    {
        closerom();
    }
    if (ImGui::MenuItem("Reset"))
    {
        gameboy_advance.reset();
    }
    ImGui::Separator();

    if (ImGui::MenuItem("Rewind Enabled", nullptr, &enabled_rewind))
    {
        // if (enabled_rewind)
        // {
        //     constexpr auto rewind_time = 60*60*10; // 10mins
        //     rewind_init(&rw, compressor_zlib, compressor_zlib_size, rewind_time);
        // }
        // else
        // {
        //     rewind_close(&rw);
        //     emu_rewind = false;
        // }
    }
    if (ImGui::MenuItem("Rewind", "Ctrl+R", &emu_rewind, enabled_rewind)) {}
}

auto ImguiBase::menubar_tab_options() -> void
{
    if (ImGui::MenuItem("Configure...")) {}
    ImGui::Separator();

    if (ImGui::MenuItem("Graphics Settings")) {}
    if (ImGui::MenuItem("Audio Settings")) {}
    if (ImGui::MenuItem("Controller Settings")) {}
    if (ImGui::MenuItem("Hotkey Settings")) {}
}

auto ImguiBase::menubar_tab_tools() -> void
{
    ImGui::MenuItem("todo...");
    ImGui::MenuItem("bit crushing", "Ctrl+A", &gameboy_advance.bit_crushing);
}

auto ImguiBase::menubar_tab_view() -> void
{
    if (ImGui::MenuItem("Fullscreen", "Ctrl+F", is_fullscreen()))
    {
        toggle_fullscreen();
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
        toggle_master_layer_enable();
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

auto ImguiBase::menubar_tab_help() -> void
{
    if (ImGui::MenuItem("Info")) {}
    if (ImGui::MenuItem("Open On GitHub"))
    {
        open_url("https://github.com/ITotalJustice/notorious_beeg");
    }
    if (ImGui::MenuItem("Open An Issue"))
    {
        open_url("https://github.com/ITotalJustice/notorious_beeg/issues/new");
    }
}

auto ImguiBase::menubar() -> void
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
            menubar_tab_file();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation", has_rom))
        {
            menubar_tab_emulation();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options"))
        {
            menubar_tab_options();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            menubar_tab_tools();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            menubar_tab_view();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            menubar_tab_help();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

auto ImguiBase::im_debug_window() -> void
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
            mem_viewer_entry<0, std::uint8_t>("256kb ewram", gameboy_advance.mem.ewram);
            mem_viewer_entry<1, std::uint8_t>("32kb iwram", gameboy_advance.mem.iwram);
            mem_viewer_entry<2, std::uint8_t>("1kb pram", gameboy_advance.mem.pram);
            mem_viewer_entry<3, std::uint8_t>("96kb vram", gameboy_advance.mem.vram);
            mem_viewer_entry<4, std::uint8_t>("1kb oam", gameboy_advance.mem.oam);
            mem_viewer_entry<5, std::uint16_t>("1kb io", gameboy_advance.mem.io);
            mem_viewer_entry<6, std::uint8_t>("32mb rom", gameboy_advance.rom);
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

auto ImguiBase::render_layers() -> void
{
    if constexpr (!debug_mode)
    {
        return;
    }

    for (auto layer = 0; layer < 4; layer++)
    {
        if (!layers[layer].enabled)
        {
            continue;
        }

        update_texture(layers[layer].id, layers[layer].pixels);

        const auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowSize(ImVec2(240, 160));
        ImGui::SetNextWindowSizeConstraints(ImVec2(240, 160), ImVec2(240, 160));

        const auto s = "bg layer: " + std::to_string(layer) + " priority: " + std::to_string(layers[layer].priority);
        ImGui::Begin(s.c_str(), &layers[layer].enabled, flags);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0F);

            ImGui::SetCursorPos({0, 0});

            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::Image(get_texture(layers[layer].id), ImVec2(240, 160));
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

auto ImguiBase::toggle_master_layer_enable() -> void
{
    layer_enable_master ^= 1;
    for (auto& layer : layers)
    {
        layer.enabled = layer_enable_master;
    }
}
