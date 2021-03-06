// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <span>
#include <type_traits>

namespace gba::mem {

struct ReadArray
{
    std::span<const u8> array;
    u32 mask;
};

struct WriteArray
{
    std::span<u8> array;
    u32 mask;
};

// everything is u8 because it makes r/w easier
// however, everything is aligned so it can be safely cast
// to the actual width.
// this is especially important for io memory.
// todo: profile having the arrays be their native bus size.
// this will remove casting in most common cases.
struct Mem
{
    ReadArray rmap_8[16]{};
    ReadArray rmap_16[16]{};
    ReadArray rmap_32[16]{};
    WriteArray wmap_8[16]{};
    WriteArray wmap_16[16]{};
    WriteArray wmap_32[16]{};

    // 256kb, 16-bit bus
    alignas(u32) u8 ewram[1024 * 256]{};

    // 32kb, 32-bit bus
    alignas(u32) u8 iwram[1024 * 32]{};

    // 1kb, 16-bit
    alignas(u32) u8 pram[1024 * 1]{};

    // 96kb, 16-bit bus
    alignas(u32) u8 vram[1024 * 96]{};

    // 1kb, 32-bit
    alignas(u64) u8 oam[1024 * 1]{};

    // 1kb, 16/32-bit
    alignas(u32) u8 io[1024 * 1]{};

