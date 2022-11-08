// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "imgui_base.hpp"
#include "debugger_io.hpp"
#include "fat/fat.hpp"
#include "gba.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "sio.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <imgui.h>
#include <imgui_memory_editor.h>

namespace {

// todo: allow user to set custom path once filebrowser
// support is properly added either via native fs or
// imgui filebrowser.
constexpr auto FAT32_PATH = "sd.raw";

void HelpMarker(const char* desc, bool question_mark = false)
{
    if (question_mark)
    {
        ImGui::TextDisabled("(?)");
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
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

auto on_hblank_callback(void* user, std::uint16_t line) -> void
{
    if (!user) { return; }

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

void on_fat_flush_callback(void* user, std::uint64_t offset, std::uint64_t size)
{
    if (!user) { return; }

    // this is UB because the actual ptr is whatever inherts the base
    auto app = static_cast<ImguiBase*>(user);
    assert(offset + size <= app->fat_sd_card.size());

    std::fstream fs{FAT32_PATH, std::ios_base::in | std::ios_base::out | std::ios_base::ate | std::ios_base::binary};

    if (fs.good())
    {
        fs.seekg(offset);
        fs.write(reinterpret_cast<const char*>(app->fat_sd_card.data() + offset), size);
        assert(fs.good());
    }

    assert(fs.good());
}

auto on_frame_callback(void* user, std::uint32_t frame_cycles, std::uint32_t halt_cycles) -> void
{
    if (!user) { return; }

    // this is UB because the actual ptr is whatever inherts the base
    auto app = static_cast<ImguiBase*>(user);

    app->cycles_per_frame.push_back(frame_cycles);
    if (app->cycles_per_frame.size() >= app->max_cycles_per_frame_entries)
    {
        const auto diff = app->cycles_per_frame.size() - app->max_cycles_per_frame_entries;
        app->cycles_per_frame.erase(app->cycles_per_frame.begin(), app->cycles_per_frame.begin() + diff);
    }
}

void on_log_callback(void* user, std::uint8_t type, std::uint8_t level, const char* str)
{
    if (!user) { return; }

    // this is UB because the actual ptr is whatever inherts the base
    auto app = static_cast<ImguiBase*>(user);

    const auto type_str = gba::log::get_type_str();
    const auto level_str = gba::log::get_level_str();
    app->logger.AddLog("[%s] [%s] %s", level_str[level], type_str[type], str);
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
    scale = 4;

    gameboy_advance.set_hblank_callback(on_hblank_callback);
    gameboy_advance.set_fat_flush_callback(on_fat_flush_callback);
    gameboy_advance.set_frame_callback(on_frame_callback);
    gameboy_advance.set_log_callback(on_log_callback);
    gameboy_advance.set_pixels(pixels, 240, 16);

    // for quick testing
    // todo: add cli commands support
    #if 0
    load_fat_device(gba::fat::Type::EZFLASH);
    #endif

    // gameboy_advance.log_level |= gba::log::LevelFlag::FLAG_INFO;
    // gameboy_advance.log_type |= gba::log::TypeFlag::FLAG_SIO;
    // gameboy_advance.log_type |= gba::log::TypeFlag::FLAG_GAME;
    // gameboy_advance.log_type |= gba::log::TypeFlag::FLAG_TIMER0;
}

ImguiBase::~ImguiBase()
{
    // the file is flushed on sector write
    #if 0
    if (!fat_sd_card.empty())
    {
        dumpfile(FAT32_PATH, fat_sd_card);
    }
    #endif

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
            if (gameboy_advance.is_gb())
            {
                debugger::io::render_gb(gameboy_advance, &viewer_io);
            }
            else
            {
                debugger::io::render_gba(gameboy_advance, &viewer_io);
            }
        }
    }

    emu_update_texture();
    emu_render();

    menubar(); // this should be child to emu screen
    im_debug_window();
    render_layers();
    log_window();
    sio_window();
    perf_window();

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

    update_texture(TextureID::emu, pixels);
}

auto ImguiBase::emu_render() -> void
{
    const auto& io = ImGui::GetIO();
    const auto is_viewport = io.BackendFlags & ImGuiBackendFlags_RendererHasViewports && io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;

    auto flags = 0;

    if (is_viewport)
    {
        flags = 0;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    }
    else
    {
        flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowPos(ImVec2(0, emu_rect.y));
        ImGui::SetNextWindowSize(ImVec2(emu_rect.w, emu_rect.h));
        ImGui::SetNextWindowSizeConstraints({0, 0}, ImVec2(emu_rect.w, emu_rect.h));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.0F);
    }

    if (ImGui::Begin("emu window", nullptr, flags))
    {
        inside_emu_window = ImGui::IsWindowFocused();

        ImVec2 p = ImGui::GetCursorScreenPos();
        auto size = ImVec2(emu_rect.w, emu_rect.h);
        if (is_viewport)
        {
            size = ImGui::GetContentRegionAvail();
        }
        ImGui::Image(get_texture(TextureID::emu), size);

        if (show_grid)
        {
            // 240/8 = 30
            draw_grid(emu_rect.w, 30, 1.0F, p.x, p.y);
        }
    }
    ImGui::End();

    if (is_viewport)
    {
        ImGui::PopStyleVar(1);
    }
    else
    {
        ImGui::PopStyleVar(11);
    }
}

auto ImguiBase::menubar_tab_file() -> void
{
    if (ImGui::MenuItem("Open", "Ctrl+O"))
    {
        if (auto path = filepicker(); !path.empty())
        {
            loadrom(path);
        }
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
        for (int i = 0; i <= 8; i++)
        {
            char label[10];
            std::sprintf(label, "Slot %d", i);
            if (ImGui::MenuItem(label, nullptr, state_slot == i)) { state_slot = i; }
        }
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
    ImGui::Separator();

    if (ImGui::BeginMenu("FatDevice"))
    {
        auto types = gba::fat::get_type_str();

        for (std::size_t i = 0; i < types.size(); i++)
        {
            const auto type = static_cast<gba::fat::Type>(i);

            if (ImGui::MenuItem(types[i], nullptr, type == gameboy_advance.fat_device.type))
            {
                load_fat_device(type);
            }
        }
        ImGui::EndMenu();
    }
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
    ImGui::Separator();

    ImGui::MenuItem("Show Logger", "Ctrl+Shift+P", &show_log_window);
    ImGui::MenuItem("Show Sio", nullptr, &show_sio_window);
    ImGui::MenuItem("Show Perf", "Ctrl+Shift+K", &show_perf_window);
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

void ImguiBase::load_fat_device(gba::fat::Type type)
{
    gameboy_advance.set_fat_device_type(type);

    if (type == gba::fat::Type::NONE)
    {
        return;
    }

    // create new sd card if one is not found
    if (fat_sd_card.empty())
    {
        fat_sd_card = loadfile(FAT32_PATH);

        if (fat_sd_card.empty())
        {
            fat_sd_card.resize(512ULL * 1024ULL * 1024ULL);
            gameboy_advance.create_fat32_image(fat_sd_card);
            gameboy_advance.set_fat32_data(fat_sd_card);
            // do initial flush of sd card
            dumpfile(FAT32_PATH, fat_sd_card);
        }
        else
        {
            gameboy_advance.set_fat32_data(fat_sd_card);
        }
    }
    else
    {
        std::printf("empty\n");
    }

    // reset gba
    gameboy_advance.reset();

    // load save data (if any)
    loadsave(rom_path);
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

void ImguiBase::log_window()
{
    if (show_log_window)
    {
        ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
        logger.Draw("Logger", &gameboy_advance.log_type, &gameboy_advance.log_level, &show_log_window);
    }
}

static void sio_normal_window(gba::Gba& gba)
{
    debugger::io::io_title_16(gba::mem::IO_SIOCNT, REG_SIOCNT);

    static const char* shift_clock_list[] = { "External", "Internal" };
    static const char* internal_shift_clock_list[] = { "256KHz", "2MHz" };
    static const char* si_state_list[] = { "Low", "High/None" };
    static const char* so_state_list[] = { "Low", "High" };
    static const char* start_bit_list[] = { "Inactive/Ready", "Start/Active" };
    static const char* transfer_length_list[] = { "8bit", "32bit" };

    debugger::io::io_list<0, 0>(REG_SIOCNT, "Shift Clock", shift_clock_list);
    debugger::io::io_list<1, 1>(REG_SIOCNT, "Internal Clock Shift", internal_shift_clock_list);
    debugger::io::io_list<2, 2>(REG_SIOCNT, "SI State (opponents SO)", si_state_list);
    debugger::io::io_list<3, 3>(REG_SIOCNT, "SO during inacticity", so_state_list);
    debugger::io::io_list<7, 7>(REG_SIOCNT, "Start Bit", start_bit_list);
    debugger::io::io_list<12, 12>(REG_SIOCNT, "Transfer Length", transfer_length_list);
    debugger::io::io_button<14>(REG_SIOCNT, "IRQ Enable");
}

void ImguiBase::sio_window()
{
    if (!show_sio_window)
    {
        return;
    }

    if (ImGui::Begin("sio", &show_sio_window))
    {
        const auto mode = gba::sio::get_mode(gameboy_advance);
        ImGui::Text("[%s]", gba::sio::get_mode_str(mode));
        ImGui::Separator();

        switch (mode)
        {
            case gba::sio::Mode::Normal_8bit:
            case gba::sio::Mode::Normal_32bit:
                sio_normal_window(gameboy_advance);
                break;

            case gba::sio::Mode::MultiPlayer:
                ImGui::Text("Unimplemented");
                break;

            case gba::sio::Mode::UART:
                ImGui::Text("Unimplemented");
                break;

            case gba::sio::Mode::JOY_BUS:
                ImGui::Text("Unimplemented");
                break;

            case gba::sio::Mode::General:
                ImGui::Text("Unimplemented");
                break;
        }
    }
    ImGui::End();
}

void ImguiBase::perf_window()
{
    if (!show_perf_window || cycles_per_frame.empty())
    {
        return;
    }

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    {
        int corner = 0; // top left
        const float PAD = 10.0F;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos;
        ImVec2 window_pos_pivot;
        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0F : 0.0F;
        window_pos_pivot.y = (corner & 2) ? 1.0F : 0.0F;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
    }

    ImGui::SetNextWindowBgAlpha(0.75F); // Transparent background
    if (ImGui::Begin("perf", &show_perf_window, window_flags))
    {
        const auto last_frame = static_cast<double>(cycles_per_frame.back());
        const double max = gameboy_advance.get_cycles_per_frame();
        const double diff = max - last_frame;
        const double perf3 = 100.0 - (diff * 100.0 / max);

        static double last_value = 0;
        static int counter = 0;
        static int rate = 60;
        static int current = 1;
        static const char* list[] = { "None", "Lines", "Histogram" };

        counter++;
        if (counter >= rate)
        {
            last_value = perf3;
            counter = 0;
        }
        ImGui::Text("cpu usage: %0.2F%%\n", last_value);
        HelpMarker("this is calculated via time not spent in halt");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        if (ImGui::InputInt("rate", &rate, 0))
        {
            rate = std::clamp(rate, 1, 300);
        }
        HelpMarker("how often should `cpu usage` text update");

        if (current == 1)
        {
            ImGui::PlotLines("##cycles2", cycles_per_frame.data(), cycles_per_frame.size(), 0, nullptr, 0, max, ImVec2(300, 80));
        }
        else if (current == 2)
        {
            ImGui::PlotHistogram("##cycles1", cycles_per_frame.data(), cycles_per_frame.size(), 0, nullptr, 0, max, ImVec2(300, 80));
        }

        ImGui::SetNextItemWidth(60);
        if (ImGui::InputInt("samples", &max_cycles_per_frame_entries, 0))
        {
            max_cycles_per_frame_entries = std::clamp(max_cycles_per_frame_entries, 1, 10000);
        }
        HelpMarker("how many cycles per frame to record");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##histogram or lines", &current, list, 3);
    }
    ImGui::End();

    #if 0
    if (ImGui::Begin("perf", &show_perf_window))
    {
        const auto last_frame = static_cast<double>(cycles_per_frame.back());
        const double diff = static_cast<double>(gba::CYCLES_PER_FRAME) - last_frame;
        const double perf3 = 100.0 - (diff * 100.0 / static_cast<double>(gba::CYCLES_PER_FRAME));

        static double last_value = 0;
        static int counter = 0;

        counter++;
        if (counter >= 60)
        {
            last_value = perf3;
            counter = 0;
        }
        ImGui::Text("Cpu usage: %0.2F%%\n", last_value);

        ImGui::PlotLines("##cycles2", cycles_per_frame.data(), cycles_per_frame.size(), 0, "my lines", 0, gba::CYCLES_PER_FRAME, ImVec2(350, 120));
        ImGui::PlotHistogram("##cycles1", cycles_per_frame.data(), cycles_per_frame.size(), 0, "my histogram", 0, gba::CYCLES_PER_FRAME, ImVec2(350, 120));

        ImGui::InputInt("max samples", &max_cycles_per_frame_entries, 0);
    }
    ImGui::End();
    #endif
}
