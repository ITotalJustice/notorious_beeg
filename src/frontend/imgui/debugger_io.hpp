// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <bit.hpp>
#include <imgui.h>
#include <cstdint>
#include <cstdio>
#include <span>

namespace gba { struct Gba; }

namespace debugger::io {

auto io_title_8(auto addr, auto reg) -> void
{
    ImGui::Text("Addr: 0x%04X Value: 0x%02X", addr, reg);
    ImGui::Separator();
    ImGui::Spacing();
}

auto io_title_16(auto addr, auto reg) -> void
{
    ImGui::Text("Addr: 0x%08X Value: 0x%04X", addr, reg);
    ImGui::Separator();
    ImGui::Spacing();
}

template<int start, int end>
auto io_list(auto& reg, const char* name, std::span<const char*> items) -> void
{
    // 2 labels because label1 is the text shows.
    // label2 is the ID of the combo, prefixed with ##
    // could just use std::string, but rather avoid the allocs
    char label[128]{};
    char label2[128]{};

    // some lists might only be 1 bit
    if constexpr(start == end)
    {
        std::sprintf(label, "[0x%X] %s\n", start, name);
    }
    else
    {
        std::sprintf(label, "[0x%X-0x%X] %s\n", start, end, name);
    }

    std::sprintf(label2, "##%s", label);
    ImGui::Text("%s", label);

    const int old = bit::get_range<start, end>(reg);
    int current = old;

    ImGui::Combo(label2, &current, items.data(), items.size());

    // if the value changed, update it in the register
    if (old != current)
    {
        reg = bit::set<start, end>(reg, current);
    }
}

template<int bit>
auto io_button(auto& reg, const char* name) -> void
{
    char label[128]{};
    std::sprintf(label, "[0x%X] %s", bit, name);
    bool is_set = bit::is_set<bit>(reg);

    if (ImGui::Checkbox(label, &is_set))
    {
        reg = bit::set<bit>(reg, is_set);
    }
}

template<int start, int end>
auto io_button(auto& reg, const char* name) -> void
{
    static_assert(start < end);
    constexpr auto max = (end-start) * 4;
    const auto value = bit::get_range<start, end>(reg);

    ImGui::Text("[0x%X-0x%X] %s\n", start, end, name);

    for (auto i = 0; i < max; i++)
    {
        char label[128]{};
        std::sprintf(label, "%d", i);
        const bool is_set = i == value;

        if (ImGui::RadioButton(label, is_set))
        {
            reg = bit::set<start, end>(reg, i);
        }

        if (i + 1 < max)
        {
            ImGui::SameLine();
        }
    }
}

template<int start, int end, bool sign = false, typename T>
auto io_int(T& reg, const char* name) -> void
{
    char label[128]{};
    char label2[128]{};

    // some lists might only be 1 bit
    if constexpr(start == end)
    {
        std::sprintf(label, "[0x%X] %s\n", start, name);
    }
    else
    {
        std::sprintf(label, "[0x%X-0x%X] %s\n", start, end, name);
    }

    std::sprintf(label2, "##%s", label);

    ImGui::Text("%s", label);

    auto max = 0;
    auto min = 0;
    int old = bit::get_range<start, end>(reg);

    if constexpr(sign)
    {
        max = bit::get_mask<start, end-1, T>() >> start;
        min = -max - 1; // eg, min: -128, max: +127
        old = bit::sign_extend<end-start>(old);
    }
    else
    {
        max = bit::get_mask<start, end, T>() >> start;
    }

    int value = old;

    ImGui::SliderInt(label2, &value, min, max);

    // update if the value changed
    if (value != old)
    {
        reg = bit::set<start, end>(reg, value);
    }
}

auto render_gb(gba::Gba& gba, bool* p_open) -> void;
auto render_gba(gba::Gba& gba, bool* p_open) -> void;

} // namespace debugger::io
