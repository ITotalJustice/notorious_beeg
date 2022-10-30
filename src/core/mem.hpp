// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::mem {

// this is used as a mask for if access is allowed for the bit width
// i chose this instead of doing `ptr != null` as it meant i'd have to
// have different tables for 8,16,32.
// it's common for access to be allowed only 16,32 and have special
// handling for 8bit.
enum Access : u8
{
    Access_8bit  = sizeof(u8),  // 1
    Access_16bit = sizeof(u16), // 2
    Access_32bit = sizeof(u32), // 4

    Access_ALL  = Access_8bit | Access_16bit | Access_32bit,
    Access_NONE = 0,
};

enum
{
    SEQ = 0, // sequential access
    NSEQ = 1, // non-sequential access
};

// packed to 32bit on x86 (struct == 8 bytes)
// 4 bytes padding on x64 (struct == 16 bytes)
struct ReadArray
{
    const u8* array;
    u32 mask : 25; // only need max of 25 bits
    u8 access : 3; // only need 0-7 values
};

struct WriteArray
{
    u8* array;
    u32 mask : 25; // only need max of 25 bits
    u8 access : 3; // only need 0-7 values
};

struct Mem
{
    // 256kb, 16-bit bus
    u8 ewram[1024 * 256];

    // 32kb, 32-bit bus
    u8 iwram[1024 * 32];

    // 1kb, 16-bit
    u8 pram[1024 * 1];

    // 96kb, 16-bit bus
    u8 vram[1024 * 96];

    // 1kb, 32-bit
    u8 oam[1024 * 1];

    // 1kb, 16/32-bit
    u16 io[0x302 >> 1]; // size is haltcnt_h + 1, as rest of io is unsued

    // internal memory control
    u16 imc_l;
    u16 imc_h;

    // the value thats returning when reading from bios
    u32 bios_openbus_value;
};

enum GeneralInternalMemory
{
    BIOS_MASK  = 0x00003FFF,
    EWRAM_MASK = 0x0003FFFF,
    IWRAM_MASK = 0x00007FFF,
    IO_MASK    = 0x3FF,

    BIOS_SIZE  = BIOS_MASK + 1,
    EWRAM_SIZE = EWRAM_MASK + 1,
    IWRAM_SIZE = IWRAM_MASK + 1,
    IO_SIZE    = IO_MASK + 1,
};

enum InternalDisplayMemory
{
    PRAM_MASK = 0x000003FF,
    VRAM_MASK = 0x0001FFFF,
    OAM_MASK  = 0x000003FF,

    PRAM_SIZE = PRAM_MASK + 1,
    VRAM_SIZE = 0x00017FFF + 1,
    OAM_SIZE  = OAM_MASK + 1,
};

enum ExternalMemory // (Game Pak)
{
    ROM_MASK = 0x01FFFFFF,
    ROM_SIZE = ROM_MASK + 1,
};

enum GPIOAddr
{
    GPIO_DATA = 0x80000C4,
    GPIO_DIRECTION = 0x80000C6,
    GPIO_CONTROL = 0x80000C8,
};

enum IOAddr
{
    IO_DISPCNT = 0x04000000,
    IO_DISPSTAT = 0x04000004,
    IO_VCOUNT = 0x04000006,

    IO_BG0CNT = 0x04000008,
    IO_BG1CNT = 0x0400000A,
    IO_BG2CNT = 0x0400000C,
    IO_BG3CNT = 0x0400000E,

    IO_BG0HOFS = 0x04000010, // (w)
    IO_BG0VOFS = 0x04000012, // (w)
    IO_BG1HOFS = 0x04000014, // (w)
    IO_BG1VOFS = 0x04000016, // (w)
    IO_BG2HOFS = 0x04000018, // (w)
    IO_BG2VOFS = 0x0400001A, // (w)
    IO_BG3HOFS = 0x0400001C, // (w)
    IO_BG3VOFS = 0x0400001E, // (w)

    IO_BG2PA = 0x04000020, // (w)
    IO_BG2PB = 0x04000022, // (w)
    IO_BG2PC = 0x04000024, // (w)
    IO_BG2PD = 0x04000026, // (w)
    IO_BG2X = 0x04000028, // (w)
    IO_BG2Y = 0x0400002C, // (w)

