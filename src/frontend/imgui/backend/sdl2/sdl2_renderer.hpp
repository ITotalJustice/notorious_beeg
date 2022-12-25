// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <SDL.h>
#include <utility>

namespace renderer::sdl2 {

auto init_pre_window() -> bool;
auto init_post_window(SDL_Window* window) -> bool;
void quit();

auto render_pre(SDL_Window* window) -> bool;
auto render_post(SDL_Window* window) -> bool;

auto create_texture(int id, int w, int h) -> bool;
auto get_texture(int id) -> void*;
auto update_texture(int id, int x, int y, int w, int h, void* pixels) -> bool;

auto get_render_size(SDL_Window* window) -> std::pair<int, int>;

} // namespace renderer::sdl2
