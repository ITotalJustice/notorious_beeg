// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "bit.hpp"
#include "mem.hpp"
#include <cstdint>
#include <imgui.h>
#include <gba.hpp>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>

// not tested enough to be placed in bit header
namespace bit {

template <u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto unset(const T value) -> T {
    static_assert(start <= end, "range is inverted! remember its lo, hi");
    static_assert(end < (sizeof(T) * 8));

    constexpr auto mask = get_mask<start, end, T>();

    return (value & ~mask);
}

template <u8 start, u8 end, IntV T> [[nodiscard]]
constexpr auto set(const T value, u32 new_v) -> T {
    static_assert(start <= end, "range is inverted! remember its lo, hi");
    static_assert(end < (sizeof(T) * 8));

    const auto unset_v = unset<start, end>(value);

    return unset_v | (new_v << start);
}

static_assert(unset<6, 7, int>(0xC0) == 0, "fail");
static_assert(
    set<6, 7, int>(0, 0x3) == 0xC0 &&
    set<6, 7, int>(1, 0x3) == 0xC1,
    "fail"
);

} // namespace bit

namespace sys::debugger::io {

using io_view_func = void(*)(gba::Gba& gba);

struct IoRegEntry
{
    const char* name;
    io_view_func func;
};

static auto io_title(auto addr, auto reg) -> void
{
    ImGui::Text("Addr: 0x%08X Value: 0x%04X", addr, reg);
    ImGui::Separator();
    ImGui::Spacing();
}

template<int start, int end>
static auto io_list(gba::Gba& gba, auto& reg, const char* name, std::span<const char*> items) -> void
{
    // 2 labels because label1 is the text shows.
    // label2 is the ID of the combo, prefixed with ##
    // could just use std::string, but rather avoid the allocs
    char label[128]{};
    char label2[128]{};

    // some lists might only be 1 bit
    if (start == end)
    {
        sprintf(label, "[0x%X] %s\n", start, name);
    }
    else
    {
        sprintf(label, "[0x%X-0x%X] %s\n", start, end, name);
    }

    sprintf(label2, "##%s", label);
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
static auto io_button(gba::Gba& gba, auto& reg, const char* name) -> void
{
    char label[128]{};
    std::sprintf(label, "[0x%X] %s", bit, name);
    const bool is_set = bit::is_set<bit>(reg);

    if (ImGui::RadioButton(label, is_set))
    {
        reg = bit::set<bit>(reg, is_set^1);
    }
}

template<int start, int end>
static auto io_button(gba::Gba& gba, auto& reg, const char* name) -> void
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

template<int start, int end, typename T>
static auto io_int(gba::Gba& gba, T& reg, const char* name) -> void
{
    char label[128]{};
    char label2[128]{};

    std::sprintf(label, "[0x%X-0x%X] %s", start, end, name);
    std::sprintf(label2, "##%s", label);

    ImGui::Text("%s", label);

    constexpr auto max = bit::get_mask<start, end, T>() >> start;
    const int old = bit::get_range<start, end>(reg);
    int value = old;

    ImGui::SliderInt(label2, &value, 0, max);

    // update if the value changed
    if (value != old)
    {
        reg = bit::set<start, end>(reg, value);
    }
}

static auto io_dispcnt(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_DISPCNT, REG_DISPCNT);

    static const char* modes[] = { "mode0 (4 reg)", "mode1 (2 reg, 1 affine)", "mode2 (4 affine)", "mode3 (bitmap)", "mode4 (bitmap)", "mode5 (bitmap)" };
    io_list<0x0, 0x2>(gba, REG_DISPCNT, "Mode", modes);
    ImGui::Separator();

    io_button<0x3>(gba, REG_DISPCNT, "GBC mode");
    io_button<0x4>(gba, REG_DISPCNT, "Page Flip");
    io_button<0x5>(gba, REG_DISPCNT, "Hblank force thing (unsure)");
    ImGui::Separator();

    static const char* obj_map[] = { "2D mapping", "1D mapping" };
    io_list<0x6, 0x6>(gba, REG_DISPCNT, "obj_map", obj_map);
    ImGui::Separator();

    io_button<0x7>(gba, REG_DISPCNT, "Force blanking (black screen)");
    ImGui::Separator();

    io_button<0x8>(gba, REG_DISPCNT, "BG0 enabled");
    io_button<0x9>(gba, REG_DISPCNT, "BG1 enabled");
    io_button<0xA>(gba, REG_DISPCNT, "BG2 enabled");
    io_button<0xB>(gba, REG_DISPCNT, "BG3 enabled");
    io_button<0xC>(gba, REG_DISPCNT, "OBJ enabled");
    ImGui::Separator();

    io_button<0xD>(gba, REG_DISPCNT, "Window 0 enabled");
    io_button<0xE>(gba, REG_DISPCNT, "Window 1 enabled");
    io_button<0xF>(gba, REG_DISPCNT, "Window OBJ enabled");
}

static auto io_dispstat(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_DISPSTAT, REG_DISPSTAT);

