// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include <gameboy/gb.hpp>
#include <gameboy/internal.hpp>
#include "mem.hpp"
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
    ImGui::Text("Addr: 0x%04X Value: 0x%02X", addr, reg);
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

auto io_JYP(gba::Gba& gba) -> void
{
    io_title(0xFF00, IO_JYP);

    if (bit::is_set<4>(IO_JYP))
    {
        io_button<0x0>(IO_JYP, "Right");
        io_button<0x1>(IO_JYP, "Left");
        io_button<0x2>(IO_JYP, "Up");
        io_button<0x3>(IO_JYP, "Down");
        ImGui::Separator();
    }

    if (bit::is_set<5>(IO_JYP))
    {
        io_button<0x0>(IO_JYP, "A");
        io_button<0x1>(IO_JYP, "B");
        io_button<0x2>(IO_JYP, "Start");
        io_button<0x3>(IO_JYP, "Select");
        ImGui::Separator();
    }


    io_button<0x4>(IO_JYP, "Directional Keys");
    io_button<0x5>(IO_JYP, "Button Keys");
}

auto io_SB(gba::Gba& gba) -> void
{
    io_title(0xFF01, IO_SB);
    io_int<0x0, 0x7>(IO_SB, "Serial Data");
}

auto io_SC(gba::Gba& gba) -> void
{
    io_title(0xFF02, IO_SC);

    static const char* shift_clock_list[] = { "External", "Internal" };
    io_list<0x0, 0x0>(IO_SC, "Shift Clock", shift_clock_list);

    static const char* clock_speed_list[] = { "Normal", "Fast" };
    io_list<0x1, 0x1>(IO_SC, "Clock Speed", clock_speed_list);

    static const char* transfer_flag_list[] = { "Normal", "Fast" };
    io_list<0x7, 0x7>(IO_SC, "Transfer Start Flag", transfer_flag_list);
}

auto io_DIV(gba::Gba& gba) -> void
{
    io_title(0xFF04, IO_DIV);
    io_int<0x0, 0x7>(IO_DIV, "Divider Register");
}

auto io_TIMA(gba::Gba& gba) -> void
{
    io_title(0xFF05, IO_TIMA);
    io_int<0x0, 0x7>(IO_TIMA, "Timer Counter");
}

auto io_TMA(gba::Gba& gba) -> void
{
    io_title(0xFF06, IO_TMA);
    io_int<0x0, 0x7>(IO_TMA, "Timer Modulo");
}

auto io_TAC(gba::Gba& gba) -> void
{
    io_title(0xFF07, IO_TAC);

    static const char* clock_select_list[] = { "4096 Hz", "262144 Hz", "65536 Hz", "16384 Hz" };
    static const char* timer_stop_list[] = { "Stop", "Start" };

    io_list<0x0, 0x1>(IO_TAC, "Timer Control", clock_select_list);
    io_list<0x2, 0x2>(IO_TAC, "Timer Stop", timer_stop_list);
}

auto io_NR10(gba::Gba& gba) -> void
{
    io_title(0xFF10, REG_SOUND1CNT_L & 0xFF);

    static const char* sweep_direction_list[] = { "Increase", "Decrease" };

    io_int<0x0, 0x2>(REG_SOUND1CNT_L, "sweep shift");
    io_list<0x3, 0x3>(REG_SOUND1CNT_L, "sweep direction", sweep_direction_list);
    io_int<0x4, 0x6>(REG_SOUND1CNT_L, "sweep time");
}

auto io_NRX1(auto addr, auto& reg) -> void
{
    io_title(addr, reg & 0xFF);

    static const char* wave_pattern_duty_list[] = { "12.5%", "25%", "50%", "75%" };

    io_int<0x0, 0x5>(reg, "Sound Length");
    io_list<0x6, 0x7>(reg, "Wave Pattern Duty", wave_pattern_duty_list);
}

auto io_NRX2(auto addr, auto& reg) -> void
{
    io_title(addr, reg >> 8);

    static const char* envelope_direction_list[] = { "Decrease", "Increase" };

    io_int<0x8, 0xA>(reg, "Envelope Step Time");
    io_list<0xB, 0xB>(reg, "Envelope Direction", envelope_direction_list);
    io_int<0xC, 0xF>(reg, "Initial Volume");
}