    IO_BG3PA = 0x04000030, // (w)
    IO_BG3PB = 0x04000032, // (w)
    IO_BG3PC = 0x04000034, // (w)
    IO_BG3PD = 0x04000036, // (w)
    IO_BG3X = 0x04000038, // (w)
    IO_BG3Y = 0x0400003C, // (w)

    IO_BG2X_LO = 0x04000028, // (w)
    IO_BG2X_HI = 0x0400002A, // (w)
    IO_BG2Y_LO = 0x0400002C, // (w)
    IO_BG2Y_HI = 0x0400002E, // (w)
    IO_BG3X_LO = 0x04000038, // (w)
    IO_BG3X_HI = 0x0400003A, // (w)
    IO_BG3Y_LO = 0x0400003C, // (w)
    IO_BG3Y_HI = 0x0400003E, // (w)

    IO_WIN0H = 0x4000040, // (w)
    IO_WIN1H = 0x4000042, // (w)
    IO_WIN0V = 0x4000044, // (w)
    IO_WIN1V = 0x4000046, // (w)
    IO_WININ = 0x4000048, // (r+w)
    IO_WINOUT = 0x400004A, // (r+w)

    IO_MOSAIC = 0x400004C, // (w)
    IO_BLDMOD = 0x4000050, // (r+w)
    IO_COLEV = 0x4000052, // (w)
    IO_COLEY = 0x4000054, // (w)

    IO_SOUND1CNT_L = 0x04000060,
    IO_SOUND1CNT_H = 0x04000062,
    IO_SOUND1CNT_X = 0x04000064,
    IO_SOUND2CNT_L = 0x04000068,
    IO_SOUND2CNT_H = 0x0400006C,
    IO_SOUND3CNT_L = 0x04000070,
    IO_SOUND3CNT_H = 0x04000072,
    IO_SOUND3CNT_X = 0x04000074,
    IO_SOUND4CNT_L = 0x04000078,
    IO_SOUND4CNT_H = 0x0400007C,
    IO_SOUNDCNT_L = 0x04000080,
    IO_SOUNDCNT_H = 0x04000082,
    IO_SOUNDCNT_X = 0x04000084,
    IO_SOUNDBIAS = 0x04000088,
    IO_WAVE_RAM0_L = 0x04000090,
    IO_WAVE_RAM0_H = 0x04000092,
    IO_WAVE_RAM1_L = 0x04000094,
    IO_WAVE_RAM1_H = 0x04000096,
    IO_WAVE_RAM2_L = 0x04000098,
    IO_WAVE_RAM2_H = 0x0400009A,
    IO_WAVE_RAM3_L = 0x0400009C,
    IO_WAVE_RAM3_H = 0x0400009E,
    IO_FIFO_A_L = 0x040000A0, // (w)
    IO_FIFO_A_H = 0x040000A2, // (w)
    IO_FIFO_B_L = 0x040000A4, // (w)
    IO_FIFO_B_H = 0x040000A6, // (w)

    // https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#DMA%20Source%20Registers
    IO_DMA0SAD = 0x40000B0, // 27bit (w)
    IO_DMA1SAD = 0x40000BC, // 28bit (w)
    IO_DMA2SAD = 0x40000C8, // 28bit (w)
    IO_DMA3SAD = 0x40000D4, // 28bit (w)

    IO_DMA0DAD = 0x40000B4, // 27bit (w)
    IO_DMA1DAD = 0x40000C0, // 27bit (w)
    IO_DMA2DAD = 0x40000CC, // 27bit (w)
    IO_DMA3DAD = 0x40000D8, // 28bit (w)

    IO_DMA0SAD_LO = 0x40000B0, // (w)
    IO_DMA0SAD_HI = 0x40000B2, // (w)
    IO_DMA1SAD_LO = 0x40000BC, // (w)
    IO_DMA1SAD_HI = 0x40000BE, // (w)
    IO_DMA2SAD_LO = 0x40000C8, // (w)
    IO_DMA2SAD_HI = 0x40000CA, // (w)
    IO_DMA3SAD_LO = 0x40000D4, // (w)
    IO_DMA3SAD_HI = 0x40000D6, // (w)

    IO_DMA0DAD_LO = 0x40000B4, // (w)
    IO_DMA0DAD_HI = 0x40000B6, // (w)
    IO_DMA1DAD_LO = 0x40000C0, // (w)
    IO_DMA1DAD_HI = 0x40000C2, // (w)
    IO_DMA2DAD_LO = 0x40000CC, // (w)
    IO_DMA2DAD_HI = 0x40000CE, // (w)
    IO_DMA3DAD_LO = 0x40000D8, // (w)
    IO_DMA3DAD_HI = 0x40000DA, // (w)