    io_button<0x0>(gba, REG_DISPSTAT, "vblank (in vblank)");
    io_button<0x1>(gba, REG_DISPSTAT, "hblank (in hblank)");
    io_button<0x2>(gba, REG_DISPSTAT, "vcount (vcount == lyc)");
    ImGui::Separator();

    io_button<0x3>(gba, REG_DISPSTAT, "enable vblank IRQ");
    io_button<0x4>(gba, REG_DISPSTAT, "enable hblank IRQ");
    io_button<0x5>(gba, REG_DISPSTAT, "enable vcount IRQ");
    ImGui::Separator();

    io_int<0x8, 0xF>(gba, REG_DISPSTAT, "lyc");
}

static auto io_vcount(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_VCOUNT, REG_VCOUNT);

    ImGui::Text("NOTE: messing with vcount is a sure way to\nbreak games!");
    ImGui::Separator();

    io_int<0x0, 0x7>(gba, REG_VCOUNT, "vcount");
}

static auto io_bgXcnt(gba::Gba& gba, auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_button<0, 1>(gba, reg, "Priority");
    ImGui::Separator();

    io_int<0x2, 0x3>(gba, reg, "tile data addr (addr * 0x4000)");
    ImGui::Separator();

    io_button<0x6>(gba, reg, "Mosaic effect");
    ImGui::Separator();

    static const char* colour_palette[] = { "8bpp (16 colours)", "4bpp (256 colours)" };
    io_list<0x7, 0x7>(gba, reg, "Colour Palette", colour_palette);
    ImGui::Separator();

    io_int<0x8, 0xC>(gba, reg, "char data addr (addr * 0x800)");
    ImGui::Separator();

    io_button<0xD>(gba, reg, "Screen over (unsure)");
    ImGui::Separator();

    // todo: check if affine vs text
    if (1)
    {
        static const char* tile_map_size[] = { "256x256 (32x32 tiles)", "512x256 (64x32 tiles)", "256x512 (32x64 tiles)", "512x512 (64x64 tiles)" };
        io_list<0xE, 0xF>(gba, reg, "Tile Map Size (text)", tile_map_size);
    }
    else
    {
        static const char* tile_map_size[] = { "128x128 (16x16 tiles)", "256x256 (32x32 tiles)", "512x512 (64x64 tiles)", "1024x1024 (128x128 tiles)" };
        io_list<0xE, 0xF>(gba, reg, "Tile Map Size (affine)", tile_map_size);
    }
}

static auto io_bg0cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba, gba::mem::IO_BG0CNT, REG_BG0CNT);
}

static auto io_bg1cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba, gba::mem::IO_BG1CNT, REG_BG1CNT);
}

static auto io_bg2cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba, gba::mem::IO_BG2CNT, REG_BG0CNT);
}

static auto io_bg3cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba, gba::mem::IO_BG3CNT, REG_BG0CNT);
}

static auto io_bgXHVofs(gba::Gba& gba, auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x9>(gba, reg, "Scroll value (pixels)");
}

static auto io_bg0hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG0HOFS, REG_BG0HOFS);
}

static auto io_bg0vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG0VOFS, REG_BG0VOFS);
}

static auto io_bg1hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG1HOFS, REG_BG1HOFS);
}

static auto io_bg1vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG1VOFS, REG_BG1VOFS);
}

static auto io_bg2hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG2HOFS, REG_BG2HOFS);
}

static auto io_bg2vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG2VOFS, REG_BG2VOFS);
}

static auto io_bg3hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG3HOFS, REG_BG3HOFS);
}

static auto io_bg3vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba, gba::mem::IO_BG3VOFS, REG_BG3VOFS);
}

static auto io_winXh(gba::Gba& gba, auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x7>(gba, reg, "X: Rightmost"); ImGui::Separator();
    io_int<0x8, 0xF>(gba, reg, "X: Leftmost");
}

static auto io_winXv(gba::Gba& gba, auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x7>(gba, reg, "Y: Bottom"); ImGui::Separator();
    io_int<0x8, 0xF>(gba, reg, "X: Top");
}

static auto io_win0h(gba::Gba& gba) -> void
{
    io_winXh(gba, gba::mem::IO_WIN0H, REG_WIN0H);
}

