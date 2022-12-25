// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "mem.hpp"
#include "debugger_io.hpp"
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <gba.hpp>
#include <bit.hpp>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>

namespace debugger::io {
namespace {

using io_view_func = void(*)(gba::Gba& gba);

struct IoRegEntry
{
    const char* name;
    io_view_func func;
};

auto io_title(auto addr, auto reg) -> void
{
    io_title_16(addr, reg);
}

auto io_dispcnt(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_DISPCNT, REG_DISPCNT);

    static const char* modes[] = { "mode0 (4 reg)", "mode1 (2 reg, 1 affine)", "mode2 (4 affine)", "mode3 (bitmap)", "mode4 (bitmap)", "mode5 (bitmap)" };
    io_list<0x0, 0x2>(REG_DISPCNT, "Mode", modes);
    ImGui::Separator();

    io_button<0x3>(REG_DISPCNT, "GBC mode");
    io_button<0x4>(REG_DISPCNT, "Page Flip");
    io_button<0x5>(REG_DISPCNT, "Hblank force thing (unsure)");
    ImGui::Separator();

    static const char* obj_map[] = { "2D mapping", "1D mapping" };
    io_list<0x6, 0x6>(REG_DISPCNT, "obj_map", obj_map);
    ImGui::Separator();

    io_button<0x7>(REG_DISPCNT, "Force blanking (black screen)");
    ImGui::Separator();

    io_button<0x8>(REG_DISPCNT, "BG0 enabled");
    io_button<0x9>(REG_DISPCNT, "BG1 enabled");
    io_button<0xA>(REG_DISPCNT, "BG2 enabled");
    io_button<0xB>(REG_DISPCNT, "BG3 enabled");
    io_button<0xC>(REG_DISPCNT, "OBJ enabled");
    ImGui::Separator();

    io_button<0xD>(REG_DISPCNT, "Window 0 enabled");
    io_button<0xE>(REG_DISPCNT, "Window 1 enabled");
    io_button<0xF>(REG_DISPCNT, "Window OBJ enabled");
}

auto io_dispstat(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_DISPSTAT, REG_DISPSTAT);

    io_button<0x0>(REG_DISPSTAT, "vblank (in vblank)");
    io_button<0x1>(REG_DISPSTAT, "hblank (in hblank)");
    io_button<0x2>(REG_DISPSTAT, "vcount (vcount == lyc)");
    ImGui::Separator();

    io_button<0x3>(REG_DISPSTAT, "enable vblank IRQ");
    io_button<0x4>(REG_DISPSTAT, "enable hblank IRQ");
    io_button<0x5>(REG_DISPSTAT, "enable vcount IRQ");
    ImGui::Separator();

    io_int<0x8, 0xF>(REG_DISPSTAT, "lyc");
}

auto io_vcount(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_VCOUNT, REG_VCOUNT);

    ImGui::Text("NOTE: messing with vcount is a sure way to\nbreak games!");
    ImGui::Separator();

    io_int<0x0, 0x7>(REG_VCOUNT, "vcount");
}

auto io_bgXcnt(auto addr, auto& reg) -> void
{
    io_title(addr, reg);
    // io_button<0x0, 0xF>(reg, "idk");

    io_button<0, 1>(reg, "Priority");
    ImGui::Separator();

    io_int<0x2, 0x3>(reg, "tile data addr (addr * 0x4000)");
    ImGui::Separator();

    io_button<0x6>(reg, "Mosaic effect");
    ImGui::Separator();

    static const char* colour_palette[] = { "8bpp (16 colours)", "4bpp (256 colours)" };
    io_list<0x7, 0x7>(reg, "Colour Palette", colour_palette);
    ImGui::Separator();

    io_int<0x8, 0xC>(reg, "char data addr (addr * 0x800)");
    ImGui::Separator();

    io_button<0xD>(reg, "Screen over (unsure)");
    ImGui::Separator();

    // todo: check if affine vs text
    if (1)
    {
        static const char* tile_map_size[] = { "256x256 (32x32 tiles)", "512x256 (64x32 tiles)", "256x512 (32x64 tiles)", "512x512 (64x64 tiles)" };
        io_list<0xE, 0xF>(reg, "Tile Map Size (text)", tile_map_size);
    }
    else
    {
        static const char* tile_map_size[] = { "128x128 (16x16 tiles)", "256x256 (32x32 tiles)", "512x512 (64x64 tiles)", "1024x1024 (128x128 tiles)" };
        io_list<0xE, 0xF>(reg, "Tile Map Size (affine)", tile_map_size);
    }
}

auto io_bg0cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba::mem::IO_BG0CNT, REG_BG0CNT);
}

auto io_bg1cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba::mem::IO_BG1CNT, REG_BG1CNT);
}

auto io_bg2cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba::mem::IO_BG2CNT, REG_BG2CNT);
}