    IO_DMA0CNT = 0x40000B8, // (DMA0 Count Register)
    IO_DMA1CNT = 0x40000C4, // (DMA1 Count Register)
    IO_DMA2CNT = 0x40000D0, // (DMA2 Count Register)
    IO_DMA3CNT = 0x40000DC, // (DMA3 Count Register)

    // same as above, but 16-bit (cowbite docs refer to them as such)
    IO_DMA0CNT_L = 0x40000B8, // (DMA0 Count Register) (w)
    IO_DMA1CNT_L = 0x40000C4, // (DMA1 Count Register) (w)
    IO_DMA2CNT_L = 0x40000D0, // (DMA2 Count Register) (w)
    IO_DMA3CNT_L = 0x40000DC, // (DMA3 Count Register) (w)

    IO_DMA0CNT_H = 0x40000BA, // (DMA0 Control Register)
    IO_DMA1CNT_H = 0x40000C6, // (DMA1 Control Register)
    IO_DMA2CNT_H = 0x40000D2, // (DMA2 Control Register)
    IO_DMA3CNT_H = 0x40000DE, // (DMA3 Control Register)

    IO_TM0D = 0x4000100, // (Timer 0 Data)
    IO_TM1D = 0x4000104, // (Timer 1 Data)
    IO_TM2D = 0x4000108, // (Timer 2 Data)
    IO_TM3D = 0x400010C, // (Timer 3 Data)

    IO_TM0CNT = 0x4000102, // (Timer 0 Control)
    IO_TM1CNT = 0x4000106, // (Timer 1 Control)
    IO_TM2CNT = 0x400010A, // (Timer 2 Control)
    IO_TM3CNT = 0x400010E, // (Timer 3 Control)

    // general
    IO_SIODATA8 = 0x0400012A, // SIO Normal Communication 8bit Data (R/W)
    IO_SIODATA32_L = 0x04000120, // SIO Normal Communication lower 16bit data (R/W)
    IO_SIODATA32_H = 0x04000122, // SIO Normal Communication upper 16bit data (R/W)
    IO_SIOCNT = 0x04000128, // SIO Control, usage in NORMAL Mode (R/W)

    // multiplayer
    IO_SIOMLT_SEND = 0x0400012A, // Data Send Register (R/W)
    IO_SIOMULTI0 = 0x04000120, // SIO Multi-Player Data 0 (Parent) (R/W)
    IO_SIOMULTI1 = 0x04000122, // SIO Multi-Player Data 1 (1st child) (R/W)
    IO_SIOMULTI2 = 0x04000124, // SIO Multi-Player Data 2 (2nd child) (R/W)
    IO_SIOMULTI3 = 0x04000126, // SIO Multi-Player Data 3 (3rd child) (R/W)

    IO_KEY = 0x04000130, // (The input register)(Read Only)
    IO_KEYCNT = 0x04000132, // (Input control register, used for interrupts)
    IO_RCNT = 0x04000134, // Mode Selection (R), in Normal/Multiplayer/UART modes (R/W)

    IO_IE = 0x04000200, // (Interrupt Enable Register)
    IO_IF = 0x04000202, // (Interrupt Flags Regster)
    IO_WSCNT = 0x4000204, // (Wait State Control)
    IO_IME = 0x4000208, // (Interrupt Master Enable)

    IO_HALTCNT_L = 0x4000300, // (First Boot/Debug Control)
    IO_HALTCNT_H = 0x4000301, // (Low Power Mode Control)

    IO_IMC_L = 0x4000800, // (Internal Memory Control)
    IO_IMC_H = 0x4000802, // (Internal Memory Control)
};

enum LogOnOff
{
    IO_LOG_ON = 0xC0DE,
    IO_LOG_OFF = 0x0000,
    IO_LOG_ON_RESULT = 0x1DEA,
};

enum Breakpoint
{
    BREAKPOINT_ON = 0xC0DE,
};

enum LogLevel
{
    IO_LOG_LEVEL_FATAL = 0,
    IO_LOG_LEVEL_ERROR = 1,
    IO_LOG_LEVEL_WARN = 2,
    IO_LOG_LEVEL_INFO = 3,
    IO_LOG_LEVEL_DEBUG = 4
};