static auto io_win1h(gba::Gba& gba) -> void
{
    io_winXh(gba, gba::mem::IO_WIN1H, REG_WIN1H);
}

static auto io_win0v(gba::Gba& gba) -> void
{
    io_winXv(gba, gba::mem::IO_WIN0V, REG_WIN0V);
}

static auto io_win1v(gba::Gba& gba) -> void
{
    io_winXv(gba, gba::mem::IO_WIN1V, REG_WIN1V);
}

static auto io_winin(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_WININ, REG_WININ);

    io_button<0x0>(gba, REG_WININ, "BG0 in win0");
    io_button<0x1>(gba, REG_WININ, "BG1 in win0");
    io_button<0x2>(gba, REG_WININ, "BG2 in win0");
    io_button<0x3>(gba, REG_WININ, "BG3 in win0");
    io_button<0x4>(gba, REG_WININ, "OBJ in win0");
    io_button<0x5>(gba, REG_WININ, "Blend in win0");
    ImGui::Separator();

    io_button<0x8>(gba, REG_WININ, "BG0 in win1");
    io_button<0x9>(gba, REG_WININ, "BG1 in win1");
    io_button<0xA>(gba, REG_WININ, "BG2 in win1");
    io_button<0xB>(gba, REG_WININ, "BG3 in win1");
    io_button<0xC>(gba, REG_WININ, "OBJ in win1");
    io_button<0xD>(gba, REG_WININ, "Blend in win1");
}

static auto io_winout(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_WINOUT, REG_WINOUT);

    io_button<0x0>(gba, REG_WINOUT, "BG0 outside");
    io_button<0x1>(gba, REG_WINOUT, "BG1 outside");
    io_button<0x2>(gba, REG_WINOUT, "BG2 outside");
    io_button<0x3>(gba, REG_WINOUT, "BG3 outside");
    io_button<0x4>(gba, REG_WINOUT, "OBJ outside");
    io_button<0x5>(gba, REG_WINOUT, "Blend outside");
    ImGui::Separator();

    io_button<0x8>(gba, REG_WINOUT, "BG0 in OBJ win");
    io_button<0x9>(gba, REG_WINOUT, "BG1 in OBJ win");
    io_button<0xA>(gba, REG_WINOUT, "BG2 in OBJ win");
    io_button<0xB>(gba, REG_WINOUT, "BG3 in OBJ win");
    io_button<0xC>(gba, REG_WINOUT, "OBJ in OBJ win");
    io_button<0xD>(gba, REG_WINOUT, "Blend in OBJ win");
}

static auto io_mosaic(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_MOSAIC, REG_MOSAIC);

    io_int<0x0, 0x3>(gba, REG_MOSAIC, "BG X Size"); ImGui::Separator();
    io_int<0x4, 0x7>(gba, REG_MOSAIC, "BG Y Size"); ImGui::Separator();
    io_int<0x8, 0xB>(gba, REG_MOSAIC, "OBJ X Size"); ImGui::Separator();
    io_int<0xC, 0xF>(gba, REG_MOSAIC, "OBJ X Size");
}

static auto io_bldmod(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_BLDMOD, REG_BLDMOD);

    io_button<0x0>(gba, REG_BLDMOD, "Blend BG0 (src)");
    io_button<0x1>(gba, REG_BLDMOD, "Blend BG1 (src)");
    io_button<0x2>(gba, REG_BLDMOD, "Blend BG2 (src)");
    io_button<0x3>(gba, REG_BLDMOD, "Blend BG3 (src)");
    io_button<0x4>(gba, REG_BLDMOD, "Blend OBJ (src)");
    io_button<0x5>(gba, REG_BLDMOD, "Blend backdrop (src)");
    ImGui::Separator();

    static const char* modes[] = { "Off", "Alpha", "Lighten", "Darken" };
    io_list<0x6, 0x7>(gba, REG_BLDMOD, "Mode", modes);

    ImGui::Separator();
    io_button<0x8>(gba, REG_BLDMOD, "Blend BG0 (dst)");
    io_button<0x9>(gba, REG_BLDMOD, "Blend BG1 (dst)");
    io_button<0xA>(gba, REG_BLDMOD, "Blend BG2 (dst)");
    io_button<0xB>(gba, REG_BLDMOD, "Blend BG3 (dst)");
    io_button<0xC>(gba, REG_BLDMOD, "Blend OBJ (dst)");
    io_button<0xD>(gba, REG_BLDMOD, "Blend backdrop (dst)");
}

static auto io_colev(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_COLEV, REG_COLEV);

    io_int<0x0, 0x4>(gba, REG_COLEV, "src coeff (layer above)"); ImGui::Separator();
    io_int<0x8, 0xC>(gba, REG_COLEV, "dst coeff (layer below)");
}

