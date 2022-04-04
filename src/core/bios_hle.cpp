// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "bit.hpp"
#include "gba.hpp"
#include "bios_hle.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "mem.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace gba::bios {

// https://problemkaputt.de/gbatek.htm#biosfunctionsummary
static constexpr const char* SWI_STR[0xFF] =
{
   /*[0x00] =*/ "SoftReset",
   /*[0x01] =*/ "RegisterRamReset",
   /*[0x02] =*/ "Halt",
   /*[0x03] =*/ "Stop",
   /*[0x04] =*/ "IntrWait",
   /*[0x05] =*/ "VBlankIntrWait",
   /*[0x06] =*/ "Div",
   /*[0x07] =*/ "DivArm",
   /*[0x08] =*/ "Sqrt",
   /*[0x09] =*/ "ArcTan",
   /*[0x0A] =*/ "ArcTan2",
   /*[0x0B] =*/ "CpuSet",
   /*[0x0C] =*/ "CpuFastSet",
   /*[0x0D] =*/ "GetBiosChecksum",
   /*[0x0E] =*/ "BgAffineSet",
   /*[0x0F] =*/ "ObjAffineSet",

   // Decompression Functions
   /*[0x10] =*/ "BitUnPack",
   /*[0x11] =*/ "LZ77UnCompReadNormalWrite8bit",
   /*[0x12] =*/ "LZ77UnCompReadNormalWrite16bit",
   /*[0x13] =*/ "HuffUnCompReadNormal",
   /*[0x14] =*/ "RLUnCompReadNormalWrite8bit",
   /*[0x15] =*/ "RLUnCompReadNormalWrite16bit",
   /*[0x16] =*/ "Diff8bitUnFilterWrite8bit",
   /*[0x17] =*/ "Diff8bitUnFilterWrite16bit",
   /*[0x18] =*/ "Diff16bitUnFilter",

   // Sound (and Multiboot/HardReset/CustomHalt)
   /*[0x19] =*/ "SoundBias",
   /*[0x1A] =*/ "SoundDriverInit",
   /*[0x1B] =*/ "SoundDriverMode",
   /*[0x1C] =*/ "SoundDriverMain",
   /*[0x1D] =*/ "SoundDriverVSync",
   /*[0x1E] =*/ "SoundChannelClear",
   /*[0x1F] =*/ "MidiKey2Freq",
   /*[0x20] =*/ "SoundWhatever0",
   /*[0x21] =*/ "SoundWhatever1",
   /*[0x22] =*/ "SoundWhatever2",
   /*[0x23] =*/ "SoundWhatever3",
   /*[0x24] =*/ "SoundWhatever4",
   /*[0x25] =*/ "MultiBoot",
   /*[0x26] =*/ "HardReset",
   /*[0x27] =*/ "CustomHalt",
   /*[0x28] =*/ "SoundDriverVSyncOff",
   /*[0x29] =*/ "SoundDriverVSyncOn",
   /*[0x2A] =*/ "SoundGetJumpList",

   // (SWI xxh..FFh do jump to garbage addresses)
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash", "Crash",
   "Crash", "Crash",
};

// https://problemkaputt.de/gbatek.htm#biosarithmeticfunctions

// 0x2
static auto Halt(Gba& gba) -> bool
{
    arm7tdmi::on_halt_trigger(gba, arm7tdmi::HaltType::hle_halt);
    return true;
}

// 0x6
static auto Div(Gba& gba) -> bool
{
    const s32 number = arm7tdmi::get_reg(gba, 0);
    const s32 denom = arm7tdmi::get_reg(gba, 1);

    if (number == 0 || denom == 0)
    {
        return false; // don't handle edge cases
    }

    const auto result = std::div(number, denom);
    const auto is_signed = result.quot < 0 ? -1 : 1;

    arm7tdmi::set_reg(gba, 0, result.quot);
    arm7tdmi::set_reg(gba, 1, result.rem);
    arm7tdmi::set_reg(gba, 3, result.quot * is_signed);

    gba_log("[DIV] number: %d denom: %d q: %d r: %d\n", number, denom, result.quot, result.rem);
    return true;
}

// 0x8
static auto Sqrt(Gba& gba) -> bool
{
    const u32 number = arm7tdmi::get_reg(gba, 0);
    const u16 result = std::sqrt(number);
    arm7tdmi::set_reg(gba, 0, result);
    return true;
}

// 0xB
auto CpuSet(Gba& gba) -> bool
{
    // this currently breaks emerald intro, likely timing
    return false;

    auto r0 = arm7tdmi::get_reg(gba, 0);
    auto r1 = arm7tdmi::get_reg(gba, 1);
    auto r2 = arm7tdmi::get_reg(gba, 2);

    auto src = r0;
    auto dst = r1;
    auto len = bit::get_range<0, 20>(r2);
    const auto mode = bit::is_set<24>(r2);
    const auto width = bit::is_set<26>(r2);

    const auto func = [&gba, &src, &dst, &len, mode](auto read, auto write, auto inc)
    {
        if (mode == 0) // copy
        {
            while (len--)
            {
                const auto data = read(gba, src);
                write(gba, dst, data);
                src += inc;
                dst += inc;
            }
        }
        else // fill
        {
            const auto data = read(gba, src);
            src += inc;

            while (len--)
            {
                write(gba, dst, data);
                dst += inc;
            }
        }
    };

    if (width == 0) // 16bit
    {
        func(mem::read16, mem::write16, 2);
    }
    else // 32bit
    {
        func(mem::read32, mem::write32, 4);
    }

    // i'd imagine the registers are written back after
    r0 = src;
    r1 = dst;
    r2 &= ~0b11111111111111111111;// unset len
    arm7tdmi::set_reg_thumb(gba, 0, r0);
    arm7tdmi::set_reg_thumb(gba, 1, r1);
    arm7tdmi::set_reg_thumb(gba, 2, r2);

    return true;
}

auto hle(Gba& gba, u8 comment_field) -> bool
{
    // return false;
    //gba_log("[SWI] comment_field: %u %s\n", comment_field, SWI_STR[comment_field]);

    switch (comment_field)
    {
        case 0x02: return Halt(gba);
        case 0x06: return Div(gba);
        case 0x08: return Sqrt(gba);
        case 0x0B: return CpuSet(gba);

        default:
            // std::printf("[BIOS-HLE] unhandled: 0x%02X %s\n", comment_field, SWI_STR[comment_field]);
            //assert(0 && "unhandled swi");
            return false;
    }
}

} // namespace gba::bios