enum LogAddr
{
    IO_MGBA_STDOUT = 0x4FFF600,
    IO_MGBA_FLAGS = 0x4FFF700,
    IO_MGBA_CONTROL = 0x4FFF780,

    IO_BREAKPOINT = 0x4FFF790, // todo:
};

#define REG_DISPCNT  gba.mem.io[(gba::mem::IO_DISPCNT & 0x3FF) >> 1]
#define REG_DISPSTAT gba.mem.io[(gba::mem::IO_DISPSTAT & 0x3FF) >> 1]
#define REG_VCOUNT   gba.mem.io[(gba::mem::IO_VCOUNT & 0x3FF) >> 1]

#define REG_BG0CNT gba.mem.io[(gba::mem::IO_BG0CNT & 0x3FF) >> 1]
#define REG_BG1CNT gba.mem.io[(gba::mem::IO_BG1CNT & 0x3FF) >> 1]
#define REG_BG2CNT gba.mem.io[(gba::mem::IO_BG2CNT & 0x3FF) >> 1]
#define REG_BG3CNT gba.mem.io[(gba::mem::IO_BG3CNT & 0x3FF) >> 1]

#define REG_BG0HOFS gba.mem.io[(gba::mem::IO_BG0HOFS & 0x3FF) >> 1]
#define REG_BG0VOFS gba.mem.io[(gba::mem::IO_BG0VOFS & 0x3FF) >> 1]
#define REG_BG1HOFS gba.mem.io[(gba::mem::IO_BG1HOFS & 0x3FF) >> 1]
#define REG_BG1VOFS gba.mem.io[(gba::mem::IO_BG1VOFS & 0x3FF) >> 1]
#define REG_BG2HOFS gba.mem.io[(gba::mem::IO_BG2HOFS & 0x3FF) >> 1]
#define REG_BG2VOFS gba.mem.io[(gba::mem::IO_BG2VOFS & 0x3FF) >> 1]
#define REG_BG3HOFS gba.mem.io[(gba::mem::IO_BG3HOFS & 0x3FF) >> 1]
#define REG_BG3VOFS gba.mem.io[(gba::mem::IO_BG3VOFS & 0x3FF) >> 1]

#define REG_BG2PA gba.mem.io[(gba::mem::IO_BG2PA & 0x3FF) >> 1]
#define REG_BG2PB gba.mem.io[(gba::mem::IO_BG2PB & 0x3FF) >> 1]
#define REG_BG2PC gba.mem.io[(gba::mem::IO_BG2PC & 0x3FF) >> 1]
#define REG_BG2PD gba.mem.io[(gba::mem::IO_BG2PD & 0x3FF) >> 1]
#define REG_BG2X_LO gba.mem.io[(gba::mem::IO_BG2X_LO & 0x3FF) >> 1]
#define REG_BG2X_HI gba.mem.io[(gba::mem::IO_BG2X_HI & 0x3FF) >> 1]
#define REG_BG2Y_LO gba.mem.io[(gba::mem::IO_BG2Y_LO & 0x3FF) >> 1]
#define REG_BG2Y_HI gba.mem.io[(gba::mem::IO_BG2Y_HI & 0x3FF) >> 1]

#define REG_BG3PA gba.mem.io[(gba::mem::IO_BG3PA & 0x3FF) >> 1]
#define REG_BG3PB gba.mem.io[(gba::mem::IO_BG3PB & 0x3FF) >> 1]
#define REG_BG3PC gba.mem.io[(gba::mem::IO_BG3PC & 0x3FF) >> 1]
#define REG_BG3PD gba.mem.io[(gba::mem::IO_BG3PD & 0x3FF) >> 1]
#define REG_BG3X_LO gba.mem.io[(gba::mem::IO_BG3X_LO & 0x3FF) >> 1]
#define REG_BG3X_HI gba.mem.io[(gba::mem::IO_BG3X_HI & 0x3FF) >> 1]
#define REG_BG3Y_LO gba.mem.io[(gba::mem::IO_BG3Y_LO & 0x3FF) >> 1]
#define REG_BG3Y_HI gba.mem.io[(gba::mem::IO_BG3Y_HI & 0x3FF) >> 1]