auto io_bg3cnt(gba::Gba& gba) -> void
{
    io_bgXcnt(gba::mem::IO_BG3CNT, REG_BG3CNT);
}

auto io_bgXHVofs(auto addr, auto& reg) -> void
{
    io_title(addr, reg);
    io_int<0x0, 0x9>(reg, "Scroll value (pixels)");
}

auto io_bg0hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG0HOFS, REG_BG0HOFS);
}

auto io_bg0vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG0VOFS, REG_BG0VOFS);
}

auto io_bg1hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG1HOFS, REG_BG1HOFS);
}

auto io_bg1vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG1VOFS, REG_BG1VOFS);
}

auto io_bg2hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG2HOFS, REG_BG2HOFS);
}

auto io_bg2vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG2VOFS, REG_BG2VOFS);
}

auto io_bg3hofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG3HOFS, REG_BG3HOFS);
}

auto io_bg3vofs(gba::Gba& gba) -> void
{
    io_bgXHVofs(gba::mem::IO_BG3VOFS, REG_BG3VOFS);
}

auto io_bg23pabcd(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x7>(reg, "Fraction"); ImGui::Separator();
    io_int<0x8, 0xF, true>(reg, "Integer"); ImGui::Separator();
}

auto io_bg23xy(auto addr, auto& hi, auto& lo) -> void
{
    std::uint32_t reg = (hi << 16) | lo;
    io_title(addr, reg);

    io_int<0x0, 0x7>(reg, "Fraction"); ImGui::Separator();
    io_int<0x8, 27, true>(reg, "Integer"); ImGui::Separator();

    lo = reg & 0xFFFF;
    hi = (reg >> 16) & 0xFFFF;
}

auto IO_BG2PA(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG2PA, REG_BG2PA);
}

auto IO_BG2PB(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG2PB, REG_BG2PB);
}

auto IO_BG2PC(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG2PC, REG_BG2PC);
}

auto IO_BG2PD(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG2PD, REG_BG2PD);
}

auto IO_BG2X(gba::Gba& gba) -> void
{
    io_bg23xy(gba::mem::IO_BG2X, REG_BG2X_HI, REG_BG2X_LO);
}

auto IO_BG2Y(gba::Gba& gba) -> void
{
    io_bg23xy(gba::mem::IO_BG2Y, REG_BG2Y_HI, REG_BG2Y_LO);
}

auto IO_BG3PA(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG3PA, REG_BG3PA);
}

auto IO_BG3PB(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG3PB, REG_BG3PB);
}

auto IO_BG3PC(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG3PC, REG_BG3PC);
}

auto IO_BG3PD(gba::Gba& gba) -> void
{
    io_bg23pabcd(gba::mem::IO_BG3PD, REG_BG3PD);
}

auto IO_BG3X(gba::Gba& gba) -> void
{
    io_bg23xy(gba::mem::IO_BG3X, REG_BG3X_HI, REG_BG3X_LO);
}

auto IO_BG3Y(gba::Gba& gba) -> void
{
    io_bg23xy(gba::mem::IO_BG3Y, REG_BG3Y_HI, REG_BG3Y_LO);
}

auto io_winXh(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x7>(reg, "X: Rightmost"); ImGui::Separator();
    io_int<0x8, 0xF>(reg, "X: Leftmost");
}

auto io_winXv(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x7>(reg, "Y: Bottom"); ImGui::Separator();
    io_int<0x8, 0xF>(reg, "Y: Top");
}

auto io_win0h(gba::Gba& gba) -> void
{
    io_winXh(gba::mem::IO_WIN0H, REG_WIN0H);
}

auto io_win1h(gba::Gba& gba) -> void
{
    io_winXh(gba::mem::IO_WIN1H, REG_WIN1H);
}

auto io_win0v(gba::Gba& gba) -> void
{
    io_winXv(gba::mem::IO_WIN0V, REG_WIN0V);
}

auto io_win1v(gba::Gba& gba) -> void
{
    io_winXv(gba::mem::IO_WIN1V, REG_WIN1V);
}

auto io_winin(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_WININ, REG_WININ);

    io_button<0x0>(REG_WININ, "BG0 in win0");
    io_button<0x1>(REG_WININ, "BG1 in win0");
    io_button<0x2>(REG_WININ, "BG2 in win0");
    io_button<0x3>(REG_WININ, "BG3 in win0");
    io_button<0x4>(REG_WININ, "OBJ in win0");
    io_button<0x5>(REG_WININ, "Blend in win0");
    ImGui::Separator();

    io_button<0x8>(REG_WININ, "BG0 in win1");
    io_button<0x9>(REG_WININ, "BG1 in win1");
    io_button<0xA>(REG_WININ, "BG2 in win1");
    io_button<0xB>(REG_WININ, "BG3 in win1");
    io_button<0xC>(REG_WININ, "OBJ in win1");
    io_button<0xD>(REG_WININ, "Blend in win1");
}

