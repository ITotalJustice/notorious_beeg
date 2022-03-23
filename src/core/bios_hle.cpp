// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "gba.hpp"
#include "bios_hle.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include <cstdlib>
#include <cmath>

namespace gba::bios {

// https://problemkaputt.de/gbatek.htm#biosfunctionsummary
//constexpr const char* SWI_STR[0xFF] =
//{
//    [0x00] = "SoftReset",  //
//    [0x01] = "RegisterRamReset",   //
//    [0x02] = "Halt",   //
//    [0x03] = "Stop",   // Sleep
//    [0x04] = "IntrWait",   //
//    [0x05] = "VBlankIntrWait", //
//    [0x06] = "Div",    //
//    [0x07] = "DivArm", //
//    [0x08] = "Sqrt",   //
//    [0x09] = "ArcTan", //
//    [0x0A] = "ArcTan2",    //
//    [0x0B] = "CpuSet", //
//    [0x0C] = "CpuFastSet", //
//    [0x0D] = "GetBiosChecksum",    //
//    [0x0E] = "BgAffineSet",    //
//    [0x0F] = "ObjAffineSet",   //
//
//    // Decompression Functions
//    [0x10] = "BitUnPack",  //
//    [0x11] = "LZ77UnCompReadNormalWrite8bit",  //   ;"Wram"
//    [0x12] = "LZ77UnCompReadNormalWrite16bit", //  ;"Vram"
//    [0x13] = "HuffUnCompReadNormal",   //
//    [0x14] = "RLUnCompReadNormalWrite8bit",    //     ;"Wram"
//    [0x15] = "RLUnCompReadNormalWrite16bit",   //    ;"Vram"
//    [0x16] = "Diff8bitUnFilterWrite8bit",  //       ;"Wram"
//    [0x17] = "Diff8bitUnFilterWrite16bit", //      ;"Vram"
//    [0x18] = "Diff16bitUnFilter",  //
//
//    // Sound (and Multiboot/HardReset/CustomHalt)
//    [0x19] = "SoundBias",  //
//    [0x1A] = "SoundDriverInit",    //
//    [0x1B] = "SoundDriverMode",    //
//    [0x1C] = "SoundDriverMain",    //
//    [0x1D] = "SoundDriverVSync",   //
//    [0x1E] = "SoundChannelClear",  //
//    [0x1F] = "MidiKey2Freq",   //
//    [0x20] = "SoundWhatever0", //
//    [0x21] = "SoundWhatever1", //
//    [0x22] = "SoundWhatever2", //
//    [0x23] = "SoundWhatever3", //
//    [0x24] = "SoundWhatever4", //
//    [0x25] = "MultiBoot",  //
//    [0x26] = "HardReset",  //
//    [0x27] = "CustomHalt", //
//    [0x28] = "SoundDriverVSyncOff",    //
//    [0x29] = "SoundDriverVSyncOn", //
//    [0x2A] = "SoundGetJumpList",   //
//    [0x2B] = "Crash",  // (SWI xxh..FFh do jump to garbage addresses)
//};

// https://problemkaputt.de/gbatek.htm#biosarithmeticfunctions

// 0x2
auto Halt(Gba& gba) -> void
{
    // printf("halt swi\n");
    CPU.halted = true;
    arm7tdmi::on_halt_event(gba);
}

// 0x6
auto Div(Gba& gba) -> void
{
    const std::int32_t number = arm7tdmi::get_reg(gba, 0);
    const std::int32_t denom = arm7tdmi::get_reg(gba, 1);

    const auto result = std::div(number, denom);
    const auto is_signed = result.quot < 0 ? -1 : 1;

    arm7tdmi::set_reg(gba, 0, result.quot);
    arm7tdmi::set_reg(gba, 1, result.rem);
    arm7tdmi::set_reg(gba, 3, result.quot * is_signed);

    gba_log("[DIV] number: %d denom: %d q: %d r: %d\n", number, denom, result.quot, result.rem);
}

// 0x8
auto Sqrt(Gba& gba) -> void
{
    const std::uint32_t number = arm7tdmi::get_reg(gba, 0);
    const std::uint16_t result = std::sqrt(number);
    arm7tdmi::set_reg(gba, 0, result);
}

#if 1
int swi_Div(u32 a, u32 b)
{
    const std::int32_t number = a;
    const std::int32_t denom = b;

    const auto result = std::div(number, denom);
    const auto is_signed = result.quot < 0 ? -1 : 1;

    return  result.quot * is_signed;
}

// 0x9
u32 swi_ArcTan(u32 input)
{
    return std::atan(input);
    s32 a = -(((s32)(input * input)) >> 14);
    s32 b = ((0xA9 * a) >> 14) + 0x390;
    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;
    a = ((s32)input * b) >> 16;
    return (u32)a;
}

u32 swi_ArcTan2(s32 x, s32 y)
{
    u32 res = 0;
    if (y == 0) {
        res = ((x >> 16) & 0x8000);
    }
    else {
        if (x == 0) {
            res = ((y >> 16) & 0x8000) + 0x4000;
        }
        else {
            if ((abs(x) > abs(y)) || ((abs(x) == abs(y)) && (!((x < 0) && (y < 0))))) {
                u32 div = swi_Div(y << 14, (u32)x);
                div = swi_ArcTan(div);
                if (x < 0)
                    res = 0x8000 + div;
                else
                    res = (((y >> 16) & 0x8000) << 1) + div;
            }
            else {
                u32 div = swi_Div(x << 14, (u32)y);
                div = swi_ArcTan(div);
                res = (0x4000 + ((y >> 16) & 0x8000)) - div;
            }
        }
    }
    return res;
}

auto ArcTan(Gba& gba) -> void
{
    arm7tdmi::set_reg(gba, 0, swi_ArcTan(arm7tdmi::get_reg(gba, 0)));
    return;
    const std::uint16_t number = arm7tdmi::get_reg(gba, 0);
    const std::uint16_t result = std::atan(number);
    arm7tdmi::set_reg(gba, 0, result);
}

// 0xA
auto ArcTan2(Gba& gba) -> void
{
    const std::uint32_t x = arm7tdmi::get_reg(gba, 0);
    const std::uint32_t y = arm7tdmi::get_reg(gba, 1);
    const std::uint32_t result = swi_ArcTan2(x, y);
    std::printf("hlw result: %u stl result: %u\n", result, (uint32_t)std::atan2((int16_t)y, (int16_t)x));
    arm7tdmi::set_reg(gba, 0, result);
}
#else
auto ArcTan(Gba& gba) -> void
{
    const std::uint16_t number = arm7tdmi::get_reg(gba, 0);
    const std::uint16_t result = std::atan(number);
    arm7tdmi::set_reg(gba, 0, result);
}

// 0xA
auto ArcTan2(Gba& gba) -> void
{
    const std::uint32_t x = arm7tdmi::get_reg(gba, 0);
    const std::uint32_t y = arm7tdmi::get_reg(gba, 1);
    const std::uint32_t result = std::atan2(y, x);
    std::printf("hlw result: %u stl result: %u\n", result, (uint32_t)std::atan2((int16_t)y, (int16_t)x));
    arm7tdmi::set_reg(gba, 0, result);
}
#endif

// auto Halt(Gba& gba) -> void
// {

// }

// auto VBlankIntrWait(Gba& gba) -> void
// {

// }

auto hle(Gba& gba, std::uint8_t comment_field) -> bool
{
    //std::printf("[SWI] comment_field: %u %s\n", comment_field, SWI_STR[comment_field]);

    switch (comment_field)
    {
        case 0x02: Halt(gba); return true;

        // case 0x04: std::printf("[BIOS-HLE] IntrWait\n"); return false;
        // case 0x05: std::printf("[BIOS-HLE] VBlankIntrWait\n"); return false;

        // case 0x06: std::printf("[BIOS-HLE] Div\n"); Div(gba); return true;
        // case 0x08: std::printf("[BIOS-HLE] Sqrt\n"); Sqrt(gba); return true;
        // case 0x09: std::printf("[BIOS-HLE] ArcTan\n"); ArcTan(gba); return true;
        // case 0x0A: std::printf("[BIOS-HLE] ArcTan2\n"); ArcTan2(gba); return true;

        default:
            gba_log("unhandled hle: 0x%02X\n", comment_field);
            //arm7tdmi::toggle_breakpoint(gba, true);
            //assert(0 && "unhandled swi");
            return false;
    }
}

} // namespace gba::bios
