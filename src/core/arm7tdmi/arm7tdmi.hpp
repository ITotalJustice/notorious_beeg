// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "fwd.hpp"

namespace gba::arm7tdmi {

constexpr auto SP_INDEX = 13; // stack pointer
constexpr auto LR_INDEX = 14; // gets set to r15 during branch and links
constexpr auto PC_INDEX = 15; // bits 0-1 are zero in arm state, bit-0 is zero in thumb

constexpr auto MODE_USER       = 16;
constexpr auto MODE_FIQ        = 17;
constexpr auto MODE_IRQ        = 18;
constexpr auto MODE_SUPERVISOR = 19;
constexpr auto MODE_ABORT      = 23;
constexpr auto MODE_UNDEFINED  = 27;
constexpr auto MODE_SYSTEM     = 31;

constexpr auto COND_EQ = 0x0; // Z set
constexpr auto COND_NE = 0x1; // Z clear
constexpr auto COND_CS = 0x2; // C set
constexpr auto COND_CC = 0x3; // C clear
constexpr auto COND_MI = 0x4; // N set
constexpr auto COND_PL = 0x5; // N clear
constexpr auto COND_VS = 0x6; // V set
constexpr auto COND_VC = 0x7; // V clear
constexpr auto COND_HI = 0x8; // C set and Z clear
constexpr auto COND_LS = 0x9; // C clear or Z set
constexpr auto COND_GE = 0xA; // N equals V
constexpr auto COND_LT = 0xB; // N not equal to V
constexpr auto COND_GT = 0xC; // Z clear and N equals V
constexpr auto COND_LE = 0xD; // Z set or N not equal to V
constexpr auto COND_AL = 0xE; // ignored (AL = always)

enum class State : bool
{
    ARM   = false,
    THUMB = true,
};

// https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm#REG_IE
enum class Interrupt : u16
{
    VBlank   = 1 << 0x0, // (V) = VBlank Interrupt
    HBlank   = 1 << 0x1, // (H) = HBlank Interrupt
    VCount   = 1 << 0x2, // (C) = VCount Interrupt
    Timer0   = 1 << 0x3, // (I) = Timer 0 Interrupt
    Timer1   = 1 << 0x4, // (J) = Timer 1 Interrupt
    Timer2   = 1 << 0x5, // (K) = Timer 2 Interrupt
    Timer3   = 1 << 0x6, // (L) = Timer 3 Interrupt
    Serial   = 1 << 0x7, // (S) = Serial Communication Interrupt
    DMA0     = 1 << 0x8, // (D) = DMA0 Interrupt
    DMA1     = 1 << 0x9, // (E) = DMA1 Interrupt
    DMA2     = 1 << 0xA, // (F) = DMA2 Interrupt
    DMA3     = 1 << 0xB, // (G) = DMA3 Interrupt
    Key      = 1 << 0xC, // (Y) = Key Interrupt
    Cassette = 1 << 0xD, // (T) = Cassette Interrupt
};

struct Psr
{
    // condition flags
    bool N:1;     // negative, less than
    bool Z:1;     // zero
    bool C:1;     // carry, borrow, extend
    bool V:1;     // overflow

    // control
    bool I:1;     // IRQ disable (1=off,0=on)
    bool F:1;     // FIQ disable (1=off,0=on)
    bool T:1;     // state bit (1=thumb,0=arm)
    u8 M:5;  // mode
};

struct Arm7tdmi
{
    u32 pipeline[2];

    u32 registers[16];
    Psr cpsr;
    Psr spsr;

    u32 banked_r8_r12[5]; // used for restoring r8-12 leaving fiq
    u32 banked_reg_usr[2]; // used for restoring r13-14 when entering usr/sys mode

    u32 banked_reg_irq[2]; // r13_irq, r14_irq, SPSR_irq
    u32 banked_reg_fiq[7]; // r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq, and SPSR_fiq
    u32 banked_reg_svc[2]; // r13_svc, r14_svc, SPSR_svc.
    u32 banked_reg_abt[2]; // r13_abt, r14_abt, SPSR_abt.
    u32 banked_reg_und[2]; // r13_und, r14_und, SPSR_und.

    Psr banked_spsr_irq;
    Psr banked_spsr_fiq;
    Psr banked_spsr_svc;
    Psr banked_spsr_abt;
    Psr banked_spsr_und;

    bool halted;
};

#define CPU gba.cpu

STATIC auto reset(Gba& gba, bool skip_bios) -> void;

STATIC auto refill_pipeline(Gba& gba) -> void;

STATIC auto software_interrupt(Gba& gba, u8 comment_field) -> void;

STATIC auto schedule_interrupt(Gba& gba) -> void;
STATIC auto fire_interrupt(Gba& gba, Interrupt i) -> void;
STATIC auto disable_interrupts(Gba& gba) -> void;

STATIC auto change_mode(Gba& gba, u8 old_mode, u8 new_mode) -> void;
STATIC auto load_spsr_mode_into_cpsr(Gba& gba) -> void;
STATIC auto load_spsr_into_cpsr(Gba& gba) -> void;
[[nodiscard]]
STATIC auto get_u32_from_cpsr(Gba& gba) -> u32;
[[nodiscard]]
STATIC auto get_u32_from_spsr(Gba& gba) -> u32;
STATIC auto set_cpsr_from_u32(Gba& gba, u32 value, bool flag_write, bool control_write) -> void;
STATIC auto set_spsr_from_u32(Gba& gba, u32 value, bool flag_write, bool control_write) -> void;

[[nodiscard]]
STATIC_INLINE auto get_mode(const Gba& gba) -> u8;

[[nodiscard]]
STATIC_INLINE auto get_state(const Gba& gba) -> State;
[[nodiscard]]
STATIC_INLINE auto check_cond(const Gba& gba, u8 cond) -> bool;

[[nodiscard]]
STATIC_INLINE auto get_lr(const Gba& gba) -> u32;
[[nodiscard]]
STATIC_INLINE auto get_sp(const Gba& gba) -> u32;
[[nodiscard]]
STATIC_INLINE auto get_pc(const Gba& gba) -> u32;

STATIC_INLINE auto set_lr(Gba& gba, u32 value) -> void;
STATIC_INLINE auto set_sp(Gba& gba, u32 value) -> void;
STATIC_INLINE auto set_pc(Gba& gba, u32 value) -> void;

[[nodiscard]]
STATIC_INLINE auto get_reg(const Gba& gba, u8 reg) -> u32;
STATIC_INLINE auto set_reg(Gba& gba, u8 reg, u32 value) -> void;
STATIC_INLINE auto set_reg_data_processing(Gba& gba, u8 reg, u32 value) -> void;
STATIC_INLINE auto set_reg_thumb(Gba& gba, u8 reg, u32 value) -> void;

STATIC_INLINE auto run(Gba& gba) -> void;

// on halt event
enum class HaltType
{
    // REG_HLTCNT was written to
    // (pc is then checked to make sure the bios did this)
    write,
    // bios hle of hlt to skip mode switching
    hle_halt,
    // bios hle of waiting for vblank int until halt is exited
    // hle_vblank_halt,
    // bio hle of waiting for specific int until halt is exited
    // hle_int_halt,
};

STATIC auto on_interrupt_event(Gba& gba) -> void;
STATIC auto on_halt_event(Gba& gba) -> void;
STATIC auto on_halt_trigger(Gba& gba, HaltType type) -> void;

} // namespace gba::arm7tdmi