auto io_winout(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_WINOUT, REG_WINOUT);

    io_button<0x0>(REG_WINOUT, "BG0 outside");
    io_button<0x1>(REG_WINOUT, "BG1 outside");
    io_button<0x2>(REG_WINOUT, "BG2 outside");
    io_button<0x3>(REG_WINOUT, "BG3 outside");
    io_button<0x4>(REG_WINOUT, "OBJ outside");
    io_button<0x5>(REG_WINOUT, "Blend outside");
    ImGui::Separator();

    io_button<0x8>(REG_WINOUT, "BG0 in OBJ win");
    io_button<0x9>(REG_WINOUT, "BG1 in OBJ win");
    io_button<0xA>(REG_WINOUT, "BG2 in OBJ win");
    io_button<0xB>(REG_WINOUT, "BG3 in OBJ win");
    io_button<0xC>(REG_WINOUT, "OBJ in OBJ win");
    io_button<0xD>(REG_WINOUT, "Blend in OBJ win");
}

auto io_mosaic(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_MOSAIC, REG_MOSAIC);

    io_int<0x0, 0x3>(REG_MOSAIC, "BG X Size"); ImGui::Separator();
    io_int<0x4, 0x7>(REG_MOSAIC, "BG Y Size"); ImGui::Separator();
    io_int<0x8, 0xB>(REG_MOSAIC, "OBJ X Size"); ImGui::Separator();
    io_int<0xC, 0xF>(REG_MOSAIC, "OBJ X Size");
}

auto io_bldmod(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_BLDMOD, REG_BLDMOD);

    io_button<0x0>(REG_BLDMOD, "Blend BG0 (src)");
    io_button<0x1>(REG_BLDMOD, "Blend BG1 (src)");
    io_button<0x2>(REG_BLDMOD, "Blend BG2 (src)");
    io_button<0x3>(REG_BLDMOD, "Blend BG3 (src)");
    io_button<0x4>(REG_BLDMOD, "Blend OBJ (src)");
    io_button<0x5>(REG_BLDMOD, "Blend backdrop (src)");
    ImGui::Separator();

    static const char* modes[] = { "Off", "Alpha", "Lighten", "Darken" };
    io_list<0x6, 0x7>(REG_BLDMOD, "Mode", modes);

    ImGui::Separator();
    io_button<0x8>(REG_BLDMOD, "Blend BG0 (dst)");
    io_button<0x9>(REG_BLDMOD, "Blend BG1 (dst)");
    io_button<0xA>(REG_BLDMOD, "Blend BG2 (dst)");
    io_button<0xB>(REG_BLDMOD, "Blend BG3 (dst)");
    io_button<0xC>(REG_BLDMOD, "Blend OBJ (dst)");
    io_button<0xD>(REG_BLDMOD, "Blend backdrop (dst)");
}

auto io_colev(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_COLEV, REG_COLEV);

    io_int<0x0, 0x4>(REG_COLEV, "src coeff (layer above)"); ImGui::Separator();
    io_int<0x8, 0xC>(REG_COLEV, "dst coeff (layer below)");
}

auto io_coley(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_COLEY, REG_COLEY);
    io_int<0x0, 0x4>(REG_COLEY, "lighten/darken value");
}

auto io_NRX1_NRX2(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    static const char* wave_pattern_duty_list[] = { "12.5%", "25%", "50%", "75%" };
    static const char* envelope_direction_list[] = { "Decrease", "Increase" };

    io_int<0x0, 0x5>(reg, "Sound Length");
    io_list<0x6, 0x7>(reg, "Wave Pattern Duty", wave_pattern_duty_list);
    io_int<0x8, 0xA>(reg, "Envelope Step Time");
    io_list<0xB, 0xB>(reg, "Envelope Direction", envelope_direction_list);
    io_int<0xC, 0xF>(reg, "Initial Volume");
}

auto io_NRX3_NRX4(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_int<0x0, 0x7>(reg, "Freq Lower 8bits");
    io_int<0x8, 0xA>(reg, "Freq Upper 3bits");
    io_button<0xE>(reg, "Length Enable Flag");
    io_button<0xF>(reg, "trigger");
}

auto io_SOUND1CNT_L(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUND1CNT_L, REG_SOUND1CNT_L);

    static const char* sweep_direction_list[] = { "Increase", "Decrease" };

    io_int<0x0, 0x2>(REG_SOUND1CNT_L, "sweep shift");
    io_list<0x3, 0x3>(REG_SOUND1CNT_L, "sweep direction", sweep_direction_list);
    io_int<0x4, 0x6>(REG_SOUND1CNT_L, "sweep time");
}

auto io_SOUND1CNT_H(gba::Gba& gba) -> void
{
    io_NRX1_NRX2(gba::mem::IO_SOUND1CNT_H, REG_SOUND1CNT_H);
}

