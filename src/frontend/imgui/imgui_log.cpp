// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "imgui_log.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <log.hpp>

#include <cstddef>
#include <cstdarg>
#include <algorithm>
#include <cstdint>
#include <array>
#include <cstring>
#include <string_view>
#include <utility>

namespace {

[[nodiscard]] auto get_colour(std::string_view view)  -> std::pair<ImVec4, bool>
{
    if (view.contains("[FATAL]")) {
        return { ImVec4(1.0F, 0.4F, 0.4F, 1.0F), true };
    }
    else if (view.contains("[ERROR]")) {
        return { ImVec4(0.7F, 0.4F, 0.4F, 1.0F), true };
    }
    else if (view.contains("[WARN]")) {
        return { ImVec4(1.0F, 0.8F, 0.6F, 1.0F), true };
    }
    else if (view.contains("[INFO]")) {
        return { ImVec4(0.7F, 1.0F, 7.0F, 1.0F), true };
    }
    else if (view.contains("[DEBUG]")) {
        return { ImVec4(0.7F, 0.7F, 1.0F, 1.0F), true };
    }

    return {};
}

// same code as in imgui_widgets but uses u64 instead
auto CheckboxFlags(const char* label, std::uint64_t* flags, std::uint64_t flags_value) -> bool
{
    bool all_on = (*flags & flags_value) == flags_value;
    bool any_on = (*flags & flags_value) != 0;
    bool pressed;
    if (!all_on && any_on)
    {
        ImGuiContext& g = *GImGui;
        ImGuiItemFlags backup_item_flags = g.CurrentItemFlags;
        g.CurrentItemFlags |= ImGuiItemFlags_MixedValue;
        pressed = ImGui::Checkbox(label, &all_on);
        g.CurrentItemFlags = backup_item_flags;
    }
    else
    {
        pressed = ImGui::Checkbox(label, &all_on);

    }
    if (pressed)
    {
        if (all_on) {
            *flags |= flags_value;
        } else {
            *flags &= ~flags_value;
        }
    }
    return pressed;
}

} // namespace

ExampleAppLog::ExampleAppLog()
{
    AutoScroll = true;
    Clear();
}

void ExampleAppLog::Clear()
{
    Buf.clear();
    LineOffsets.clear();
    LineOffsets.push_back(0);
}

void ExampleAppLog::AddLog(const char* fmt, ...)
{
    if (max == 0) // this is broken atm
    {
        return;
    }

    int old_size = Buf.size();
    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);
    for (int new_size = Buf.size(); old_size < new_size; old_size++) {
        if (Buf[old_size] == '\n') {
            LineOffsets.push_back(old_size + 1);
        }
    }

    // if we have too many lines, then remove the first few lines
    // until we are in bounds
    if (LineOffsets.size() > max) {
        // how many lines to remove
        const auto to_remove = LineOffsets.size() - max;
        // offset of the last line to remove
        const auto last_offset = LineOffsets[to_remove];

        // erase all lines up until that point
        Buf.Buf.erase(Buf.begin(), Buf.begin() + last_offset);
        // erase all offsets until that point
        LineOffsets.erase(LineOffsets.begin(), LineOffsets.begin() + to_remove);

        // we need to adjust all the remaining offests
        for (auto& offset : LineOffsets) {
            offset -= last_offset;
        }
    }
}

void ExampleAppLog::Draw(const char* title, std::uint64_t* type_flags, std::uint64_t* level_flags, bool* p_open)
{
    if (!ImGui::Begin(title, p_open))
    {
        ImGui::End();
        return;
    }

    // Left
    {
        ImGui::BeginChild("left pane", ImVec2(150, 0), true);
        CheckboxFlags("ALL", type_flags, gba::log::FLAG_TYPE_ALL);
        const auto type_str = gba::log::get_type_str();
        for (std::size_t i = 0; i < type_str.size(); i++)
        {
            CheckboxFlags(type_str[i], type_flags, 1ULL << i);
        }
        ImGui::EndChild();
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::BeginChild("right pane");

    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Level:");
    ImGui::SameLine();
    CheckboxFlags("ALL", level_flags, gba::log::FLAG_LEVEL_ALL);
    const auto level_str = gba::log::get_level_str();
    for (std::size_t i = 0; i < level_str.size(); i++)
    {
        ImGui::SameLine();
        CheckboxFlags(level_str[i], level_flags, 1ULL << i);
    }

    // Main window
    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("Options");
    }
    ImGui::SameLine();
    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::InputInt("Max", &max, 0))
    {
        max = std::clamp(max, 0, 10000);
    }
    ImGui::SameLine();
    Filter.Draw("Filter", -100.0F);

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (clear) {
        Clear();
    }
    if (copy) {
        ImGui::LogToClipboard();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* buf = Buf.begin();
    const char* buf_end = Buf.end();
    if (Filter.IsActive())
    {
        // In this example we don't use the clipper when Filter is enabled.
        // This is because we don't have a random access on the result on our filter.
        // A real application processing logs with ten of thousands of entries may want to store the result of
        // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
        for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
        {
            const char* line_start = buf + LineOffsets[line_no];
            const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;

            if (Filter.PassFilter(line_start, line_end)) {

                const auto [colour, has_colour] = get_colour(std::string_view{line_start, line_end});

                if (has_colour) {
                    ImGui::PushStyleColor(ImGuiCol_Text, colour);
                }
                ImGui::TextUnformatted(line_start, line_end);
                if (has_colour) {
                    ImGui::PopStyleColor();
                }
            }
        }
    }
    else
    {
        // The simplest and easy way to display the entire buffer:
        //   ImGui::TextUnformatted(buf_begin, buf_end);
        // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
        // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
        // within the visible area.
        // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
        // on your side is recommended. Using ImGuiListClipper requires
        // - A) random access into your data
        // - B) items all being the  same height,
        // both of which we can handle since we an array pointing to the beginning of each line of text.
        // When using the filter (in the block of code above) we don't have random access into the data to display
        // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
        // it possible (and would be recommended if you want to search through tens of thousands of entries).
        ImGuiListClipper clipper;
        clipper.Begin(LineOffsets.Size);
        while (clipper.Step())
        {
            for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
            {
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;

                const auto [colour, has_colour] = get_colour(std::string_view{line_start, line_end});

                if (has_colour) {
                    ImGui::PushStyleColor(ImGuiCol_Text, colour);
                }
                ImGui::TextUnformatted(line_start, line_end);
                if (has_colour) {
                    ImGui::PopStyleColor();
                }
            }
        }
        clipper.End();
    }
    ImGui::PopStyleVar();

    if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0F);
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::End();
}