auto io_NRX3(auto addr, auto& reg) -> void
{
    io_title(addr, reg & 0xFF);
    io_int<0x0, 0x7>(reg, "Freq Lower 8bits");
}

auto io_NRX4(auto addr, auto& reg) -> void
{
    io_title(addr, reg >> 8);

    io_int<0x8, 0xA>(reg, "Freq Upper 3bits");
    io_button<0xE>(reg, "Length Enable Flag");
    io_button<0xF>(reg, "trigger");
}

auto io_NR11(gba::Gba& gba) -> void
{
    io_NRX1(0xFF11, REG_SOUND1CNT_H);
}

auto io_NR12(gba::Gba& gba) -> void
{
    io_NRX2(0xFF12, REG_SOUND1CNT_H);
}

auto io_NR13(gba::Gba& gba) -> void
{
    io_NRX3(0xFF13, REG_SOUND1CNT_X);
}

auto io_NR14(gba::Gba& gba) -> void
{
    io_NRX4(0xFF14, REG_SOUND1CNT_X);
}

auto io_NR21(gba::Gba& gba) -> void
{
    io_NRX1(0xFF21, REG_SOUND2CNT_L);
}

auto io_NR22(gba::Gba& gba) -> void
{
    io_NRX2(0xFF22, REG_SOUND2CNT_L);
}

auto io_NR23(gba::Gba& gba) -> void
{
    io_NRX3(0xFF23, REG_SOUND2CNT_H);
}

auto io_NR24(gba::Gba& gba) -> void
{
    io_NRX4(0xFF24, REG_SOUND2CNT_H);
}

auto io_NR30(gba::Gba& gba) -> void
{
    io_title(0xFF30, REG_SOUND3CNT_L & 0xFF);

    static const char* bank_mode_list[] = { "1 bank (32 entries)", "2 banks (64 entries)" };

    io_list<0x5, 0x5>(REG_SOUND3CNT_L, "Wave Bank Mode", bank_mode_list);
    io_int<0x6, 0x6>(REG_SOUND3CNT_L, "Wave Bank Number");
    io_button<0x7>(REG_SOUND3CNT_L, "Dac Power");
}

auto io_NR31(gba::Gba& gba) -> void
{
    io_title(0xFF31, REG_SOUND3CNT_H & 0xFF);
    io_int<0x0, 0x7>(REG_SOUND3CNT_H, "Sound Length");
}

auto io_NR32(gba::Gba& gba) -> void
{
    io_title(0xFF32, REG_SOUND3CNT_H >> 8);

    static const char* sound_volume_table[] = { "0%", "100%", "50%", "25%" };
    static const char* force_volume_table[] = { "Off", "Forced 75%", };

    io_list<0xD, 0xE>(REG_SOUND3CNT_H, "Sound Volume", sound_volume_table);
    io_list<0xF, 0xF>(REG_SOUND3CNT_H, "Force Volume", force_volume_table);
}

auto io_NR33(gba::Gba& gba) -> void
{
    io_NRX3(0xFF33, REG_SOUND3CNT_X);
}

auto io_NR34(gba::Gba& gba) -> void
{
    io_NRX4(0xFF34, REG_SOUND3CNT_X);
}

auto io_WAVE_TABLE(gba::Gba& gba) -> void
{
    // io_title(0xFF00, IO_WAVE_TABLE);
}

auto io_NR41(gba::Gba& gba) -> void
{
    io_title(0xFF41, REG_SOUND4CNT_L & 0xFF);
    io_int<0x0, 0x5>(REG_SOUND4CNT_L, "Sound Length");
}

auto io_NR42(gba::Gba& gba) -> void
{
    io_NRX2(0xFF42, REG_SOUND4CNT_L);
}

auto io_NR43(gba::Gba& gba) -> void
{
    io_title(0xFF43, REG_SOUND4CNT_H & 0xFF);

    static const char* counter_width_list[] = { "15-bits", "7-bits" };

    io_int<0x0, 0x2>(REG_SOUND4CNT_H, "Dividing Retio of Freq");
    io_list<0x3, 0x3>(REG_SOUND4CNT_H, "Counter Width", counter_width_list);
    io_int<0x4, 0x7>(REG_SOUND4CNT_H, "Shift Clock Freq");
}