auto io_SOUND1CNT_X(gba::Gba& gba) -> void
{
    io_NRX3_NRX4(gba::mem::IO_SOUND1CNT_X, REG_SOUND1CNT_X);
}

auto io_SOUND2CNT_L(gba::Gba& gba) -> void
{
    io_NRX1_NRX2(gba::mem::IO_SOUND2CNT_L, REG_SOUND2CNT_L);
}

auto io_SOUND2CNT_H(gba::Gba& gba) -> void
{
    io_NRX3_NRX4(gba::mem::IO_SOUND2CNT_H, REG_SOUND2CNT_H);
}

auto io_SOUND3CNT_L(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUND3CNT_L, REG_SOUND3CNT_L);

    static const char* bank_mode_list[] = { "1 bank (32 entries)", "2 banks (64 entries)" };

    io_list<0x5, 0x5>(REG_SOUND3CNT_L, "Wave Bank Mode", bank_mode_list);
    io_int<0x6, 0x6>(REG_SOUND3CNT_L, "Wave Bank Number");
    io_button<0x7>(REG_SOUND3CNT_L, "Dac Power");
}

auto io_SOUND3CNT_H(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUND3CNT_H, REG_SOUND3CNT_H);

    static const char* sound_volume_table[] = { "0%", "100%", "50%", "25%" };
    static const char* force_volume_table[] = { "Off", "Forced 75%", };

    io_int<0x0, 0x7>(REG_SOUND3CNT_H, "Sound Length");
    io_list<0xD, 0xE>(REG_SOUND3CNT_H, "Sound Volume", sound_volume_table);
    io_list<0xF, 0xF>(REG_SOUND3CNT_H, "Force Volume", force_volume_table);
}

auto io_SOUND3CNT_X(gba::Gba& gba) -> void
{
    io_NRX3_NRX4(gba::mem::IO_SOUND3CNT_X, REG_SOUND3CNT_X);
}

auto io_SOUND4CNT_L(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUND4CNT_L, REG_SOUND4CNT_L);

    static const char* envelope_direction_list[] = { "Decrease", "Increase" };

    io_int<0x0, 0x5>(REG_SOUND4CNT_L, "Sound Length");
    io_int<0x8, 0xA>(REG_SOUND4CNT_L, "Envelope Step Time");
    io_list<0xB, 0xB>(REG_SOUND4CNT_L, "Envelope Direction", envelope_direction_list);
    io_int<0xC, 0xF>(REG_SOUND4CNT_L, "Initial Volume");
}

auto io_SOUND4CNT_H(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUND4CNT_H, REG_SOUND4CNT_H);

    static const char* counter_width_list[] = { "15-bits", "7-bits" };

    io_int<0x0, 0x2>(REG_SOUND4CNT_H, "Dividing Retio of Freq");
    io_list<0x3, 0x3>(REG_SOUND4CNT_H, "Counter Width", counter_width_list);
    io_int<0x4, 0x7>(REG_SOUND4CNT_H, "Shift Clock Freq");
    io_button<0xE>(REG_SOUND4CNT_H, "Length Enable Flag");
    io_button<0xF>(REG_SOUND4CNT_H, "trigger");
}

auto io_SOUNDCNT_L(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUNDCNT_L, REG_SOUNDCNT_L);

    io_int<0x0, 0x2>(REG_SOUNDCNT_L, "PSG Vol Right");
    io_int<0x4, 0x6>(REG_SOUNDCNT_L, "PSG Vol Left");
    ImGui::Separator();

    io_button<0x8>(REG_SOUNDCNT_L, "PSG Sound 1 Right Enable");
    io_button<0x9>(REG_SOUNDCNT_L, "PSG Sound 2 Right Enable");
    io_button<0xA>(REG_SOUNDCNT_L, "PSG Sound 3 Right Enable");
    io_button<0xB>(REG_SOUNDCNT_L, "PSG Sound 4 Right Enable");
    ImGui::Separator();

    io_button<0xC>(REG_SOUNDCNT_L, "PSG Sound 1 Left Enable");
    io_button<0xD>(REG_SOUNDCNT_L, "PSG Sound 2 Left Enable");
    io_button<0xE>(REG_SOUNDCNT_L, "PSG Sound 3 Left Enable");
    io_button<0xF>(REG_SOUNDCNT_L, "PSG Sound 4 Left Enable");
}

