// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "sdl2_renderer.hpp"
#include <SDL.h>
#include <cstdint>
#include <cstdio>
#include <unordered_map>

#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>

namespace renderer::sdl2 {
namespace {

SDL_Renderer* renderer{};
std::unordered_map<int, SDL_Texture*> textures;

} // namespace

auto init_pre_window() -> bool
{
    return true;
}

auto init_post_window(SDL_Window* window) -> bool
{
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::printf("%s\n", SDL_GetError());
        goto fail;
    }

    SDL_RenderSetVSync(renderer, 1);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    return true;

fail:
    quit();
    return false;
}

void quit()
{
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    for (auto& [_, text] : textures)
    {
        SDL_DestroyTexture(text);
    }

    if (renderer != nullptr)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    textures.clear();
}

auto render_pre(SDL_Window* window) -> bool
{
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    return true;
}

auto render_post(SDL_Window* window) -> bool
{
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
    return true;
}

auto create_texture(int id, int w, int h) -> bool
{
    auto new_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!new_texture)
    {
        std::printf("%s\n", SDL_GetError());
        return false;
    }

    textures.insert({id, new_texture});
    return true;
}

auto get_texture(int id) -> void*
{
    const auto texture = textures.find(id);
    if (texture == textures.end())
    {
        return nullptr;
    }
    return texture->second;
}

auto update_texture(int id, int x, int y, int w, int h, void* pixels) -> bool
{
    auto _texture = static_cast<SDL_Texture*>(get_texture(id));
    if (!_texture)
    {
        return false;
    }

    void* texture_pixels{};
    int pitch{};

    SDL_LockTexture(_texture, nullptr, &texture_pixels, &pitch);
        SDL_ConvertPixels(
            w, h, // w,h
            SDL_PIXELFORMAT_BGR555, pixels, w * sizeof(std::uint16_t), // src
            SDL_PIXELFORMAT_BGR555, texture_pixels, pitch // dst
        );
    SDL_UnlockTexture(_texture);

    return true;
}

auto get_render_size(SDL_Window* window) -> std::pair<int, int>
{
    int w;
    int h;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    return {w, h};
}

} // namespace renderer::sdl2
