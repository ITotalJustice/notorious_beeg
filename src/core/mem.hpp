// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"
#include <cstdint>

namespace gba::mem {

// everything is u8 because it makes r/w easier
// however, everything is aligned so it can be safely cast
// to the actual width.
// this is especially important for io memory
struct Mem
{
    // 16kb, 32-bus
    alignas(uint32_t) uint8_t bios[1024 * 16];

    // 256kb, 16-bit bus
    alignas(uint32_t) uint8_t ewram[1024 * 256];

    // 32kb, 32-bit bus
    alignas(uint32_t) uint8_t iwram[1024 * 32];

    // 1kb, 16-bit
    alignas(uint32_t) uint8_t palette_ram[1024 * 1];

    // 96kb, 16-bit bus
    alignas(uint32_t) uint8_t vram[1024 * 96];

    // 1kb, 32-bit
    alignas(uint32_t) uint8_t oam[1024 * 1];

    // IO regs
    alignas(uint32_t) uint8_t io[0x400];
};

#define EWRAM_16   reinterpret_cast<uint16_t*>(gba.mem.ewram)
#define IWRAM_32   reinterpret_cast<uint32_t*>(gba.mem.iwram)
#define IO_16      reinterpret_cast<uint16_t*>(gba.mem.io)
#define PALETTE_16 reinterpret_cast<uint16_t*>(gba.mem.palette_ram)
#define VRAM_16    reinterpret_cast<uint16_t*>(gba.mem.vram)
#define OAM_32     reinterpret_cast<uint32_t*>(gba.mem.oam)
#define ROM_16     reinterpret_cast<uint16_t*>(gba.mem.rom)

constexpr auto IO_DISPCNT = 0x04000000;
constexpr auto IO_DISPSTAT = 0x04000004;
constexpr auto IO_VCOUNT = 0x04000006;

constexpr auto IO_BG0CNT = 0x04000008;
constexpr auto IO_BG1CNT = 0x0400000A;
constexpr auto IO_BG2CNT = 0x0400000C;
constexpr auto IO_BG3CNT = 0x0400000E;

constexpr auto IO_BG0HOFS = 0x04000010;
constexpr auto IO_BG0VOFS = 0x04000012;
constexpr auto IO_BG1HOFS = 0x04000014;
constexpr auto IO_BG1VOFS = 0x04000016;
constexpr auto IO_BG2HOFS = 0x04000018;
constexpr auto IO_BG2VOFS = 0x0400001A;
constexpr auto IO_BG3HOFS = 0x0400001C;
constexpr auto IO_BG3VOFS = 0x0400001E;

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
constexpr auto IO_FIFO_A_L = 0x040000A0;
constexpr auto IO_FIFO_A_H = 0x040000A2;
constexpr auto IO_FIFO_B_L = 0x040000A4;
constexpr auto IO_FIFO_B_H = 0x040000A6;

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#DMA%20Source%20Registers
constexpr auto IO_DMA0SAD = 0x40000B0; // 27bit
constexpr auto IO_DMA1SAD = 0x40000BC; // 28bit
constexpr auto IO_DMA2SAD = 0x40000C8; // 28bit
constexpr auto IO_DMA3SAD = 0x40000D4; // 28bit

constexpr auto IO_DMA0DAD = 0x40000B4; // 27bit
constexpr auto IO_DMA1DAD = 0x40000C0; // 27bit
constexpr auto IO_DMA2DAD = 0x40000CC; // 27bit
constexpr auto IO_DMA3DAD = 0x40000D8; // 28bit

constexpr auto IO_DMA0CNT = 0x40000B8; // (DMA0 Count Register)
constexpr auto IO_DMA1CNT = 0x40000C4; // (DMA1 Count Register)
constexpr auto IO_DMA2CNT = 0x40000D0; // (DMA2 Count Register)
constexpr auto IO_DMA3CNT = 0x40000DC; // (DMA3 Count Register)

// same as above, but 16-bit (cowbite docs refer to them as such)
constexpr auto IO_DMA0CNT_L = 0x40000B8; // (DMA0 Count Register)
constexpr auto IO_DMA1CNT_L = 0x40000C4; // (DMA1 Count Register)
constexpr auto IO_DMA2CNT_L = 0x40000D0; // (DMA2 Count Register)
constexpr auto IO_DMA3CNT_L = 0x40000DC; // (DMA3 Count Register)

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
constexpr auto IO_IE = 0x04000200; // (Interrupt Enable Register)
constexpr auto IO_IF = 0x04000202; // (Interrupt Flags Regster)
constexpr auto IO_IME = 0x4000208; // (Interrupt Master Enable)

constexpr auto REG_HALTCNT_L = 0x4000300; // (First Boot/Debug Control)
constexpr auto IO_HALTCNT_L = 0x4000300; // (Low Power Mode Control)
constexpr auto IO_HALTCNT_H = 0x4000301; // (Low Power Mode Control)

#define REG_DISPCNT  *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DISPCNT & 0x3FF))
#define REG_DISPSTAT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DISPSTAT & 0x3FF))
#define REG_VCOUNT   *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_VCOUNT & 0x3FF))

#define REG_BG0CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG0CNT & 0x3FF))
#define REG_BG1CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG1CNT & 0x3FF))
#define REG_BG2CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG2CNT & 0x3FF))
#define REG_BG3CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG3CNT & 0x3FF))

#define REG_BG0HOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG0HOFS & 0x3FF))
#define REG_BG0VOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG0VOFS & 0x3FF))
#define REG_BG1HOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG1HOFS & 0x3FF))
#define REG_BG1VOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG1VOFS & 0x3FF))
#define REG_BG2HOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG2HOFS & 0x3FF))
#define REG_BG2VOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG2VOFS & 0x3FF))
#define REG_BG3HOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG3HOFS & 0x3FF))
#define REG_BG3VOFS *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_BG3VOFS & 0x3FF))

// sound registers
// http://belogic.com/gba/
#define REG_SOUND1CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND1CNT_L & 0x3FF)) /* Sound 1 Sweep control */
#define REG_SOUND1CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND1CNT_H & 0x3FF)) /* Sound 1 Lenght, wave duty and envelope control */
#define REG_SOUND1CNT_X *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND1CNT_X & 0x3FF)) /* Sound 1 Frequency, reset and loop control */
#define REG_SOUND2CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND2CNT_L & 0x3FF)) /* Sound 2 Lenght, wave duty and envelope control */
#define REG_SOUND2CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND2CNT_H & 0x3FF)) /* Sound 2 Frequency, reset and loop control */
#define REG_SOUND3CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND3CNT_L & 0x3FF)) /* Sound 3 Enable and wave ram bank control */
#define REG_SOUND3CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND3CNT_H & 0x3FF)) /* Sound 3 Sound lenght and output level control */
#define REG_SOUND3CNT_X *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND3CNT_X & 0x3FF)) /* Sound 3 Frequency, reset and loop control */
#define REG_SOUND4CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND4CNT_L & 0x3FF)) /* Sound 4 Lenght, output level and envelope control */
#define REG_SOUND4CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUND4CNT_H & 0x3FF)) /* Sound 4 Noise parameters, reset and loop control */
#define REG_SOUNDCNT_L  *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUNDCNT_L & 0x3FF)) /* Sound 1-4 Output level and Stereo control */
#define REG_SOUNDCNT_H  *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUNDCNT_H & 0x3FF)) /* Direct Sound control and Sound 1-4 output ratio */
#define REG_SOUNDCNT_X  *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUNDCNT_X & 0x3FF)) /* Master sound enable and Sound 1-4 play status */
#define REG_SOUNDBIAS   *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_SOUNDBIAS & 0x3FF)) /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM     reinterpret_cast<uint8_t*>(gba.mem.io + (mem::IO_WAVE_RAM0_L & 0x3FF)) /* Sound 3 samples 0-3 */
#define REG_WAVE_RAM0_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM0_L & 0x3FF)) /* Sound 3 samples 0-3 */
#define REG_WAVE_RAM0_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM0_H & 0x3FF)) /* Sound 3 samples 4-7 */
#define REG_WAVE_RAM1_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM1_L & 0x3FF)) /* Sound 3 samples 8-11 */
#define REG_WAVE_RAM1_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM1_H & 0x3FF)) /* Sound 3 samples 12-15 */
#define REG_WAVE_RAM2_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM2_L & 0x3FF)) /* Sound 3 samples 16-19 */
#define REG_WAVE_RAM2_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM2_H & 0x3FF)) /* Sound 3 samples 20-23 */
#define REG_WAVE_RAM3_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM3_L & 0x3FF)) /* Sound 3 samples 23-27 */
#define REG_WAVE_RAM3_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_WAVE_RAM3_H & 0x3FF)) /* Sound 3 samples 28-31 */
#define REG_FIFO_A_L    *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_FIFO_A_L & 0x3FF)) /* Direct Sound channel A samples 0-1 */
#define REG_FIFO_A_H    *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_FIFO_A_H & 0x3FF)) /* Direct Sound channel A samples 2-3 */
#define REG_FIFO_B_L    *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_FIFO_B_L & 0x3FF)) /* Direct Sound channel B samples 0-1 */
#define REG_FIFO_B_H    *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_FIFO_B_H & 0x3FF)) /* Direct Sound channel B samples 2-3 */

#define REG_DMA0SAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA0SAD & 0x3FF))
#define REG_DMA1SAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA1SAD & 0x3FF))
#define REG_DMA2SAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA2SAD & 0x3FF))
#define REG_DMA3SAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA3SAD & 0x3FF))

#define REG_DMA0DAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA0DAD & 0x3FF))
#define REG_DMA1DAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA1DAD & 0x3FF))
#define REG_DMA2DAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA2DAD & 0x3FF))
#define REG_DMA3DAD *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA3DAD & 0x3FF))

#define REG_DMA0CNT *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA0CNT & 0x3FF))
#define REG_DMA1CNT *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA1CNT & 0x3FF))
#define REG_DMA2CNT *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA2CNT & 0x3FF))
#define REG_DMA3CNT *reinterpret_cast<uint32_t*>(gba.mem.io + (mem::IO_DMA3CNT & 0x3FF))

#define REG_DMA0CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA0CNT_L & 0x3FF))
#define REG_DMA1CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA1CNT_L & 0x3FF))
#define REG_DMA2CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA2CNT_L & 0x3FF))
#define REG_DMA3CNT_L *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA3CNT_L & 0x3FF))

#define REG_DMA0CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA0CNT_H & 0x3FF))
#define REG_DMA1CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA1CNT_H & 0x3FF))
#define REG_DMA2CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA2CNT_H & 0x3FF))
#define REG_DMA3CNT_H *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_DMA3CNT_H & 0x3FF))

#define REG_TM0D *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM0D & 0x3FF))
#define REG_TM1D *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM1D & 0x3FF))
#define REG_TM2D *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM2D & 0x3FF))
#define REG_TM3D *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM3D & 0x3FF))

#define REG_TM0CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM0CNT & 0x3FF))
#define REG_TM1CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM1CNT & 0x3FF))
#define REG_TM2CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM2CNT & 0x3FF))
#define REG_TM3CNT *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_TM3CNT & 0x3FF))

#define REG_KEY *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_KEY & 0x3FF))
#define REG_IE *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_IE & 0x3FF))
#define REG_IF *reinterpret_cast<uint16_t*>(gba.mem.io + (mem::IO_IF & 0x3FF))
#define REG_IME *reinterpret_cast<uint8_t*>(gba.mem.io + (mem::IO_IME & 0x3FF))

auto on_savestate_load(Gba& gba) -> void;
auto reset(Gba& gba) -> void;

[[nodiscard]]
STATIC_INLINE auto read8(Gba& gba, std::uint32_t addr) -> std::uint8_t;
[[nodiscard]]
STATIC_INLINE auto read16(Gba& gba, std::uint32_t addr) -> std::uint16_t;
[[nodiscard]]
STATIC_INLINE auto read32(Gba& gba, std::uint32_t addr) -> std::uint32_t;
STATIC_INLINE auto write8(Gba& gba, std::uint32_t addr, std::uint8_t value) -> void;
STATIC_INLINE auto write16(Gba& gba, std::uint32_t addr, std::uint16_t value) -> void;
STATIC_INLINE auto write32(Gba& gba, std::uint32_t addr, std::uint32_t value) -> void;

} // namespace gba::mem