#define REG_WIN0H gba.mem.io[(gba::mem::IO_WIN0H & 0x3FF) >> 1]
#define REG_WIN1H gba.mem.io[(gba::mem::IO_WIN1H & 0x3FF) >> 1]
#define REG_WIN0V gba.mem.io[(gba::mem::IO_WIN0V & 0x3FF) >> 1]
#define REG_WIN1V gba.mem.io[(gba::mem::IO_WIN1V & 0x3FF) >> 1]

#define REG_WININ gba.mem.io[(gba::mem::IO_WININ & 0x3FF) >> 1]
#define REG_WINOUT gba.mem.io[(gba::mem::IO_WINOUT & 0x3FF) >> 1]

#define REG_MOSAIC gba.mem.io[(gba::mem::IO_MOSAIC & 0x3FF) >> 1]
#define REG_BLDMOD gba.mem.io[(gba::mem::IO_BLDMOD & 0x3FF) >> 1]
#define REG_COLEV gba.mem.io[(gba::mem::IO_COLEV & 0x3FF) >> 1]
#define REG_COLEY gba.mem.io[(gba::mem::IO_COLEY & 0x3FF) >> 1]

// sound registers
// http://belogic.com/gba/
#define REG_SOUND1CNT_L gba.mem.io[(gba::mem::IO_SOUND1CNT_L & 0x3FF) >> 1] /* Sound 1 Sweep control */
#define REG_SOUND1CNT_H gba.mem.io[(gba::mem::IO_SOUND1CNT_H & 0x3FF) >> 1] /* Sound 1 Lenght, wave duty and envelope control */
#define REG_SOUND1CNT_X gba.mem.io[(gba::mem::IO_SOUND1CNT_X & 0x3FF) >> 1] /* Sound 1 Frequency, reset and loop control */

#define REG_SOUND2CNT_L gba.mem.io[(gba::mem::IO_SOUND2CNT_L & 0x3FF) >> 1] /* Sound 2 Lenght, wave duty and envelope control */
#define REG_SOUND2CNT_H gba.mem.io[(gba::mem::IO_SOUND2CNT_H & 0x3FF) >> 1] /* Sound 2 Frequency, reset and loop control */

#define REG_SOUND3CNT_L gba.mem.io[(gba::mem::IO_SOUND3CNT_L & 0x3FF) >> 1] /* Sound 3 Enable and wave ram bank control */
#define REG_SOUND3CNT_H gba.mem.io[(gba::mem::IO_SOUND3CNT_H & 0x3FF) >> 1] /* Sound 3 Sound lenght and output level control */
#define REG_SOUND3CNT_X gba.mem.io[(gba::mem::IO_SOUND3CNT_X & 0x3FF) >> 1] /* Sound 3 Frequency, reset and loop control */

#define REG_SOUND4CNT_L gba.mem.io[(gba::mem::IO_SOUND4CNT_L & 0x3FF) >> 1] /* Sound 4 Lenght, output level and envelope control */
#define REG_SOUND4CNT_H gba.mem.io[(gba::mem::IO_SOUND4CNT_H & 0x3FF) >> 1] /* Sound 4 Noise parameters, reset and loop control */

#define REG_SOUNDCNT_L  gba.mem.io[(gba::mem::IO_SOUNDCNT_L & 0x3FF) >> 1] /* Sound 1-4 Output level and Stereo control */
#define REG_SOUNDCNT_H  gba.mem.io[(gba::mem::IO_SOUNDCNT_H & 0x3FF) >> 1] /* Direct Sound control and Sound 1-4 output ratio */
#define REG_SOUNDCNT_X  gba.mem.io[(gba::mem::IO_SOUNDCNT_X & 0x3FF) >> 1] /* Master sound enable and Sound 1-4 play status */

#define REG_SOUNDBIAS   gba.mem.io[(gba::mem::IO_SOUNDBIAS & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */

#define REG_WAVE_RAM0_L gba.mem.io[(gba::mem::IO_WAVE_RAM0_L & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM0_H gba.mem.io[(gba::mem::IO_WAVE_RAM0_H & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM1_L gba.mem.io[(gba::mem::IO_WAVE_RAM1_L & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM1_H gba.mem.io[(gba::mem::IO_WAVE_RAM1_H & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM2_L gba.mem.io[(gba::mem::IO_WAVE_RAM2_L & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM2_H gba.mem.io[(gba::mem::IO_WAVE_RAM2_H & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM3_L gba.mem.io[(gba::mem::IO_WAVE_RAM3_L & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */
#define REG_WAVE_RAM3_H gba.mem.io[(gba::mem::IO_WAVE_RAM3_H & 0x3FF) >> 1] /* Sound bias and Amplitude resolution control */