auto io_NR44(gba::Gba& gba) -> void
{
    io_title(0xFF44, REG_SOUND4CNT_H >> 8);

    io_button<0xE>(REG_SOUND4CNT_H, "Length Enable Flag");
    io_button<0xF>(REG_SOUND4CNT_H, "trigger");
}

auto io_NR50(gba::Gba& gba) -> void
{
    io_title(0xFF24, REG_SOUNDCNT_L & 0xFF);

    io_int<0x0, 0x2>(REG_SOUNDCNT_L, "Master Vol Right");
    io_int<0x4, 0x6>(REG_SOUNDCNT_L, "Master Vol Left");
}

auto io_NR51(gba::Gba& gba) -> void
{
    io_title(0xFF25, REG_SOUNDCNT_L >> 8);

    io_button<0x8>(REG_SOUNDCNT_L, "Sound 1 Right Enable");
    io_button<0x9>(REG_SOUNDCNT_L, "Sound 2 Right Enable");
    io_button<0xA>(REG_SOUNDCNT_L, "Sound 3 Right Enable");
    io_button<0xB>(REG_SOUNDCNT_L, "Sound 4 Right Enable");
    ImGui::Separator();

    io_button<0xC>(REG_SOUNDCNT_L, "Sound 1 Left Enable");
    io_button<0xD>(REG_SOUNDCNT_L, "Sound 2 Left Enable");
    io_button<0xE>(REG_SOUNDCNT_L, "Sound 3 Left Enable");
    io_button<0xF>(REG_SOUNDCNT_L, "Sound 4 Left Enable");
}

auto io_NR52(gba::Gba& gba) -> void
{
    io_title(0xFF26, REG_SOUNDCNT_X & 0xFF);

    io_button<0x0>(REG_SOUNDCNT_X, "Sound 1 ON");
    io_button<0x1>(REG_SOUNDCNT_X, "Sound 2 ON");
    io_button<0x2>(REG_SOUNDCNT_X, "Sound 3 ON");
    io_button<0x3>(REG_SOUNDCNT_X, "Sound 4 ON");
    ImGui::Separator();

    io_button<0x7>(REG_SOUNDCNT_X, "Master Enable");
}

auto io_LCDC(gba::Gba& gba) -> void
{
    io_title(0xFF40, IO_LCDC);

    static const char* map_list[] = { "0x9800-0x9BFF", "0x9C00-0x9FFF" };
    static const char* data_list[] = { "0x8800-0x97FF", "0x8000-0x8FFF" };
    static const char* obj_size_list[] = { "8x8", "8x16" };

    io_button<0x0>(IO_LCDC, "BG Display");
    io_button<0x1>(IO_LCDC, "OBJ (Sprite) Display Enable");
    io_list<0x2, 0x2>(IO_LCDC, "OBJ (Sprite) Size", obj_size_list);
    io_list<0x3, 0x3>(IO_LCDC, "BG Tile Map Display Select", map_list);
    io_list<0x4, 0x4>(IO_LCDC, "BG & Window Tile Data Select", data_list);
    io_button<0x5>(IO_LCDC, "Window Display Enable");
    io_list<0x6, 0x6>(IO_LCDC, "Window Tile Map Display Select", map_list);
    io_button<0x7>(IO_LCDC, "LCD Display Enable");
}

auto io_STAT(gba::Gba& gba) -> void
{
    io_title(0xFF41, IO_STAT);

    static const char* modes[] = { "Hblank", "Vblank", "Oam", "Transfer" };
    io_list<0x0, 0x1>(IO_STAT, "Mode", modes);
    ImGui::Separator();

    io_button<0x2>(IO_STAT, "Coincidence Flag");
    io_button<0x3>(IO_STAT, "Mode 0 H-Blank Interrupt");
    io_button<0x4>(IO_STAT, "Mode 1 V-Blank Interrupt");
    io_button<0x5>(IO_STAT, "Mode 2 OAM Interrupt");
    io_button<0x6>(IO_STAT, "LYC=LY Coincidence Interrupt");
}

auto io_SCY(gba::Gba& gba) -> void
{
    io_title(0xFF42, IO_SCY);
    io_int<0x0, 0x7>(IO_SCY, "Scroll Y");
}

auto io_SCX(gba::Gba& gba) -> void
{
    io_title(0xFF43, IO_SCX);
    io_int<0x0, 0x7>(IO_SCX, "Scroll X");
}

