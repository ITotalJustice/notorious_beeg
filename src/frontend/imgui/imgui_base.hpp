// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <frontend_base.hpp>
#include "imgui_log.hpp"
#include <vector>

enum class TextureID
{
    emu,
    layer0,
    layer1,
    layer2,
    layer3,
};

struct Rect
{
    int x, y, w, h;
};

struct ImguiBase : frontend::Base
{
    ImguiBase(int argc, char** argv);
    ~ImguiBase() override;

    virtual auto poll_events() -> void = 0;
    virtual auto render_begin() -> void = 0;
    virtual auto render_end() -> void = 0;

    virtual auto get_texture(TextureID id) -> void* = 0;
    virtual auto update_texture(TextureID id, std::uint16_t pixels[160][240]) -> void = 0;

    virtual auto get_window_size() -> std::pair<int, int> = 0;
    virtual auto set_window_size(std::pair<int, int> new_size) -> void = 0;

    virtual auto is_fullscreen() -> bool = 0;
    virtual auto toggle_fullscreen() -> void = 0;

    virtual auto open_url(const char* url) -> void = 0;

    auto set_button(gba::Button button, bool down) -> void override;

protected:
    auto run_render() -> void;

    auto resize_to_menubar() -> void;
    auto resize_emu_screen() -> void;

    auto emu_update_texture() -> void;
    auto emu_render() -> void;

    auto menubar_tab_file() -> void;
    auto menubar_tab_emulation() -> void;
    auto menubar_tab_options() -> void;
    auto menubar_tab_tools() -> void;
    auto menubar_tab_view() -> void;
    auto menubar_tab_help() -> void;
    auto menubar() -> void;

    void load_fat_device(gba::fat::Type type);

    // debug
    auto im_debug_window() -> void;
    auto render_layers() -> void;
    auto toggle_master_layer_enable() -> void;
    void log_window();
    void sio_window();
    void perf_window();

public:
    #if DEBUGGER == 0
        static constexpr inline bool debug_mode{false};
    #else
        static constexpr inline bool debug_mode{true};
    #endif
public:
    std::uint16_t pixels[160][240];

    Rect emu_rect{};
    int emu_scale{scale};

    int menubar_height{0};
    bool should_resize{true};

    bool show_debug_window{false};
    bool show_demo_window{false};
    bool show_menubar{true};

    bool show_log_window{false};
    bool show_sio_window{false};
    bool show_perf_window{false};

    bool inside_emu_window{true};
    bool layer_enable_master{false};

    std::vector<std::uint8_t> fat_sd_card;

    struct Layer
    {
        Layer(TextureID i) : id{i} {}

        const TextureID id;
        uint16_t pixels[160][240];
        uint8_t priority;
        bool enabled;
    };

    Layer layers[4]{ {TextureID::layer0}, {TextureID::layer1}, {TextureID::layer2}, {TextureID::layer3} };

    bool viewer_io{false};

    bool show_grid{false};

    ExampleAppLog logger{};

    std::vector<float> cycles_per_frame;
    int max_cycles_per_frame_entries{100};
};
