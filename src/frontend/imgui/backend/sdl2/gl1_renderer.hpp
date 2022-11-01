// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace renderer::gl1 {

auto init(void *(*proc)(const char *)) -> bool;
auto init_post_window() -> bool;
void quit();

auto render_pre() -> bool;
auto render_post() -> bool;

auto create_texture(int id, int w, int h) -> bool;
auto get_texture(int id) -> void*;
auto update_texture(int id, int x, int y, int w, int h, void* pixels) -> bool;

} // namespace renderer::gl1