auto io_LY(gba::Gba& gba) -> void
{
    io_title(0xFF44, IO_LY);
    io_int<0x0, 0x7>(IO_LY, "Y coordinate");
}

auto io_LYC(gba::Gba& gba) -> void
{
    io_title(0xFF45, IO_LYC);
    io_int<0x0, 0x7>(IO_LYC, "LY compare");
}

auto io_DMA(gba::Gba& gba) -> void
{
    io_title(0xFF46, IO_DMA);
}

auto io_BGP(gba::Gba& gba) -> void
{
    io_title(0xFF47, IO_BGP);

    static const char* shades[] = { "White", "Light gray", "Dark gray", "Black" };
    io_list<0x0, 0x1>(IO_BGP, "Colour 0", shades);
    io_list<0x2, 0x3>(IO_BGP, "Colour 1", shades);
    io_list<0x4, 0x5>(IO_BGP, "Colour 2", shades);
    io_list<0x6, 0x7>(IO_BGP, "Colour 3", shades);
}

auto io_OBP0(gba::Gba& gba) -> void
{
    io_title(0xFF48, IO_OBP0);

    static const char* shades[] = { "White", "Light gray", "Dark gray", "Black" };
    io_list<0x2, 0x3>(IO_OBP0, "Colour 1", shades);
    io_list<0x4, 0x5>(IO_OBP0, "Colour 2", shades);
    io_list<0x6, 0x7>(IO_OBP0, "Colour 3", shades);
}

auto io_OBP1(gba::Gba& gba) -> void
{
    io_title(0xFF49, IO_OBP1);

    static const char* shades[] = { "White", "Light gray", "Dark gray", "Black" };
    io_list<0x2, 0x3>(IO_OBP1, "Colour 1", shades);
    io_list<0x4, 0x5>(IO_OBP1, "Colour 2", shades);
    io_list<0x6, 0x7>(IO_OBP1, "Colour 3", shades);
}

auto io_WY(gba::Gba& gba) -> void
{
    io_title(0xFF4A, IO_WY);
    io_int<0x0, 0x7>(IO_WY, "Win Y");
}

auto io_WX(gba::Gba& gba) -> void
{
    io_title(0xFF4B, IO_WX);
    io_int<0x0, 0x7>(IO_WX, "Win X");
}

auto io_VBK(gba::Gba& gba) -> void
{
    io_title(0xFF4F, IO_VBK);
    io_int<0x0, 0x0>(IO_VBK, "vram bank");
}

auto io_HDMA1(gba::Gba& gba) -> void
{
    io_title(0xFF51, IO_HDMA1);
    io_int<0x0, 0x7>(IO_HDMA1, "Src Addr Upper");
}

auto io_HDMA2(gba::Gba& gba) -> void
{
    io_title(0xFF52, IO_HDMA2);
    io_int<0x3, 0x7>(IO_HDMA2, "Src Addr Lower");
}

auto io_HDMA3(gba::Gba& gba) -> void
{
    io_title(0xFF53, IO_HDMA3);
    io_int<0x0, 0x4>(IO_HDMA3, "Dst Addr Upper");
}

auto io_HDMA4(gba::Gba& gba) -> void
{
    io_title(0xFF54, IO_HDMA4);
    io_int<0x3, 0x7>(IO_HDMA4, "Dst Addr Lower");
}

auto io_HDMA5(gba::Gba& gba) -> void
{
    io_title(0xFF55, IO_HDMA5);

    static const char* mode_list[] = { "GDMA", "HDMA" };

    io_int<0x0, 0x6>(IO_HDMA5, "Length");
    io_list<0x7, 0x7>(IO_HDMA5, "Mode", mode_list);
    ImGui::Separator();

    const auto src = (IO_HDMA1 << 8) | IO_HDMA2;
    const auto dst = (IO_HDMA3 << 8) | IO_HDMA4 | 0x8000;

    ImGui::Text("Enabled: %u\n", gba.gameboy.ppu.hdma_length > 0);
    ImGui::Text("Length: %u\n", gba.gameboy.ppu.hdma_length);
    ImGui::Text("Src: 0x%04X Region: %s\n", src, gba::gb::get_name_of_region_str(src));
    ImGui::Text("Dst: 0x%04X Region: %s\n", dst, gba::gb::get_name_of_region_str(dst));
}

