// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// todo: this needs to be cleaned up and split into multiple files
#include <cstddef>
#include <cstring>
#include <gba.hpp>
#include <cstdint>
#include <string>
#include <mutex>
#include <SDL.h>

namespace sys {

struct System
{
    ~System();

    auto init(int argc, char** argv) -> bool;
    auto run() -> void;
    auto on_key_event(const SDL_KeyboardEvent& e) -> void;

    auto loadrom(std::string path) -> bool;
    auto closerom() -> void;

    auto loadsave(std::string path) -> bool;
    auto savegame(std::string path) const -> bool;

    auto loadstate(std::string path) -> bool;
    auto savestate(std::string path) const -> bool;

    static inline gba::Gba gameboy_advance{};
    static inline SDL_Window* window{};
    static inline SDL_Renderer* renderer{};
    static inline SDL_Texture* texture{};
    static inline SDL_Texture* texture_bg_layer[4]{};
    static inline SDL_AudioDeviceID audio_device{};
    static inline SDL_AudioStream* audio_stream{};
    static inline SDL_AudioSpec aspec_wnt{};
    static inline SDL_AudioSpec aspec_got{};

    static constexpr inline auto width = 240;
    static constexpr inline auto height = 160;
    static constexpr inline auto scale = 4;

    static inline int emu_scale{scale};
    static inline int state_slot{};
    static inline std::string rom_path{};
    static inline bool has_rom{false};
    static inline bool running{true};
    static inline bool emu_run{true};
    static inline bool show_debug_window{false};
    static inline bool show_demo_window{false};
    static inline bool show_menubar{true};
    // inputs are ignored if not pressed inside window
    static inline bool inside_emu_window{true};

    static inline bool layer_enable_master{false};

    struct Layer
    {
        uint16_t pixels[160][240];
        uint8_t priority;
        bool enabled;
    };

    static inline Layer layers[4];
    static inline std::mutex audio_mutex{};

    #ifdef NDEBUG
    static inline const bool debug_mode{false};
    #else
    static inline const bool debug_mode{true};
    #endif

    static inline bool viewer_io{false};

// todo: branch these off into structs
private:
    // static inline bool
    static inline bool show_grid{false};

private:
    auto run_events() -> void;
    auto run_emu() -> void;
    auto run_render() -> void;

// misc
private:
    auto is_fullscreen() -> bool;
    auto toggle_fullscreen() -> void;

// emu stuff
private:
    auto emu_update_texture() -> void;
    auto emu_render() -> void;
    auto emu_set_button(gba::Button button, bool down) -> void;

// imgui stuff
private:
    auto menubar_tab_file() -> void;
    auto menubar_tab_emulation() -> void;
    auto menubar_tab_options() -> void;
    auto menubar_tab_tools() -> void;
    auto menubar_tab_view() -> void;
    auto menubar_tab_help() -> void;
    auto menubar() -> void;

// debug stuff
private:
    auto im_debug_window() -> void;
    auto render_layers() -> void;
    auto toggle_master_layer_enable() -> void;
};
} // namespace sys