auto io_SOUNDCNT_H(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUNDCNT_H, REG_SOUNDCNT_H);

    static const char* psg_vol_list[] = { "25%", "50%", "100%", "Prohibited" };
    static const char* fifo_vol_list[] = { "50%", "100%" };
    static const char* fifo_timer_list[] = { "Timer 0", "Timer 1" };

    io_list<0x0, 0x1>(REG_SOUNDCNT_H, "PSG Volume", psg_vol_list);
    io_list<0x2, 0x2>(REG_SOUNDCNT_H, "Fifo A Volume", fifo_vol_list);
    io_list<0x3, 0x3>(REG_SOUNDCNT_H, "Fifo B Volume", fifo_vol_list);
    ImGui::Separator();

    io_button<0x8>(REG_SOUNDCNT_H, "Fifo A Right Enable");
    io_button<0x9>(REG_SOUNDCNT_H, "Fifo A Left Enable");
    io_list<0xA, 0xA>(REG_SOUNDCNT_H, "Fifo A Timer", fifo_timer_list);
    io_button<0xB>(REG_SOUNDCNT_H, "Fifo A Reset");
    ImGui::Separator();

    io_button<0xC>(REG_SOUNDCNT_H, "Fifo B Right Enable");
    io_button<0xD>(REG_SOUNDCNT_H, "Fifo B Left Enable");
    io_list<0xE, 0xE>(REG_SOUNDCNT_H, "Fifo B Timer", fifo_timer_list);
    io_button<0xF>(REG_SOUNDCNT_H, "Fifo B Reset");
}

auto io_SOUNDCNT_X(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUNDCNT_X, REG_SOUNDCNT_X);

    io_button<0x0>(REG_SOUNDCNT_X, "Sound 1 ON");
    io_button<0x1>(REG_SOUNDCNT_X, "Sound 2 ON");
    io_button<0x2>(REG_SOUNDCNT_X, "Sound 3 ON");
    io_button<0x3>(REG_SOUNDCNT_X, "Sound 4 ON");
    ImGui::Separator();

    io_button<0x7>(REG_SOUNDCNT_X, "Master Enable");
}

auto io_SOUNDBIAS(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_SOUNDBIAS, REG_SOUNDBIAS);

    static const char* resample_list[] = { "9bit / 32.768kHz", "8bit / 65.536kHz", "7bit / 131.072kHz", "6bit / 262.144kHz" };
    io_int<0x1, 0x9>(REG_SOUNDBIAS, "Bias");
    io_list<0xE, 0xF>(REG_SOUNDBIAS, "Resample Mode", resample_list);
}

auto io_WAVE_RAMX(auto addr, auto& reg) -> void
{
    io_title(addr, reg);
    io_int<0x0, 0x3>(reg, "sample 0");
    io_int<0x4, 0x7>(reg, "sample 1");
    io_int<0x8, 0xB>(reg, "sample 2");
    io_int<0xC, 0xF>(reg, "sample 3");
}

auto io_WAVE_RAM0_L(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM0_L, REG_WAVE_RAM0_L);
}

auto io_WAVE_RAM0_H(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM0_H, REG_WAVE_RAM0_H);
}

auto io_WAVE_RAM1_L(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM1_L, REG_WAVE_RAM1_L);
}

auto io_WAVE_RAM1_H(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM1_H, REG_WAVE_RAM1_H);
}

auto io_WAVE_RAM2_L(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM2_L, REG_WAVE_RAM2_L);
}

auto io_WAVE_RAM2_H(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM2_H, REG_WAVE_RAM2_H);
}

auto io_WAVE_RAM3_L(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM3_L, REG_WAVE_RAM3_L);
}

auto io_WAVE_RAM3_H(gba::Gba& gba) -> void
{
    io_WAVE_RAMX(gba::mem::IO_WAVE_RAM3_H, REG_WAVE_RAM3_H);
}

template<int width>
auto io_DMAXXAD(auto addr, auto& hi, auto& lo, const char* txt)
{
    std::uint32_t reg = (hi << 16) | (lo);

    io_title(addr, reg);
    io_int<0x0, width>(reg, txt);

    lo = reg & 0xFFFF;
    hi = (reg >> 16) & 0xFFFF;
}

auto IO_DMA0SAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<26>(gba::mem::IO_DMA0SAD, REG_DMA0SAD_HI, REG_DMA0SAD_LO, "27-bit source address");
}

auto IO_DMA1SAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<27>(gba::mem::IO_DMA1SAD, REG_DMA1SAD_HI, REG_DMA1SAD_LO, "28-bit source address");
}

auto IO_DMA2SAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<27>(gba::mem::IO_DMA2SAD, REG_DMA2SAD_HI, REG_DMA2SAD_LO, "28-bit source address");
}

auto IO_DMA3SAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<27>(gba::mem::IO_DMA3SAD, REG_DMA3SAD_HI, REG_DMA3SAD_LO, "28-bit source address");
}

auto IO_DMA0DAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<26>(gba::mem::IO_DMA0DAD, REG_DMA0DAD_HI, REG_DMA0DAD_LO, "27-bit destition address");
}

auto IO_DMA1DAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<26>(gba::mem::IO_DMA1DAD, REG_DMA1DAD_HI, REG_DMA1DAD_LO, "27-bit destition address");
}

