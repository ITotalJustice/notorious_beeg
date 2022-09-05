// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "../frontend_base.hpp"

#include <SDL.h>
#include <unordered_map>
#include <mutex>

namespace frontend::sdl2 {

struct Sdl2Base : frontend::Base
{
    Sdl2Base(int argc, char** argv);
    ~Sdl2Base() override;

public:
    virtual auto init_audio(void* user, SDL_AudioCallback sdl2_cb, gba::AudioCallback gba_cb, int sample_rate = 65536) -> bool;

    auto set_button(gba::Button button, bool down) -> void override;

    auto loop() -> void override;
    virtual auto step() -> void;

    virtual auto poll_events() -> void;
    virtual auto run(double delta) -> void;
    virtual auto render() -> void = 0;

    virtual auto on_key_event(const SDL_KeyboardEvent& e) -> void;
    virtual auto on_display_event(const SDL_DisplayEvent& e) -> void;
    virtual auto on_window_event(const SDL_WindowEvent& e) -> void;
    virtual auto on_dropfile_event(SDL_DropEvent& e) -> void;
    virtual auto on_user_event(SDL_UserEvent& e) -> void {}
    virtual auto on_controlleraxis_event(const SDL_ControllerAxisEvent& e) -> void;
    virtual auto on_controllerbutton_event(const SDL_ControllerButtonEvent& e) -> void;
    virtual auto on_controllerdevice_event(const SDL_ControllerDeviceEvent& e) -> void;
    virtual auto on_mousebutton_event(const SDL_MouseButtonEvent& e) -> void {}
    virtual auto on_mousemotion_event(const SDL_MouseMotionEvent& e) -> void {}
    virtual auto on_touch_event(const SDL_TouchFingerEvent& e) -> void {}

    virtual auto is_fullscreen() const -> bool;
    virtual auto toggle_fullscreen() -> void;
    // convert x,y window coor to renderer
    virtual auto get_window_to_render_scale(int mx, int my) const -> std::pair<int, int>;
    // convert x,y renderer coor to window
    virtual auto get_render_to_window_scale(int mx, int my) const -> std::pair<int, int>;

    virtual auto get_renderer_size() const -> std::pair<int, int>;
    virtual auto get_window_size() const -> std::pair<int, int>;
    virtual auto set_window_size(std::pair<int, int> new_size) const -> void;
    virtual auto set_window_size_from_renderer() -> void;
    virtual auto resize_emu_screen() -> void;

    virtual auto open_url(const char* url) -> void;
    virtual auto rom_file_picker() -> void {}

    // if tick_rom is true, when the stream doesnt have enough samples
    // itll run the rom until enough samples are generated.
    virtual auto fill_audio_data_from_stream(Uint8* data, int len, bool tick_rom = true) -> void;
    virtual auto fill_stream_from_sample_data() -> void;

    virtual auto update_pixels_from_gba() -> void;
    virtual auto update_texture_from_pixels() -> void;

public:
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    SDL_Texture* texture{};

    SDL_Rect emu_rect{};

    SDL_AudioDeviceID audio_device{};
    SDL_AudioStream* audio_stream{};
    SDL_AudioSpec aspec_wnt{};
    SDL_AudioSpec aspec_got{};
    std::mutex audio_mutex{};
    std::mutex core_mutex{};
    std::vector<std::int16_t> sample_data{};
    bool has_focus{true};

    std::uint16_t pixels[160][240]{};
    bool has_new_frame{false};

    std::unordered_map<Sint32, SDL_GameController*> controllers{};
};

} // namespace frontend::sdl2