static auto io_coley(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_COLEY, REG_COLEY);

    io_int<0x0, 0x4>(gba, REG_COLEY, "lighten/darken value");
}

static auto io_key(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_KEY, REG_KEY);

    io_button<0x0>(gba, REG_KEY, "Button::A");
    io_button<0x1>(gba, REG_KEY, "Button::B");
    io_button<0x2>(gba, REG_KEY, "Button::SELECT");
    io_button<0x3>(gba, REG_KEY, "Button::START");
    io_button<0x4>(gba, REG_KEY, "Button::RIGHT");
    io_button<0x5>(gba, REG_KEY, "Button::LEFT");
    io_button<0x6>(gba, REG_KEY, "Button::UP");
    io_button<0x7>(gba, REG_KEY, "Button::DOWN");
    io_button<0x8>(gba, REG_KEY, "Button::L");
    io_button<0x9>(gba, REG_KEY, "Button::R");
}

static auto io_ie_if(gba::Gba& gba, auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_button<0x0>(gba, reg, "vblank interrupt");
    io_button<0x1>(gba, reg, "hblank interrupt");
    io_button<0x2>(gba, reg, "vcount interrupt");
    ImGui::Separator();

    io_button<0x3>(gba, reg, "timer 0 interrupt");
    io_button<0x4>(gba, reg, "timer 1 interrupt");
    io_button<0x5>(gba, reg, "timer 2 interrupt");
    io_button<0x6>(gba, reg, "timer 3 interrupt");
    ImGui::Separator();

    io_button<0x7>(gba, reg, "serial interrupt");
    ImGui::Separator();

    io_button<0x8>(gba, reg, "dma 0 interrupt");
    io_button<0x9>(gba, reg, "dma 1 interrupt");
    io_button<0xA>(gba, reg, "dma 2 interrupt");
    io_button<0xB>(gba, reg, "dma 3 interrupt");
    ImGui::Separator();

    io_button<0xC>(gba, reg, "key interrupt");
    io_button<0xD>(gba, reg, "cassette interrupt");
}

static auto io_ie(gba::Gba& gba) -> void
{
    io_ie_if(gba, gba::mem::IO_IE, REG_IE);
}

static auto io_if(gba::Gba& gba) -> void
{
    io_ie_if(gba, gba::mem::IO_IF, REG_IF);
}

static auto io_ime(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_IME, REG_IME);

    io_button<0x0>(gba, REG_IME, "Master interrupt enable");
}

static auto unimpl_io_view(gba::Gba& gba) -> void
{
    ImGui::Text("Unimplemented");
}