auto IO_DMA2DAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<26>(gba::mem::IO_DMA2DAD, REG_DMA2DAD_HI, REG_DMA2DAD_LO, "27-bit destition address");
}

auto IO_DMA3DAD(gba::Gba& gba) -> void
{
    io_DMAXXAD<27>(gba::mem::IO_DMA3DAD, REG_DMA3DAD_HI, REG_DMA3DAD_LO, "28-bit destition address");
}

auto IO_DMAXCNT(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    static const char* dst_inc[] = { "Increment", "Decrement", "Unchanged", "Increment/Reload" };
    io_list<0x5, 0x6>(reg, "Dst Inc Mode", dst_inc);
    ImGui::Separator();

    static const char* src_inc[] = { "Increment", "Decrement", "Unchanged", "Illegal" };
    io_list<0x7, 0x8>(reg, "Src Inc Mode", src_inc);
    ImGui::Separator();

    io_button<0x9>(reg, "Repeat");
    ImGui::Separator();

    static const char* sizes[] = { "16-bit", "32-bit" };
    io_list<0xA, 0xA>(reg, "Size", sizes);
    ImGui::Separator();

    io_button<0xB>(reg, "Unknown");
    ImGui::Separator();

    static const char* mode[] = { "Immediate", "Vblank (vdma)", "Hblank (hdma)", "Special" };
    io_list<0xC, 0xD>(reg, "Start Mode", mode);
    ImGui::Separator();

    io_button<0xE>(reg, "IRQ");
    io_button<0xF>(reg, "Enable");
}

auto IO_DMA0CNT_H(gba::Gba& gba) -> void
{
    IO_DMAXCNT(gba::mem::IO_DMA0CNT_H, REG_DMA0CNT_H);
}

auto IO_DMA1CNT_H(gba::Gba& gba) -> void
{
    IO_DMAXCNT(gba::mem::IO_DMA1CNT_H, REG_DMA1CNT_H);
}

auto IO_DMA2CNT_H(gba::Gba& gba) -> void
{
    IO_DMAXCNT(gba::mem::IO_DMA2CNT_H, REG_DMA2CNT_H);
}

auto IO_DMA3CNT_H(gba::Gba& gba) -> void
{
    IO_DMAXCNT(gba::mem::IO_DMA3CNT_H, REG_DMA3CNT_H);
}

auto IO_TMXCNT(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    static const char* freq[] = { "1-clk (16.78MHz)", "64-clk (262187.5KHz)", "256-clk (65546.875KHz)", "1024-clk (16386.71875KHz)" };
    io_list<0x0, 0x1>(reg, "Frequency", freq);
    ImGui::Separator();

    io_button<0x2>(reg, "Cascade");
    io_button<0x6>(reg, "IRQ");
    io_button<0x7>(reg, "Enable");
}

auto IO_TM0CNT(gba::Gba& gba) -> void
{
    IO_TMXCNT(gba::mem::IO_TM0CNT, REG_TM0CNT);
}

auto IO_TM1CNT(gba::Gba& gba) -> void
{
    IO_TMXCNT(gba::mem::IO_TM1CNT, REG_TM1CNT);
}

auto IO_TM2CNT(gba::Gba& gba) -> void
{
    IO_TMXCNT(gba::mem::IO_TM2CNT, REG_TM2CNT);
}

auto IO_TM3CNT(gba::Gba& gba) -> void
{
    IO_TMXCNT(gba::mem::IO_TM3CNT, REG_TM3CNT);
}

void IO_SIOCNT_general(gba::Gba& gba)
{

}

void IO_SIOCNT_multiplayer(gba::Gba& gba)
{

}

void IO_SIOCNT(gba::Gba& gba)
{

}

auto io_key(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_KEY, REG_KEY);

    io_button<0x0>(REG_KEY, "Button::A");
    io_button<0x1>(REG_KEY, "Button::B");
    io_button<0x2>(REG_KEY, "Button::SELECT");
    io_button<0x3>(REG_KEY, "Button::START");
    io_button<0x4>(REG_KEY, "Button::RIGHT");
    io_button<0x5>(REG_KEY, "Button::LEFT");
    io_button<0x6>(REG_KEY, "Button::UP");
    io_button<0x7>(REG_KEY, "Button::DOWN");
    io_button<0x8>(REG_KEY, "Button::L");
    io_button<0x9>(REG_KEY, "Button::R");
}

