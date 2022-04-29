// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// todo: this needs to be cleaned up and split into multiple files
#include <cstddef>
#include <cstring>
#include <gba.hpp>
#include <cstdint>
#include <string>
#include <chrono>

namespace sys {

enum class TextureID
{
    emu,
    layer0,
    layer1,
    layer2,
    layer3,
    folder_icon,
    file_icon,

    max, // not real texture
};

struct Rect
{
    int x, y, w, h;
};

struct System
{
    ~System();

    static auto init(int argc, char** argv) -> bool;
    static auto run() -> void;

    static auto loadrom(const std::string& path) -> bool;
    static auto closerom() -> void;

    static auto loadsave(const std::string& path) -> bool;
    static auto savegame(const std::string& path) -> bool;

    static auto loadstate(const std::string& path) -> bool;
    static auto savestate(const std::string& path) -> bool;

    static inline gba::Gba gameboy_advance{};

    static constexpr inline auto width = 240;
    static constexpr inline auto height = 160;
    static constexpr inline auto scale = 4;

    static inline Rect emu_rect{};

    // used for padding the size of the window to fit both the
    // menubar and the emu screen.
    static inline int menubar_height{0};
    static inline bool should_resize{true};

    static inline int emu_scale{scale};
    static inline int state_slot{};
    static inline std::string rom_path{};
    static inline bool has_rom{false};
    static inline bool running{true};
    static inline bool emu_run{false};
    static inline bool show_debug_window{false};
    static inline bool show_demo_window{false};
    static inline bool show_menubar{true};
    // inputs are ignored if not pressed inside window
    static inline bool inside_emu_window{true};

    static inline bool layer_enable_master{false};
    #if SPEED_TEST == 1
    static inline auto fps = 0;
    static inline auto start_time = std::chrono::high_resolution_clock::now();
    #endif

    // set to true to fill the screen
    static inline bool emu_stretch{false};

    struct Layer
    {
        Layer(TextureID i) : id{i} {};

        const TextureID id;
        uint16_t pixels[160][240];
        uint8_t priority;
        bool enabled;
    };

    static inline Layer layers[4]{ {TextureID::layer0}, {TextureID::layer1}, {TextureID::layer2}, {TextureID::layer3} };

    #ifdef NDEBUG
    static constexpr inline bool debug_mode{false};
    #else
    static constexpr inline bool debug_mode{true};
    #endif

    static inline bool viewer_io{false};

// todo: branch these off into structs
// private:
    static inline bool show_grid{false};

// private:
    static auto run_events() -> void;
    static auto run_emu() -> void;
    static auto run_render() -> void;

// misc
// private:
    static auto is_fullscreen() -> bool;
    static auto toggle_fullscreen() -> void;
    static auto resize_to_menubar() -> void;
    static auto resize_emu_screen() -> void;

// emu stuff
// private:
    static auto emu_update_texture() -> void;
    static auto emu_render() -> void;
    static auto emu_set_button(gba::Button button, bool down) -> void;

// imgui stuff
// private:
    static auto menubar_tab_file() -> void;
    static auto menubar_tab_emulation() -> void;
    static auto menubar_tab_options() -> void;
    static auto menubar_tab_tools() -> void;
    static auto menubar_tab_view() -> void;
    static auto menubar_tab_help() -> void;
    static auto menubar() -> void;

// debug stuff
// private:
    static auto im_debug_window() -> void;
    static auto render_layers() -> void;
    static auto toggle_master_layer_enable() -> void;
};
} // namespace sys
