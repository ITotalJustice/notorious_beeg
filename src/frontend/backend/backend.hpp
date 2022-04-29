// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "../system.hpp"
#include <cstdint>
#include <span>
#include <utility>

namespace sys::backend {

[[nodiscard]] auto init() -> bool;
auto quit() -> void;

auto poll_events() -> void;
// used for setup
auto render_begin() -> void;
// render anything specific to the backend
auto render() -> void;
// flip the screen
auto render_end() -> void;

auto get_texture(TextureID id) -> void*;
auto update_texture(TextureID id, std::uint16_t pixels[160][240]) -> void;

// misc stuff
[[nodiscard]] auto get_window_size() -> std::pair<int, int>;
auto set_window_size(std::pair<int, int> new_size) -> void;

[[nodiscard]] auto is_fullscreen() -> bool;
auto toggle_fullscreen() -> void;

auto open_url(const char* url) -> void;

} // sys::backend