static const std::array IO_NAMES =
{
    IoRegEntry{ "DISPCNT", io_dispcnt },
    IoRegEntry{ "DISPSTAT", io_dispstat },
    IoRegEntry{ "VCOUNT", io_vcount },
    IoRegEntry{ "BG0CNT", io_bg0cnt },
    IoRegEntry{ "BG1CNT", io_bg1cnt },
    IoRegEntry{ "BG2CNT", io_bg2cnt },
    IoRegEntry{ "BG3CNT", io_bg3cnt },
    IoRegEntry{ "BG0HOFS", io_bg0hofs },
    IoRegEntry{ "BG0VOFS", io_bg0vofs },
    IoRegEntry{ "BG1HOFS", io_bg1hofs },
    IoRegEntry{ "BG1VOFS", io_bg1vofs },
    IoRegEntry{ "BG2HOFS", io_bg2hofs },
    IoRegEntry{ "BG2VOFS", io_bg2vofs },
    IoRegEntry{ "BG3HOFS", io_bg3vofs },
    IoRegEntry{ "BG3VOFS", io_bg3hofs },
    IoRegEntry{ "WIN0H", io_win0h },
    IoRegEntry{ "WIN1H", io_win1h },
    IoRegEntry{ "WIN0V", io_win0v },
    IoRegEntry{ "WIN1V", io_win1v },
    IoRegEntry{ "WININ", io_winin },
    IoRegEntry{ "WINOUT", io_winout },
    IoRegEntry{ "MOSAIC", io_mosaic },
    IoRegEntry{ "BLDMOD", io_bldmod },
    IoRegEntry{ "COLEV", io_colev },
    IoRegEntry{ "COLEY", io_coley },
    IoRegEntry{ "SOUND1CNT_L", unimpl_io_view },
    IoRegEntry{ "SOUND1CNT_H", unimpl_io_view },
    IoRegEntry{ "SOUND1CNT_X", unimpl_io_view },
    IoRegEntry{ "SOUND2CNT_L", unimpl_io_view },
    IoRegEntry{ "SOUND2CNT_H", unimpl_io_view },
    IoRegEntry{ "SOUND3CNT_L", unimpl_io_view },
    IoRegEntry{ "SOUND3CNT_H", unimpl_io_view },
    IoRegEntry{ "SOUND3CNT_X", unimpl_io_view },
    IoRegEntry{ "SOUND4CNT_L", unimpl_io_view },
    IoRegEntry{ "SOUND4CNT_H", unimpl_io_view },
    IoRegEntry{ "SOUNDCNT_L", unimpl_io_view },
    IoRegEntry{ "SOUNDCNT_H", unimpl_io_view },
    IoRegEntry{ "SOUNDCNT_X", unimpl_io_view },
    IoRegEntry{ "SOUNDBIAS", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM0_L", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM0_H", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM1_L", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM1_H", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM2_L", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM2_H", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM3_L", unimpl_io_view },
    IoRegEntry{ "WAVE_RAM3_H", unimpl_io_view },
    IoRegEntry{ "FIFO_A_L", unimpl_io_view },
    IoRegEntry{ "FIFO_A_H", unimpl_io_view },
    IoRegEntry{ "FIFO_B_L", unimpl_io_view },
    IoRegEntry{ "FIFO_B_H", unimpl_io_view },
    IoRegEntry{ "DMA0SAD", unimpl_io_view },
    IoRegEntry{ "DMA1SAD", unimpl_io_view },
    IoRegEntry{ "DMA2SAD", unimpl_io_view },
    IoRegEntry{ "DMA3SAD", unimpl_io_view },
    IoRegEntry{ "DMA0DAD", unimpl_io_view },
    IoRegEntry{ "DMA1DAD", unimpl_io_view },
    IoRegEntry{ "DMA2DAD", unimpl_io_view },
    IoRegEntry{ "DMA3DAD", unimpl_io_view },
    IoRegEntry{ "DMA0CNT", unimpl_io_view },
    IoRegEntry{ "DMA1CNT", unimpl_io_view },
    IoRegEntry{ "DMA2CNT", unimpl_io_view },
    IoRegEntry{ "DMA3CNT", unimpl_io_view },
    IoRegEntry{ "DMA0CNT_L", unimpl_io_view },
    IoRegEntry{ "DMA1CNT_L", unimpl_io_view },
    IoRegEntry{ "DMA2CNT_L", unimpl_io_view },
    IoRegEntry{ "DMA3CNT_L", unimpl_io_view },
    IoRegEntry{ "DMA0CNT_H", unimpl_io_view },
    IoRegEntry{ "DMA1CNT_H", unimpl_io_view },
    IoRegEntry{ "DMA2CNT_H", unimpl_io_view },
    IoRegEntry{ "DMA3CNT_H", unimpl_io_view },
    IoRegEntry{ "TM0D", unimpl_io_view },
    IoRegEntry{ "TM1D", unimpl_io_view },
    IoRegEntry{ "TM2D", unimpl_io_view },
    IoRegEntry{ "TM3D", unimpl_io_view },
    IoRegEntry{ "TM0CNT", unimpl_io_view },
    IoRegEntry{ "TM1CNT", unimpl_io_view },
    IoRegEntry{ "TM2CNT", unimpl_io_view },
    IoRegEntry{ "TM3CNT", unimpl_io_view },
    IoRegEntry{ "KEY", io_key },
    IoRegEntry{ "IE", io_ie },
    IoRegEntry{ "IF", io_if },
    IoRegEntry{ "IME", io_ime },
    IoRegEntry{ "HALTCNT_L", unimpl_io_view },
    IoRegEntry{ "HALTCNT_H", unimpl_io_view },
};

auto render(gba::Gba& gba, bool* p_open) -> void
{
    // below is copied from imgui demo

    ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("io viewer", p_open))
    {
        // Left
        static int selected = 0;
        {
            ImGui::BeginChild("left pane", ImVec2(150, 0), true);
            for (auto i = 0; i < IO_NAMES.size(); i++)
            {
                if (ImGui::Selectable(IO_NAMES[i].name, selected == i))
                    selected = i;
            }
            ImGui::EndChild();
        }
        ImGui::SameLine();

        // Right
        {
            ImGui::BeginGroup();
            ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us
            ImGui::Text("%s", IO_NAMES[selected].name);
            ImGui::Separator();
            IO_NAMES[selected].func(gba);
            ImGui::EndChild();
            ImGui::EndGroup();
        }
    }
    ImGui::End();
}

} // namespace sys::debugger::io