    // internal memory control
    alignas(u32) u8 imc[4];
};


// General Internal Memory
constexpr auto BIOS_MASK    = 0x00003FFF;
constexpr auto EWRAM_MASK   = 0x0003FFFF;
constexpr auto IWRAM_MASK   = 0x00007FFF;
constexpr auto IO_MASK      = 0x3FF;
// Internal Display Memory
constexpr auto PRAM_MASK    = 0x000003FF;
constexpr auto VRAM_MASK    = 0x0001FFFF;
constexpr auto OAM_MASK     = 0x000003FF;
// External Memory (Game Pak)
constexpr auto ROM_MASK     = 0x01FFFFFF;

// General Internal Memory
[[maybe_unused]] constexpr auto BIOS_SIZE   = BIOS_MASK + 1;
[[maybe_unused]] constexpr auto EWRAM_SIZE  = EWRAM_MASK + 1;
[[maybe_unused]] constexpr auto IWRAM_SIZE  = IWRAM_MASK + 1;
[[maybe_unused]] constexpr auto IO_SIZE     = IO_MASK + 1;
// Internal Display Memory
[[maybe_unused]] constexpr auto PRAM_SIZE   = PRAM_MASK + 1;
[[maybe_unused]] constexpr auto VRAM_SIZE   = 0x00017FFF + 1;
[[maybe_unused]] constexpr auto OAM_SIZE    = OAM_MASK + 1;
// External Memory (Game Pak)
[[maybe_unused]] constexpr auto ROM_SIZE    = ROM_MASK + 1;


constexpr auto IO_DISPCNT = 0x04000000;
constexpr auto IO_DISPSTAT = 0x04000004;
constexpr auto IO_VCOUNT = 0x04000006;

constexpr auto IO_BG0CNT = 0x04000008;
constexpr auto IO_BG1CNT = 0x0400000A;
constexpr auto IO_BG2CNT = 0x0400000C;
constexpr auto IO_BG3CNT = 0x0400000E;

constexpr auto IO_BG0HOFS = 0x04000010; // (w)
constexpr auto IO_BG0VOFS = 0x04000012; // (w)
constexpr auto IO_BG1HOFS = 0x04000014; // (w)
constexpr auto IO_BG1VOFS = 0x04000016; // (w)
constexpr auto IO_BG2HOFS = 0x04000018; // (w)
constexpr auto IO_BG2VOFS = 0x0400001A; // (w)
constexpr auto IO_BG3HOFS = 0x0400001C; // (w)
constexpr auto IO_BG3VOFS = 0x0400001E; // (w)

constexpr auto IO_BG2PA = 0x04000020; // (w)
constexpr auto IO_BG2PB = 0x04000022; // (w)
constexpr auto IO_BG2PC = 0x04000024; // (w)
constexpr auto IO_BG2PD = 0x04000026; // (w)
constexpr auto IO_BG2X = 0x04000028; // (w)
constexpr auto IO_BG2Y = 0x0400002C; // (w)

constexpr auto IO_BG3PA = 0x04000030; // (w)
constexpr auto IO_BG3PB = 0x04000032; // (w)
constexpr auto IO_BG3PC = 0x04000034; // (w)
constexpr auto IO_BG3PD = 0x04000036; // (w)
constexpr auto IO_BG3X = 0x04000038; // (w)
constexpr auto IO_BG3Y = 0x0400003C; // (w)

constexpr auto IO_BG2X_LO = 0x04000028; // (w)
constexpr auto IO_BG2X_HI = 0x0400002A; // (w)
constexpr auto IO_BG2Y_LO = 0x0400002C; // (w)
constexpr auto IO_BG2Y_HI = 0x0400002E; // (w)
constexpr auto IO_BG3X_LO = 0x04000038; // (w)
constexpr auto IO_BG3X_HI = 0x0400003A; // (w)
constexpr auto IO_BG3Y_LO = 0x0400003C; // (w)
constexpr auto IO_BG3Y_HI = 0x0400003E; // (w)

constexpr auto IO_WIN0H = 0x4000040; // (w)
constexpr auto IO_WIN1H = 0x4000042; // (w)
constexpr auto IO_WIN0V = 0x4000044; // (w)
constexpr auto IO_WIN1V = 0x4000046; // (w)
constexpr auto IO_WININ = 0x4000048; // (r+w)
constexpr auto IO_WINOUT = 0x400004A; // (r+w)

constexpr auto IO_MOSAIC = 0x400004C; // (w)
constexpr auto IO_BLDMOD = 0x4000050; // (r+w)
constexpr auto IO_COLEV = 0x4000052; // (w)
constexpr auto IO_COLEY = 0x4000054; // (w)

constexpr auto IO_SOUND1CNT_L = 0x04000060;
constexpr auto IO_SOUND1CNT_H = 0x04000062;
constexpr auto IO_SOUND1CNT_X = 0x04000064;
constexpr auto IO_SOUND2CNT_L = 0x04000068;
constexpr auto IO_SOUND2CNT_H = 0x0400006C;
constexpr auto IO_SOUND3CNT_L = 0x04000070;
constexpr auto IO_SOUND3CNT_H = 0x04000072;
constexpr auto IO_SOUND3CNT_X = 0x04000074;
constexpr auto IO_SOUND4CNT_L = 0x04000078;
constexpr auto IO_SOUND4CNT_H = 0x0400007C;
constexpr auto IO_SOUNDCNT_L = 0x04000080;
constexpr auto IO_SOUNDCNT_H = 0x04000082;
constexpr auto IO_SOUNDCNT_X = 0x04000084;
constexpr auto IO_SOUNDBIAS = 0x04000088;
constexpr auto IO_WAVE_RAM0_L = 0x04000090;
constexpr auto IO_WAVE_RAM0_H = 0x04000092;
constexpr auto IO_WAVE_RAM1_L = 0x04000094;
constexpr auto IO_WAVE_RAM1_H = 0x04000096;
constexpr auto IO_WAVE_RAM2_L = 0x04000098;
constexpr auto IO_WAVE_RAM2_H = 0x0400009A;
constexpr auto IO_WAVE_RAM3_L = 0x0400009C;
constexpr auto IO_WAVE_RAM3_H = 0x0400009E;
constexpr auto IO_FIFO_A_L = 0x040000A0; // (w)
constexpr auto IO_FIFO_A_H = 0x040000A2; // (w)
constexpr auto IO_FIFO_B_L = 0x040000A4; // (w)
constexpr auto IO_FIFO_B_H = 0x040000A6; // (w)

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#DMA%20Source%20Registers
constexpr auto IO_DMA0SAD = 0x40000B0; // 27bit (w)
constexpr auto IO_DMA1SAD = 0x40000BC; // 28bit (w)
constexpr auto IO_DMA2SAD = 0x40000C8; // 28bit (w)
constexpr auto IO_DMA3SAD = 0x40000D4; // 28bit (w)

constexpr auto IO_DMA0DAD = 0x40000B4; // 27bit (w)
constexpr auto IO_DMA1DAD = 0x40000C0; // 27bit (w)
constexpr auto IO_DMA2DAD = 0x40000CC; // 27bit (w)
constexpr auto IO_DMA3DAD = 0x40000D8; // 28bit (w)

constexpr auto IO_DMA0SAD_LO = 0x40000B0; // (w)
constexpr auto IO_DMA0SAD_HI = 0x40000B2; // (w)
constexpr auto IO_DMA1SAD_LO = 0x40000BC; // (w)
constexpr auto IO_DMA1SAD_HI = 0x40000BE; // (w)
constexpr auto IO_DMA2SAD_LO = 0x40000C8; // (w)
constexpr auto IO_DMA2SAD_HI = 0x40000CA; // (w)
constexpr auto IO_DMA3SAD_LO = 0x40000D4; // (w)
constexpr auto IO_DMA3SAD_HI = 0x40000D6; // (w)

constexpr auto IO_DMA0DAD_LO = 0x40000B4; // (w)
constexpr auto IO_DMA0DAD_HI = 0x40000B6; // (w)
constexpr auto IO_DMA1DAD_LO = 0x40000C0; // (w)
constexpr auto IO_DMA1DAD_HI = 0x40000C2; // (w)
constexpr auto IO_DMA2DAD_LO = 0x40000CC; // (w)
constexpr auto IO_DMA2DAD_HI = 0x40000CE; // (w)
constexpr auto IO_DMA3DAD_LO = 0x40000D8; // (w)
constexpr auto IO_DMA3DAD_HI = 0x40000DA; // (w)

constexpr auto IO_DMA0CNT = 0x40000B8; // (DMA0 Count Register)
constexpr auto IO_DMA1CNT = 0x40000C4; // (DMA1 Count Register)
constexpr auto IO_DMA2CNT = 0x40000D0; // (DMA2 Count Register)
constexpr auto IO_DMA3CNT = 0x40000DC; // (DMA3 Count Register)

// same as above, but 16-bit (cowbite docs refer to them as such)
constexpr auto IO_DMA0CNT_L = 0x40000B8; // (DMA0 Count Register) (w)
constexpr auto IO_DMA1CNT_L = 0x40000C4; // (DMA1 Count Register) (w)
constexpr auto IO_DMA2CNT_L = 0x40000D0; // (DMA2 Count Register) (w)
constexpr auto IO_DMA3CNT_L = 0x40000DC; // (DMA3 Count Register) (w)

constexpr auto IO_DMA0CNT_H = 0x40000BA; // (DMA0 Control Register)
constexpr auto IO_DMA1CNT_H = 0x40000C6; // (DMA1 Control Register)
constexpr auto IO_DMA2CNT_H = 0x40000D2; // (DMA2 Control Register)
constexpr auto IO_DMA3CNT_H = 0x40000DE; // (DMA3 Control Register)

constexpr auto IO_TM0D = 0x4000100; // (Timer 0 Data)
constexpr auto IO_TM1D = 0x4000104; // (Timer 1 Data)
constexpr auto IO_TM2D = 0x4000108; // (Timer 2 Data)
constexpr auto IO_TM3D = 0x400010C; // (Timer 3 Data)

constexpr auto IO_TM0CNT = 0x4000102; // (Timer 0 Control)
constexpr auto IO_TM1CNT = 0x4000106; // (Timer 1 Control)
constexpr auto IO_TM2CNT = 0x400010A; // (Timer 2 Control)
constexpr auto IO_TM3CNT = 0x400010E; // (Timer 3 Control)

constexpr auto IO_KEY = 0x04000130; // (The input register)(Read Only)
constexpr auto IO_RCNT = 0x04000134;

constexpr auto IO_IE = 0x04000200; // (Interrupt Enable Register)
constexpr auto IO_IF = 0x04000202; // (Interrupt Flags Regster)
constexpr auto IO_WSCNT = 0x4000204; // (Wait State Control)
constexpr auto IO_IME = 0x4000208; // (Interrupt Master Enable)

constexpr auto IO_HALTCNT_L = 0x4000300; // (LFirst Boot/Debug Control)
constexpr auto IO_HALTCNT_H = 0x4000301; // (Low Power Mode Control)

constexpr auto IO_IMC_L = 0x4000800; // (Internal Memory Control)
constexpr auto IO_IMC_H = 0x4000802; // (Internal Memory Control)

#define REG_DISPCNT  *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DISPCNT & 0x3FF))
#define REG_DISPSTAT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DISPSTAT & 0x3FF))
#define REG_VCOUNT   *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_VCOUNT & 0x3FF))

#define REG_BG0CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG0CNT & 0x3FF))
#define REG_BG1CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG1CNT & 0x3FF))
#define REG_BG2CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2CNT & 0x3FF))
#define REG_BG3CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3CNT & 0x3FF))

#define REG_BG0HOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG0HOFS & 0x3FF))
#define REG_BG0VOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG0VOFS & 0x3FF))
#define REG_BG1HOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG1HOFS & 0x3FF))
#define REG_BG1VOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG1VOFS & 0x3FF))
#define REG_BG2HOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2HOFS & 0x3FF))
#define REG_BG2VOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2VOFS & 0x3FF))
#define REG_BG3HOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3HOFS & 0x3FF))
#define REG_BG3VOFS *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3VOFS & 0x3FF))

#define REG_BG2PA *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2PA & 0x3FF))
#define REG_BG2PB *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2PB & 0x3FF))
#define REG_BG2PC *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2PC & 0x3FF))
#define REG_BG2PD *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG2PD & 0x3FF))
#define REG_BG2X *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_BG2X & 0x3FF))
#define REG_BG2Y *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_BG2Y & 0x3FF))

#define REG_BG3PA *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3PA & 0x3FF))
#define REG_BG3PB *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3PB & 0x3FF))
#define REG_BG3PC *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3PC & 0x3FF))
#define REG_BG3PD *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BG3PD & 0x3FF))
#define REG_BG3X *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_BG3X & 0x3FF))
#define REG_BG3Y *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_BG3Y & 0x3FF))

#define REG_WIN0H_LO *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN0H+0) & 0x3FF))
#define REG_WIN0H_HI *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN0H+1) & 0x3FF))
#define REG_WIN1H_LO *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN1H+0) & 0x3FF))
#define REG_WIN1H_HI *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN1H+1) & 0x3FF))

#define REG_WIN0V_LO *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN0V+0) & 0x3FF))
#define REG_WIN0V_HI *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN0V+1) & 0x3FF))
#define REG_WIN1V_LO *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN1V+0) & 0x3FF))
#define REG_WIN1V_HI *reinterpret_cast<std::uint8_t*>(gba.mem.io + ((mem::IO_WIN1V+1) & 0x3FF))

#define REG_WIN0H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WIN0H & 0x3FF))
#define REG_WIN1H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WIN1H & 0x3FF))
#define REG_WIN0V *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WIN0V & 0x3FF))
#define REG_WIN1V *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WIN1V & 0x3FF))

#define REG_WININ *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WININ & 0x3FF))
#define REG_WINOUT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WINOUT & 0x3FF))

#define REG_MOSAIC *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_MOSAIC & 0x3FF))
#define REG_BLDMOD *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_BLDMOD & 0x3FF))
#define REG_COLEV *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_COLEV & 0x3FF))
#define REG_COLEY *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_COLEY & 0x3FF))

// sound registers
// http://belogic.com/gba/
#define REG_SOUND1CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND1CNT_L & 0x3FF)) /* Sound 1 Sweep control */
#define REG_SOUND1CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND1CNT_H & 0x3FF)) /* Sound 1 Lenght, wave duty and envelope control */
#define REG_SOUND1CNT_X *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND1CNT_X & 0x3FF)) /* Sound 1 Frequency, reset and loop control */
#define REG_SOUND2CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND2CNT_L & 0x3FF)) /* Sound 2 Lenght, wave duty and envelope control */
#define REG_SOUND2CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND2CNT_H & 0x3FF)) /* Sound 2 Frequency, reset and loop control */
#define REG_SOUND3CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND3CNT_L & 0x3FF)) /* Sound 3 Enable and wave ram bank control */
#define REG_SOUND3CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND3CNT_H & 0x3FF)) /* Sound 3 Sound lenght and output level control */
#define REG_SOUND3CNT_X *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND3CNT_X & 0x3FF)) /* Sound 3 Frequency, reset and loop control */
#define REG_SOUND4CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND4CNT_L & 0x3FF)) /* Sound 4 Lenght, output level and envelope control */
#define REG_SOUND4CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUND4CNT_H & 0x3FF)) /* Sound 4 Noise parameters, reset and loop control */
#define REG_SOUNDCNT_L  *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUNDCNT_L & 0x3FF)) /* Sound 1-4 Output level and Stereo control */
#define REG_SOUNDCNT_H  *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUNDCNT_H & 0x3FF)) /* Direct Sound control and Sound 1-4 output ratio */
#define REG_SOUNDCNT_X  *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUNDCNT_X & 0x3FF)) /* Master sound enable and Sound 1-4 play status */
#define REG_SOUNDBIAS   *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_SOUNDBIAS & 0x3FF)) /* Sound bias and Amplitude resolution control */
#define REG_FIFO_A_L    *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_FIFO_A_L & 0x3FF)) /* Direct Sound channel A samples 0-1 */
#define REG_FIFO_A_H    *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_FIFO_A_H & 0x3FF)) /* Direct Sound channel A samples 2-3 */
#define REG_FIFO_B_L    *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_FIFO_B_L & 0x3FF)) /* Direct Sound channel B samples 0-1 */
#define REG_FIFO_B_H    *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_FIFO_B_H & 0x3FF)) /* Direct Sound channel B samples 2-3 */

// alias using tonc's naming
#define REG_SND1SWEEP  REG_SOUND1CNT_L
#define REG_SND1CNT    REG_SOUND1CNT_H
#define REG_SND1FREQ   REG_SOUND1CNT_X
#define REG_SND2CNT    REG_SOUND2CNT_L
#define REG_SND2FREQ   REG_SOUND2CNT_H
#define REG_SND3SEL    REG_SOUND3CNT_L
#define REG_SND3CNT    REG_SOUND3CNT_H
#define REG_SND3FREQ   REG_SOUND3CNT_X
#define REG_SND4CNT    REG_SOUND4CNT_L
#define REG_SND4FREQ   REG_SOUND4CNT_H
#define REG_SNDDMGCNT  REG_SOUNDCNT_L
#define REG_SNDDSCNT   REG_SOUNDCNT_H
#define REG_SNDSTAT    REG_SOUNDCNT_X
#define REG_SNDBIAS    REG_SOUNDBIAS

#define REG_DMA0SAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA0SAD & 0x3FF))
#define REG_DMA1SAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA1SAD & 0x3FF))
#define REG_DMA2SAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA2SAD & 0x3FF))
#define REG_DMA3SAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA3SAD & 0x3FF))

#define REG_DMA0DAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA0DAD & 0x3FF))
#define REG_DMA1DAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA1DAD & 0x3FF))
#define REG_DMA2DAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA2DAD & 0x3FF))
#define REG_DMA3DAD *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA3DAD & 0x3FF))

#define REG_DMA0CNT *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA0CNT & 0x3FF))
#define REG_DMA1CNT *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA1CNT & 0x3FF))
#define REG_DMA2CNT *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA2CNT & 0x3FF))
#define REG_DMA3CNT *reinterpret_cast<std::uint32_t*>(gba.mem.io + (gba::mem::IO_DMA3CNT & 0x3FF))

#define REG_DMA0CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA0CNT_L & 0x3FF))
#define REG_DMA1CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA1CNT_L & 0x3FF))
#define REG_DMA2CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA2CNT_L & 0x3FF))
#define REG_DMA3CNT_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA3CNT_L & 0x3FF))

#define REG_DMA0CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA0CNT_H & 0x3FF))
#define REG_DMA1CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA1CNT_H & 0x3FF))
#define REG_DMA2CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA2CNT_H & 0x3FF))
#define REG_DMA3CNT_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_DMA3CNT_H & 0x3FF))

#define REG_TM0D *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM0D & 0x3FF))
#define REG_TM1D *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM1D & 0x3FF))
#define REG_TM2D *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM2D & 0x3FF))
#define REG_TM3D *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM3D & 0x3FF))

#define REG_TM0CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM0CNT & 0x3FF))
#define REG_TM1CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM1CNT & 0x3FF))
#define REG_TM2CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM2CNT & 0x3FF))
#define REG_TM3CNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_TM3CNT & 0x3FF))

#define REG_KEY *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_KEY & 0x3FF))
#define REG_RCNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_RCNT & 0x3FF))

#define REG_IE *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_IE & 0x3FF))
#define REG_IF *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_IF & 0x3FF))
#define REG_WSCNT *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_WSCNT & 0x3FF))
#define REG_IME *reinterpret_cast<std::uint8_t*>(gba.mem.io + (gba::mem::IO_IME & 0x3FF))

// currently these regs are not handled
// #define REG_IMC_L *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_IMC_L & 0x3FF))
// #define REG_IMC_H *reinterpret_cast<std::uint16_t*>(gba.mem.io + (gba::mem::IO_IMC_H & 0x3FF))

#define REG_IMC_L *reinterpret_cast<std::uint16_t*>(gba.mem.imc + 0)
#define REG_IMC_H *reinterpret_cast<std::uint16_t*>(gba.mem.imc + 2)

STATIC auto setup_tables(Gba& gba) -> void;
STATIC auto reset(Gba& gba, bool skip_bios) -> void;

[[nodiscard]]
STATIC_INLINE auto read8(Gba& gba, u32 addr) -> u8;
[[nodiscard]]
STATIC_INLINE auto read16(Gba& gba, u32 addr) -> u16;
[[nodiscard]]
STATIC_INLINE auto read32(Gba& gba, u32 addr) -> u32;

STATIC_INLINE auto write8(Gba& gba, u32 addr, u8 value) -> void;
STATIC_INLINE auto write16(Gba& gba, u32 addr, u16 value) -> void;
STATIC_INLINE auto write32(Gba& gba, u32 addr, u32 value) -> void;

template <typename T> [[nodiscard]]
constexpr auto align(u32 addr) -> u32
{
    if constexpr(std::is_same<T, u8>())
    {
        return addr;
    }
    else if constexpr(std::is_same<T, u16>())
    {
        return addr & ~0x1;
    }
    else if constexpr(std::is_same<T, u32>())
    {
        return addr & ~0x3;
    }
}

// helpers for read / write arrays.
template <typename T> [[nodiscard]]
STATIC_INLINE constexpr auto read_array(std::span<const u8> array, auto mask, u32 addr) -> T
{
    constexpr auto shift = sizeof(T) >> 1;
    return reinterpret_cast<const T*>(array.data())[(addr>>shift) & (mask>>shift)];
}

template <typename T>
STATIC_INLINE constexpr auto write_array(std::span<u8> array, auto mask, u32 addr, T v) -> void
{
    constexpr auto shift = sizeof(T) >> 1;
    reinterpret_cast<T*>(array.data())[(addr>>shift) & (mask>>shift)] = v;
}

} // namespace gba::mem