#define REG_FIFO_A_L    gba.mem.io[(gba::mem::IO_FIFO_A_L & 0x3FF) >> 1] /* Direct Sound channel A samples 0-1 */
#define REG_FIFO_A_H    gba.mem.io[(gba::mem::IO_FIFO_A_H & 0x3FF) >> 1] /* Direct Sound channel A samples 2-3 */
#define REG_FIFO_B_L    gba.mem.io[(gba::mem::IO_FIFO_B_L & 0x3FF) >> 1] /* Direct Sound channel B samples 0-1 */
#define REG_FIFO_B_H    gba.mem.io[(gba::mem::IO_FIFO_B_H & 0x3FF) >> 1] /* Direct Sound channel B samples 2-3 */

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

#define REG_DMA0SAD_LO gba.mem.io[(gba::mem::IO_DMA0SAD_LO & 0x3FF) >> 1]
#define REG_DMA0SAD_HI gba.mem.io[(gba::mem::IO_DMA0SAD_HI & 0x3FF) >> 1]

#define REG_DMA1SAD_LO gba.mem.io[(gba::mem::IO_DMA1SAD_LO & 0x3FF) >> 1]
#define REG_DMA1SAD_HI gba.mem.io[(gba::mem::IO_DMA1SAD_HI & 0x3FF) >> 1]

#define REG_DMA2SAD_LO gba.mem.io[(gba::mem::IO_DMA2SAD_LO & 0x3FF) >> 1]
#define REG_DMA2SAD_HI gba.mem.io[(gba::mem::IO_DMA2SAD_HI & 0x3FF) >> 1]

#define REG_DMA3SAD_LO gba.mem.io[(gba::mem::IO_DMA3SAD_LO & 0x3FF) >> 1]
#define REG_DMA3SAD_HI gba.mem.io[(gba::mem::IO_DMA3SAD_HI & 0x3FF) >> 1]

#define REG_DMA0DAD_LO gba.mem.io[(gba::mem::IO_DMA0DAD_LO & 0x3FF) >> 1]
#define REG_DMA0DAD_HI gba.mem.io[(gba::mem::IO_DMA0DAD_HI & 0x3FF) >> 1]

#define REG_DMA1DAD_LO gba.mem.io[(gba::mem::IO_DMA1DAD_LO & 0x3FF) >> 1]
#define REG_DMA1DAD_HI gba.mem.io[(gba::mem::IO_DMA1DAD_HI & 0x3FF) >> 1]

#define REG_DMA2DAD_LO gba.mem.io[(gba::mem::IO_DMA2DAD_LO & 0x3FF) >> 1]
#define REG_DMA2DAD_HI gba.mem.io[(gba::mem::IO_DMA2DAD_HI & 0x3FF) >> 1]

#define REG_DMA3DAD_LO gba.mem.io[(gba::mem::IO_DMA3DAD_LO & 0x3FF) >> 1]
#define REG_DMA3DAD_HI gba.mem.io[(gba::mem::IO_DMA3DAD_HI & 0x3FF) >> 1]

#define REG_DMA0CNT_L gba.mem.io[(gba::mem::IO_DMA0CNT_L & 0x3FF) >> 1]
#define REG_DMA1CNT_L gba.mem.io[(gba::mem::IO_DMA1CNT_L & 0x3FF) >> 1]

#define REG_DMA2CNT_L gba.mem.io[(gba::mem::IO_DMA2CNT_L & 0x3FF) >> 1]
#define REG_DMA3CNT_L gba.mem.io[(gba::mem::IO_DMA3CNT_L & 0x3FF) >> 1]

#define REG_DMA0CNT_H gba.mem.io[(gba::mem::IO_DMA0CNT_H & 0x3FF) >> 1]
#define REG_DMA1CNT_H gba.mem.io[(gba::mem::IO_DMA1CNT_H & 0x3FF) >> 1]

#define REG_DMA2CNT_H gba.mem.io[(gba::mem::IO_DMA2CNT_H & 0x3FF) >> 1]
#define REG_DMA3CNT_H gba.mem.io[(gba::mem::IO_DMA3CNT_H & 0x3FF) >> 1]