auto io_RP(gba::Gba& gba) -> void
{
    io_title(0xFF56, IO_RP);

    static const char* write_data_list[] = { "LED Off", "LED On" };
    static const char* read_data_list[] = { "Recive IR Signal", "Normal" };
    static const char* data_read_enable_list[] = { "Disable", "Disable", "Disable", "Enable" };

    io_list<0x0, 0x0>(IO_RP, "Write Data", write_data_list);
    io_list<0x1, 0x1>(IO_RP, "Read Data", read_data_list);
    io_list<0x6, 0x7>(IO_RP, "Data Read Enable", data_read_enable_list);
}

auto io_BCPS(gba::Gba& gba) -> void
{
    io_title(0xFF68, IO_BCPS);

    io_int<0x0, 0x5>(IO_BCPS, "Bg Palete Index");
    io_button<0x7>(IO_BCPS, "Auto Increment");
}

auto io_BCPD(gba::Gba& gba) -> void
{
    io_title(0xFF69, IO_BCPD);

    // io_int<0x0, 0x4>(IO_BCPD, "Red");
    // io_int<0x5, 0x9>(IO_BCPD, "Green");
    // io_int<0xA, 0xE>(IO_BCPD, "Blue");
}

auto io_OCPS(gba::Gba& gba) -> void
{
    io_title(0xFF6A, IO_OCPS);

    io_int<0x0, 0x5>(IO_OCPS, "Bg Palete Index");
    io_button<0x7>(IO_OCPS, "Auto Increment");
}

auto io_OCPD(gba::Gba& gba) -> void
{
    io_title(0xFF6B, IO_OCPD);

    // io_int<0x0, 0x4>(IO_OCPD, "Red");
    // io_int<0x5, 0x9>(IO_OCPD, "Green");
    // io_int<0xA, 0xE>(IO_OCPD, "Blue");
}

auto io_OPRI(gba::Gba& gba) -> void
{
    io_title(0xFF6C, IO_OPRI);
}

auto io_SVBK(gba::Gba& gba) -> void
{
    io_title(0xFF70, IO_SVBK);
    io_int<0x0, 0x2>(IO_SVBK, "WRAM bank");
}

auto io_KEY1(gba::Gba& gba) -> void
{
    io_title(0xFF4D, IO_KEY1);

    static const char* current_speed_list[] = { "Normal", "Double" };

    io_button<0x0>(IO_KEY1, "Prepare Speed Switch");
    io_list<0x7, 0x7>(IO_KEY1, "Current Speed", current_speed_list);
}

auto io_BOOTROM(gba::Gba& gba) -> void
{
    io_title(0xFF50, IO_BOOTROM);
}

auto io_IF(gba::Gba& gba) -> void
{
    io_title(0xFF0F, GB_IO_IF);

    io_button<0x0>(GB_IO_IF, "Vblank");
    io_button<0x1>(GB_IO_IF, "Stat");
    io_button<0x2>(GB_IO_IF, "Timer");
    io_button<0x3>(GB_IO_IF, "Serial");
    io_button<0x4>(GB_IO_IF, "Joypad");
}

auto io_IE(gba::Gba& gba) -> void
{
    io_title(0xFF7F, GB_IO_IE);

    io_button<0x0>(GB_IO_IE, "Vblank");
    io_button<0x1>(GB_IO_IE, "Stat");
    io_button<0x2>(GB_IO_IE, "Timer");
    io_button<0x3>(GB_IO_IE, "Serial");
    io_button<0x4>(GB_IO_IE, "Joypad");
}

auto io_72(gba::Gba& gba) -> void
{
    io_title(0xFF72, IO_72);
    io_int<0x0, 0x7>(IO_72, "unknown");
}

auto io_73(gba::Gba& gba) -> void
{
    io_title(0xFF73, IO_73);
    io_int<0x0, 0x7>(IO_73, "unknown");
}

auto io_74(gba::Gba& gba) -> void
{
    io_title(0xFF74, IO_74);
    io_int<0x0, 0x7>(IO_74, "unknown");
}

auto io_75(gba::Gba& gba) -> void
{
    io_title(0xFF75, IO_75);
    io_int<0x0, 0x7>(IO_75, "unknown");
}