auto io_KEYCNT(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_KEYCNT, REG_KEYCNT);

    io_button<0x0>(REG_KEYCNT, "Button::A");
    io_button<0x1>(REG_KEYCNT, "Button::B");
    io_button<0x2>(REG_KEYCNT, "Button::SELECT");
    io_button<0x3>(REG_KEYCNT, "Button::START");
    io_button<0x4>(REG_KEYCNT, "Button::RIGHT");
    io_button<0x5>(REG_KEYCNT, "Button::LEFT");
    io_button<0x6>(REG_KEYCNT, "Button::UP");
    io_button<0x7>(REG_KEYCNT, "Button::DOWN");
    io_button<0x8>(REG_KEYCNT, "Button::L");
    io_button<0x9>(REG_KEYCNT, "Button::R");
    ImGui::Separator();

    io_button<14>(REG_KEYCNT, "IRQ enable");
    static const char* condition_list[] = { "Logical OR", "Logical AND" };
    io_list<15, 15>(REG_KEYCNT, "IRQ condition", condition_list);
}

auto io_RCNT(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_RCNT, REG_RCNT);

    io_int<0, 3>(REG_RCNT, "Undocumented");
    io_int<4, 8>(REG_RCNT, "Should be zero? (r/w)");
    io_int<9, 13>(REG_RCNT, "Always zero (r)");
    io_int<14, 14>(REG_RCNT, "Should be zero? (r/w)");
    io_int<15, 15>(REG_RCNT, "Must be zero");
}

auto io_ie_if(auto addr, auto& reg) -> void
{
    io_title(addr, reg);

    io_button<0x0>(reg, "vblank interrupt");
    io_button<0x1>(reg, "hblank interrupt");
    io_button<0x2>(reg, "vcount interrupt");
    ImGui::Separator();

    io_button<0x3>(reg, "timer 0 interrupt");
    io_button<0x4>(reg, "timer 1 interrupt");
    io_button<0x5>(reg, "timer 2 interrupt");
    io_button<0x6>(reg, "timer 3 interrupt");
    ImGui::Separator();

    io_button<0x7>(reg, "serial interrupt");
    ImGui::Separator();

    io_button<0x8>(reg, "dma 0 interrupt");
    io_button<0x9>(reg, "dma 1 interrupt");
    io_button<0xA>(reg, "dma 2 interrupt");
    io_button<0xB>(reg, "dma 3 interrupt");
    ImGui::Separator();

    io_button<0xC>(reg, "key interrupt");
    io_button<0xD>(reg, "cassette interrupt");
}

auto io_ie(gba::Gba& gba) -> void
{
    io_ie_if(gba::mem::IO_IE, REG_IE);
}

auto io_if(gba::Gba& gba) -> void
{
    io_ie_if(gba::mem::IO_IF, REG_IF);
}

auto IO_WSCNT(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_WSCNT, REG_WSCNT);

    static const char* ws[] = { "4 cycles", "3 cycles", "2 cycles", "8 cycles" };

    io_list<0x0, 0x1>(REG_WSCNT, "SRAM", ws);
    ImGui::Separator();

    io_list<0x2, 0x3>(REG_WSCNT, "0x08000000 initial (WS0)", ws);
    ImGui::Separator();

    io_list<0x5, 0x6>(REG_WSCNT, "0x0A000000 initial (WS1)", ws);
    ImGui::Separator();

    io_list<0x8, 0x9>(REG_WSCNT, "0x0C000000 initial (WS2)", ws);
    ImGui::Separator();

    static const char* cart_clock[] = { "idk", "4 Mhz", "8 Mhz", "16 Mhz" };
    io_list<0xB, 0xC>(REG_WSCNT, "Cart Clock", cart_clock);
    ImGui::Separator();

    io_button<0xE>(REG_WSCNT, "Prefetch");
}

auto io_ime(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_IME, REG_IME);

    io_button<0x0>(REG_IME, "Master interrupt enable");
}

auto io_HALTCNT(gba::Gba& gba) -> void
{
    io_title(gba::mem::IO_HALTCNT_L, REG_HALTCNT);

    io_button<0xE>(REG_HALTCNT, "Mode");
    io_button<0xF>(REG_HALTCNT, "Power Down");
}

auto unimpl_io_view(gba::Gba& gba) -> void
{
    ImGui::Text("Unimplemented");
}

