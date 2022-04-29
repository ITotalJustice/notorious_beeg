// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "fs.hpp"
#include "../backend.hpp"
#include "../../system.hpp"
#include "imgui_internal.h"
#include <imgui.h>
#include <vector>
#include <cstring>
#include <cstdio>

namespace nx::fs {
namespace {

struct Entry
{
    std::filesystem::path path;
    std::string filename;
    bool is_dir;
};

std::filesystem::path current_path;
std::vector<Entry> entries;
bool in_new_dir{true};

auto scan() -> void
{
    entries.clear();

    if (current_path.empty())
    {
        #if defined(__SWITCH__)
        current_path = "/";
        #else
        current_path = std::filesystem::current_path();
        #endif
    }

    // walk up dir
    if (current_path.has_parent_path())
    {
        entries.emplace_back(current_path.parent_path(), "../", true);
    }

    // should probably have a max here because uhhh
    // a folder having 10000+ entries would suck super bad
    for (const auto& entry : std::filesystem::directory_iterator{current_path})
    {
        const auto& path = entry.path();
        const auto filename = path.filename().string();

        if (filename.starts_with('.'))
        {
            continue;
        }

        if (entry.is_directory())
        {
            entries.emplace_back(path, filename, true);
        }
        else if (entry.is_regular_file())
        {
            if (!path.has_extension())
            {
                continue;
            }

            const auto extension = path.extension();

            if (extension != ".gba" && extension != ".zip")
            {
                continue;
            }

            entries.emplace_back(path, filename, false);
        }
    }
}

} // namespace

auto render() -> bool
{
    // ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    const auto window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    // fill the screen (as this fs is intended for consoles!)
    const auto [w, h] = sys::backend::get_window_size();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    if (ImGui::Begin("idk what this thing is", nullptr, window_flags))
    {
        // set title
        ImGui::Text("Path: %s\n", (current_path).string().c_str());
        ImGui::Spacing();

        const auto table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

        // basically i dont want the Begin() to have focus, i want the table
        // to alwways be focused.
        ImGui::SetNextWindowFocus();
        if (ImGui::BeginTable("##table fs", 1, table_flags))
        {
            // this stops back button being spammed
            static bool back_pressed = false;

            if (ImGui::IsNavInputDown(ImGuiNavInput_Cancel))
            {
                if (!back_pressed)
                {
                    ImGui::GetIO().NavInputs[ImGuiNavInput_Cancel] = 0;

                    if (current_path.has_parent_path())
                    {
                        printf("has parent path\n");
                        current_path = current_path.parent_path();
                        in_new_dir = true;
                    }

                    back_pressed = true;
                }
            }
            else
            {
                back_pressed = false;
            }

            // if we are in a new directory, scan the entries
            if (in_new_dir)
            {
                scan();
                in_new_dir = false;
            }

            const auto folder_icon = sys::backend::get_texture(sys::TextureID::folder_icon);
            const auto file_icon = sys::backend::get_texture(sys::TextureID::file_icon);

            for (const auto& entry : entries)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                const auto texture = entry.is_dir ? folder_icon : file_icon;
                if (texture != nullptr)
                {
                    ImGui::Image(texture, ImVec2(75, 50));
                }

                ImGui::SameLine();

                if (ImGui::Selectable(entry.filename.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 50)))
                {
                    if (entry.is_dir)
                    {
                        current_path = entry.path;
                        in_new_dir = true;
                    }
                    else if (sys::System::loadrom(entry.path.string()))
                    {
                        ImGui::EndTable();
                        ImGui::End();
                        return true;
                    }
                }
            }

        ImGui::EndTable();
        }
    }
    ImGui::End();
    // display the entries
    return false;
}

} // namespace nx::fs