auto io_76(gba::Gba& gba) -> void
{
    io_title(0xFF76, IO_76);
    io_int<0x0, 0x7>(IO_76, "unknown");
}

auto io_77(gba::Gba& gba) -> void
{
    io_title(0xFF77, IO_77);
    io_int<0x0, 0x7>(IO_77, "unknown");
}

auto unimpl_io_view(gba::Gba& gba) -> void
{
    ImGui::Text("Unimplemented");
}

const std::array IO_NAMES =
{
    IoRegEntry{ "JYP", io_JYP },
    IoRegEntry{ "SB", io_SB },
    IoRegEntry{ "SC", io_SC },
    IoRegEntry{ "DIV", io_DIV },
    IoRegEntry{ "TIMA", io_TIMA },
    IoRegEntry{ "TMA", io_TMA },
    IoRegEntry{ "TAC", io_TAC },
    IoRegEntry{ "NR10", io_NR10 },
    IoRegEntry{ "NR11", io_NR11 },
    IoRegEntry{ "NR12", io_NR12 },
    IoRegEntry{ "NR13", io_NR13 },
    IoRegEntry{ "NR14", io_NR14 },
    IoRegEntry{ "NR21", io_NR21 },
    IoRegEntry{ "NR22", io_NR22 },
    IoRegEntry{ "NR23", io_NR23 },
    IoRegEntry{ "NR24", io_NR24 },
    IoRegEntry{ "NR30", io_NR30 },
    IoRegEntry{ "NR31", io_NR31 },
    IoRegEntry{ "NR32", io_NR32 },
    IoRegEntry{ "NR33", io_NR33 },
    IoRegEntry{ "NR34", io_NR34 },
    IoRegEntry{ "WAVE_TABLE", unimpl_io_view },
    IoRegEntry{ "NR41", io_NR41 },
    IoRegEntry{ "NR42", io_NR42 },
    IoRegEntry{ "NR43", io_NR43 },
    IoRegEntry{ "NR44", io_NR44 },
    IoRegEntry{ "NR50", io_NR50 },
    IoRegEntry{ "NR51", io_NR51 },
    IoRegEntry{ "NR52", io_NR52 },
    IoRegEntry{ "LCDC", io_LCDC },
    IoRegEntry{ "STAT", io_STAT },
    IoRegEntry{ "SCY", io_SCY },
    IoRegEntry{ "SCX", io_SCX },
    IoRegEntry{ "LY", io_LY },
    IoRegEntry{ "LYC", io_LYC },
    IoRegEntry{ "DMA", io_DMA },
    IoRegEntry{ "BGP", io_BGP },
    IoRegEntry{ "OBP0", io_OBP0 },
    IoRegEntry{ "OBP1", io_OBP1 },
    IoRegEntry{ "WY", io_WY },
    IoRegEntry{ "WX", io_WX },
    IoRegEntry{ "VBK", io_VBK },
    IoRegEntry{ "HDMA1", io_HDMA1 },
    IoRegEntry{ "HDMA2", io_HDMA2 },
    IoRegEntry{ "HDMA3", io_HDMA3 },
    IoRegEntry{ "HDMA4", io_HDMA4 },
    IoRegEntry{ "HDMA5", io_HDMA5 },
    IoRegEntry{ "RP", io_RP },
    IoRegEntry{ "BCPS", io_BCPS },
    IoRegEntry{ "BCPD", unimpl_io_view },
    IoRegEntry{ "OCPS", io_OCPS },
    IoRegEntry{ "OCPD", unimpl_io_view },
    IoRegEntry{ "OPRI", unimpl_io_view },
    IoRegEntry{ "SVBK", io_SVBK },
    IoRegEntry{ "KEY1", io_KEY1 },
    IoRegEntry{ "BOOTROM", unimpl_io_view },
    IoRegEntry{ "IF", io_IF },
    IoRegEntry{ "IE", io_IE },
    IoRegEntry{ "72", io_72 },
    IoRegEntry{ "73", io_73 },
    IoRegEntry{ "74", io_74 },
    IoRegEntry{ "75", io_75 },
    IoRegEntry{ "76", io_76 },
    IoRegEntry{ "77", io_77 },
};

} // namespace

auto render_gb(gba::Gba& gba, bool* p_open) -> void
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
