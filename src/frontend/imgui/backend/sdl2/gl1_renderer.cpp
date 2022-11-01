// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include <glad/glad.h>
#include <cstdio>
#include <unordered_map>

#include "imgui_impl_opengl2.h"
#include "imgui_impl_opengl2.cpp"

namespace renderer::gl1 {
namespace {

std::unordered_map<int, GLuint> textures;

struct RestoreLastTexture
{
    RestoreLastTexture()
    {
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    }
    ~RestoreLastTexture()
    {
        glBindTexture(GL_TEXTURE_2D, last_texture);
    }

    GLint last_texture{};
};

} // namespace

auto init(void *(*proc)(const char *)) -> bool
{
    if (!gladLoadGLLoader(proc))
    {
        return false;
    }

    std::printf("Vendor:   %s\n", glGetString(GL_VENDOR));
    std::printf("Renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("Version:  %s\n", glGetString(GL_VERSION));
    ImGui_ImplOpenGL2_Init();
    return true;
}

void quit()
{
    for (auto& [_, text] : textures)
    {
        glDeleteTextures(1, &text);
    }

    textures.clear();
    ImGui_ImplOpenGL2_Shutdown();
}

auto render_pre() -> bool
{
    ImGui_ImplOpenGL2_NewFrame();
    return true;
}

auto render_post() -> bool
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    return true;
}

auto create_texture(int id, int w, int h) -> bool
{
    RestoreLastTexture last_texture;
    GLuint new_texture{};

    glGenTextures(1, &new_texture);
    glBindTexture(GL_TEXTURE_2D, new_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(
    	GL_TEXTURE_2D, // type
    	0, // idk
    	GL_RGB5, // internal format
    	w, h, // width, height
    	0, // idk
    	GL_RGBA, // idk
    	GL_UNSIGNED_SHORT_1_5_5_5_REV, // packing (555, 1 alpha)
    	nullptr
    );

    textures.insert({id, new_texture});
    return true;
}

auto get_texture(int id) -> void*
{
    const auto texture = textures.find(id);
    if (texture == textures.end())
    {
        // std::printf("failed to get texture: %d\n", id);
        return nullptr;
    }

    // https://github.com/ocornut/imgui/blob/22bcfca70055be41b12a3946132af58d4d736a58/backends/imgui_impl_opengl2.cpp#L259
    return (void*)(intptr_t)(texture->second);
}

auto update_texture(int id, int x, int y, int w, int h, void* pixels) -> bool
{
    RestoreLastTexture last_texture;

    const auto texture = get_texture(id);
    if (!texture)
    {
        return false;
    }

    // https://github.com/ocornut/imgui/blob/22bcfca70055be41b12a3946132af58d4d736a58/backends/imgui_impl_opengl2.cpp#L215
    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)(texture));
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0, // level
        0, 0, // x,y offset
        w, h, // width, hieght
        GL_RGBA,
        GL_UNSIGNED_SHORT_1_5_5_5_REV,
        pixels
    );

    return true;
}

} // namespace renderer::gl1
