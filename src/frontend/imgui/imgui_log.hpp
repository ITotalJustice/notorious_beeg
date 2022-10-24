// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <imgui.h>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

// from demo
struct ExampleAppLog
{
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    bool                AutoScroll;  // Keep scrolling if already at the bottom.
    int                 max{200};

    ExampleAppLog();

    void Clear();

    void AddLog(const char* fmt, ...) IM_FMTARGS(2)
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

    void Draw(const char* title, std::uint64_t* type_flags, std::uint64_t* level_flags, bool* p_open = nullptr);
};