constexpr std::array IO_NAMES =
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
    IoRegEntry{ "BG2PA", IO_BG2PA },
    IoRegEntry{ "BG2PB", IO_BG2PB },
    IoRegEntry{ "BG2PC", IO_BG2PC },
    IoRegEntry{ "BG2PD", IO_BG2PD },
    IoRegEntry{ "BG2X", IO_BG2X },
    IoRegEntry{ "BG2Y", IO_BG2Y },
    IoRegEntry{ "BG3PA", IO_BG3PA },
    IoRegEntry{ "BG3PB", IO_BG3PB },
    IoRegEntry{ "BG3PC", IO_BG3PC },
    IoRegEntry{ "BG3PD", IO_BG3PD },
    IoRegEntry{ "BG3X", IO_BG3X },
    IoRegEntry{ "BG3Y", IO_BG3Y },
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
    IoRegEntry{ "SOUND1CNT_L", io_SOUND1CNT_L },
    IoRegEntry{ "SOUND1CNT_H", io_SOUND1CNT_H },
    IoRegEntry{ "SOUND1CNT_X", io_SOUND1CNT_X },
    IoRegEntry{ "SOUND2CNT_L", io_SOUND2CNT_L },
    IoRegEntry{ "SOUND2CNT_H", io_SOUND2CNT_H },
    IoRegEntry{ "SOUND3CNT_L", io_SOUND3CNT_L },
    IoRegEntry{ "SOUND3CNT_H", io_SOUND3CNT_H },
    IoRegEntry{ "SOUND3CNT_X", io_SOUND3CNT_X },
    IoRegEntry{ "SOUND4CNT_L", io_SOUND4CNT_L },
    IoRegEntry{ "SOUND4CNT_H", io_SOUND4CNT_H },
    IoRegEntry{ "SOUNDCNT_L", io_SOUNDCNT_L },
    IoRegEntry{ "SOUNDCNT_H", io_SOUNDCNT_H },
    IoRegEntry{ "SOUNDCNT_X", io_SOUNDCNT_X },
    IoRegEntry{ "SOUNDBIAS", io_SOUNDBIAS },
    IoRegEntry{ "WAVE_RAM0_L", io_WAVE_RAM0_L },
    IoRegEntry{ "WAVE_RAM0_H", io_WAVE_RAM0_H },
    IoRegEntry{ "WAVE_RAM1_L", io_WAVE_RAM1_L },
    IoRegEntry{ "WAVE_RAM1_H", io_WAVE_RAM1_H },
    IoRegEntry{ "WAVE_RAM2_L", io_WAVE_RAM2_L },
    IoRegEntry{ "WAVE_RAM2_H", io_WAVE_RAM2_H },
    IoRegEntry{ "WAVE_RAM3_L", io_WAVE_RAM3_L },
    IoRegEntry{ "WAVE_RAM3_H", io_WAVE_RAM3_H },
    IoRegEntry{ "FIFO_A_L", unimpl_io_view },
    IoRegEntry{ "FIFO_A_H", unimpl_io_view },
    IoRegEntry{ "FIFO_B_L", unimpl_io_view },
    IoRegEntry{ "FIFO_B_H", unimpl_io_view },
    IoRegEntry{ "DMA0SAD",  IO_DMA0SAD },
    IoRegEntry{ "DMA1SAD",  IO_DMA1SAD },
    IoRegEntry{ "DMA2SAD",  IO_DMA2SAD },
    IoRegEntry{ "DMA3SAD",  IO_DMA3SAD },
    IoRegEntry{ "DMA0DAD",  IO_DMA0DAD },
    IoRegEntry{ "DMA1DAD",  IO_DMA1DAD },
    IoRegEntry{ "DMA2DAD",  IO_DMA2DAD },
    IoRegEntry{ "DMA3DAD",  IO_DMA3DAD },
    IoRegEntry{ "DMA0CNT_H", IO_DMA0CNT_H },
    IoRegEntry{ "DMA1CNT_H", IO_DMA1CNT_H },
    IoRegEntry{ "DMA2CNT_H", IO_DMA2CNT_H },
    IoRegEntry{ "DMA3CNT_H", IO_DMA3CNT_H },
    IoRegEntry{ "TM0D", unimpl_io_view },
    IoRegEntry{ "TM1D", unimpl_io_view },
    IoRegEntry{ "TM2D", unimpl_io_view },
    IoRegEntry{ "TM3D", unimpl_io_view },
    IoRegEntry{ "TM0CNT", IO_TM0CNT },
    IoRegEntry{ "TM1CNT", IO_TM1CNT },
    IoRegEntry{ "TM2CNT", IO_TM2CNT },
    IoRegEntry{ "TM3CNT", IO_TM3CNT },
    IoRegEntry{ "KEY", io_key },
    IoRegEntry{ "KEYCNT", io_KEYCNT },
    IoRegEntry{ "RCNT", io_RCNT },
    IoRegEntry{ "IE", io_ie },
    IoRegEntry{ "IF", io_if },
    IoRegEntry{ "WSCNT", IO_WSCNT },
    IoRegEntry{ "IME", io_ime },
    IoRegEntry{ "HALTCNT", io_HALTCNT },
};

} // namespace

auto render_gba(gba::Gba& gba, bool* p_open) -> void
{
    // below is copied from imgui demo

    ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("io viewer", p_open))
    {
        // Left
        static std::size_t selected = 0;
        {
            ImGui::BeginChild("left pane", ImVec2(150, 0), true);
            for (std::size_t i = 0; i < IO_NAMES.size(); i++)
            {
                if (ImGui::Selectable(IO_NAMES[i].name, selected == i))
                {
                    selected = i;
                }
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

} // namespace debugger::io
