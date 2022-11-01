// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include <cstdio>

#include "sdl2_gl_renderer.hpp"
#include "gl1_renderer.hpp"
#include "imgui_impl_sdl.h"

namespace renderer::sdl2_gl1 {
namespace {

SDL_GLContext gl_context;

} // namespace

auto init_pre_window() -> bool
{
    constexpr auto gl_verion_major = 1;
    constexpr auto gl_verion_minor = 2;

    if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1))
    {
        std::printf("failed to set SDL_GL_DOUBLEBUFFER: %s\n", SDL_GetError());
    }
    if (SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24))
    {
        std::printf("failed to set SDL_GL_DEPTH_SIZE: %s\n", SDL_GetError());
    }
    if (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8))
    {
        std::printf("failed to set SDL_GL_STENCIL_SIZE: %s\n", SDL_GetError());
    }
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, gl_verion_major))
    {
        std::printf("failed to set SDL_GL_CONTEXT_MAJOR_VERSION: %s\n", SDL_GetError());
    }
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, gl_verion_minor))
    {
        std::printf("failed to set SDL_GL_CONTEXT_MINOR_VERSION: %s\n", SDL_GetError());
    }

    return true;
}

auto init_post_window(SDL_Window* window) -> bool
{
    gl_context = SDL_GL_CreateContext(window);

    if (!gl_context) {
        std::printf("failed to create gl context: %s\n", SDL_GetError());
        goto fail;
    }

    if (SDL_GL_MakeCurrent(window, gl_context)) {
        std::printf("failed to make gl current: %s\n", SDL_GetError());
        goto fail;
    }

    if (SDL_GL_SetSwapInterval(1))
    {
        std::printf("failed to setup vblank: %s\n", SDL_GetError());
        goto fail;
    }

    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context))
    {
        std::printf("ImGui_ImplSDL2_InitForOpenGL failed: %s\n", SDL_GetError());
        goto fail;
    }

    if (!gl1::init(SDL_GL_GetProcAddress))
    {
        std::printf("gl1::init failed: %s\n", SDL_GetError());
        goto fail;
    }

    return true;

fail:
    quit();
    return false;
}

void quit()
{
    gl1::quit();
    ImGui_ImplSDL2_Shutdown();

    if (gl_context)
    {
        SDL_GL_DeleteContext(gl_context);
        gl_context = nullptr;
    }
}

auto render_pre(SDL_Window* window) -> bool
{
    gl1::render_pre();
    ImGui_ImplSDL2_NewFrame();

    if (SDL_GL_MakeCurrent(window, gl_context))
    {
        std::printf("failed to make gl current: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

auto render_post(SDL_Window* window) -> bool
{
    gl1::render_post();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }
    else
    {
        assert(0);
    }

    SDL_GL_SwapWindow(window);
    return true;
}

auto create_texture(int id, int w, int h) -> bool
{
    return gl1::create_texture(id, w, h);
}

auto get_texture(int id) -> void*
{
    return gl1::get_texture(id);
}

auto update_texture(int id, int x, int y, int w, int h, void* pixels) -> bool
{
    return gl1::update_texture(id, x, y, w, h, pixels);
}

auto get_render_size(SDL_Window* window) -> std::pair<int, int>
{
    int w;
    int h;
    SDL_GL_GetDrawableSize(window, &w, &h);

    return {w, h};
}

} // namespace renderer::sdl2_gl1