#define REG_TM0D gba.mem.io[(gba::mem::IO_TM0D & 0x3FF) >> 1]
#define REG_TM1D gba.mem.io[(gba::mem::IO_TM1D & 0x3FF) >> 1]
#define REG_TM2D gba.mem.io[(gba::mem::IO_TM2D & 0x3FF) >> 1]
#define REG_TM3D gba.mem.io[(gba::mem::IO_TM3D & 0x3FF) >> 1]

#define REG_TM0CNT gba.mem.io[(gba::mem::IO_TM0CNT & 0x3FF) >> 1]
#define REG_TM1CNT gba.mem.io[(gba::mem::IO_TM1CNT & 0x3FF) >> 1]
#define REG_TM2CNT gba.mem.io[(gba::mem::IO_TM2CNT & 0x3FF) >> 1]
#define REG_TM3CNT gba.mem.io[(gba::mem::IO_TM3CNT & 0x3FF) >> 1]

#define REG_SIODATA32_L gba.mem.io[(gba::mem::IO_SIODATA32_L & 0x3FF) >> 1]
#define REG_SIODATA32_H gba.mem.io[(gba::mem::IO_SIODATA32_H & 0x3FF) >> 1]
#define REG_SIOCNT gba.mem.io[(gba::mem::IO_SIOCNT & 0x3FF) >> 1]
#define REG_SIODATA8 gba.mem.io[(gba::mem::IO_SIODATA8 & 0x3FF) >> 1]

#define REG_SIOMLT_SEND gba.mem.io[(gba::mem::IO_SIOMLT_SEND & 0x3FF) >> 1]
#define REG_SIOMULTI0 gba.mem.io[(gba::mem::IO_SIOMULTI0 & 0x3FF) >> 1]
#define REG_SIOMULTI1 gba.mem.io[(gba::mem::IO_SIOMULTI1 & 0x3FF) >> 1]
#define REG_SIOMULTI2 gba.mem.io[(gba::mem::IO_SIOMULTI2 & 0x3FF) >> 1]
#define REG_SIOMULTI3 gba.mem.io[(gba::mem::IO_SIOMULTI3 & 0x3FF) >> 1]

#define REG_KEY gba.mem.io[(gba::mem::IO_KEY & 0x3FF) >> 1]
#define REG_KEYCNT gba.mem.io[(gba::mem::IO_KEYCNT & 0x3FF) >> 1]
#define REG_RCNT gba.mem.io[(gba::mem::IO_RCNT & 0x3FF) >> 1]

#define REG_IE gba.mem.io[(gba::mem::IO_IE & 0x3FF) >> 1]
#define REG_IF gba.mem.io[(gba::mem::IO_IF & 0x3FF) >> 1]
#define REG_WSCNT gba.mem.io[(gba::mem::IO_WSCNT & 0x3FF) >> 1]
#define REG_IME gba.mem.io[(gba::mem::IO_IME & 0x3FF) >> 1]

// note: this is actually 2 8bit registers but only the upper 8 bits
// are really used for halt and stop.
#define REG_HALTCNT gba.mem.io[(gba::mem::IO_HALTCNT_L & 0x3FF) >> 1]

#define REG_IMC_L gba.mem.imc_l
#define REG_IMC_H gba.mem.imc_h

auto setup_tables(Gba& gba) -> void;
auto reset(Gba& gba, bool skip_bios) -> void;

[[nodiscard]]
auto read8(Gba& gba, u32 addr) -> u8;
[[nodiscard]]
auto read16(Gba& gba, u32 addr) -> u16;
[[nodiscard]]
auto read32(Gba& gba, u32 addr) -> u32;

auto write8(Gba& gba, u32 addr, u8 value) -> void;
auto write16(Gba& gba, u32 addr, u16 value) -> void;
auto write32(Gba& gba, u32 addr, u32 value) -> void;

template <typename T> [[nodiscard]]
constexpr auto align(u32 addr) -> u32
{
    if constexpr(sizeof(T) == sizeof(u8))
    {
        return addr;
    }
    else if constexpr(sizeof(T) == sizeof(u16))
    {
        return addr & ~0x1;
    }
    else if constexpr(sizeof(T) == sizeof(u32))
    {
        return addr & ~0x3;
    }
}

} // namespace gba::mem
