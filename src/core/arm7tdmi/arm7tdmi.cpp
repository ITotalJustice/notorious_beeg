// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/arm/arm.hpp"
#include "arm7tdmi/thumb/thumb.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "bios_hle.hpp"
#include "mem.hpp"
#include "scheduler.hpp"
#include <cassert>
#include <cstdio>
#include <span>
#include <utility>

namespace gba::arm7tdmi {
namespace {

[[nodiscard]]
auto is_privileged_mode(Gba& gba)
{
    return get_mode(gba) != MODE_USER;
}

constexpr auto change_mode_save_regs(Gba& gba, std::span<u32> banked_regs, struct Psr* banked_spsr)
{
    const auto offset = 15 - banked_regs.size();

    for (std::size_t i = 0; i < banked_regs.size(); i++)
    {
        banked_regs[i] = CPU.registers[offset + i];
    }

    if (banked_spsr != nullptr)
    {
        *banked_spsr = CPU.spsr;
    }
}

constexpr auto change_mode_restore_regs(Gba& gba, std::span<const u32> banked_regs, const Psr* banked_spsr)
{
    const auto offset = 15 - banked_regs.size();

    for (std::size_t i = 0; i < banked_regs.size(); i++)
    {
        CPU.registers[offset + i] = banked_regs[i];
    }

    if (banked_spsr != nullptr)
    {
        CPU.spsr = *banked_spsr;
    }
}

[[nodiscard]]
constexpr auto is_valid_mode(const u8 mode) -> bool
{
    switch (mode)
    {
        case MODE_USER:
        case MODE_SYSTEM:
        case MODE_FIQ:
        case MODE_IRQ:
        case MODE_SUPERVISOR:
        case MODE_ABORT:
        case MODE_UNDEFINED:
            return true;
    }

    return false;
}

[[nodiscard]]
constexpr auto get_u32_from_psr(const Psr psr) -> u32
{
    u32 value = 0;
    value |= (psr.N << 31);
    value |= (psr.Z << 30);
    value |= (psr.C << 29);
    value |= (psr.V << 28);
    value |= (psr.I << 7);
    value |= (psr.F << 6);
    value |= (psr.T << 5);
    value |= psr.M;
    return value;
}

auto set_psr_from_u32(Gba& gba, Psr& psr, u32 value, const bool flag_write, const bool control_write) -> void
{
    // bit4 is always set! this means that the lowest
    // mode value possible is 16.
    // SEE: https://github.com/ITotalJustice/notorious_beeg/issues/44
    value = bit::set<4>(value);

    if (flag_write)
    {
        psr.N = bit::is_set<31>(value);
        psr.Z = bit::is_set<30>(value);
        psr.C = bit::is_set<29>(value);
        psr.V = bit::is_set<28>(value);
    }

    // control flags can only be changed in non-privilaged mode
    if (control_write && is_privileged_mode(gba))
    {
        psr.I = bit::is_set<7>(value);
        psr.T = bit::is_set<5>(value);
        psr.M = bit::get_range<0, 4>(value);

        schedule_interrupt(gba); // I may now be unset, enabling interrupts
    }
}

enum class Exception
{
    Reset,
    Undefined_Instruction,
    Software_Interrupt,
    Abort_Prefetch,
    Abort_Data,
    Reserved,
    IRQ,
    FIQ,
};

auto exception(Gba& gba, const Exception e)
{
    const auto state = get_state(gba);
    const auto cpsr = CPU.cpsr;
    const auto pc = get_pc(gba);

    u32 lr{};
    u32 vector{};
    u8 mode{};

    switch (e)
    {
        case Exception::Reset: // not tested
            lr = pc; // the value saved in lr_svc is unpredictable
            vector = 0x00000000;
            mode = MODE_SUPERVISOR;
            CPU.cpsr.I = true;
            CPU.cpsr.F = true;
            break;

        case Exception::Undefined_Instruction: // not tested
            lr = pc - (state == State::THUMB ? 2 : 4);
            vector = 0x00000004;
            mode = MODE_UNDEFINED;
            break;

        case Exception::Software_Interrupt:
            lr = pc - (state == State::THUMB ? 2 : 4);
            vector = 0x00000008;
            mode = MODE_SUPERVISOR;
            break;

        case Exception::Abort_Prefetch: // not tested
            lr = pc + (state == State::THUMB ? 2 : 0);
            vector = 0x0000000C;
            mode = MODE_ABORT;
            break;

        case Exception::Abort_Data: // not tested
            lr = pc + (state == State::THUMB ? 6 : 4); // todo: understand what lr should be here...
            vector = 0x00000010;
            mode = MODE_ABORT;
            break;

        case Exception::Reserved:
            assert(!"this exception should never be hit!");
            break;

        case Exception::IRQ:
            lr = pc + (state == State::THUMB ? 2 : 0);
            vector = 0x00000018;
            mode = MODE_IRQ;
            break;

        case Exception::FIQ: // not tested
            lr = pc + (state == State::THUMB ? 2 : 0);
            vector = 0x0000001C;
            mode = MODE_FIQ;
            break;
    }

    // swap to new mode
    change_mode(gba, get_mode(gba), mode);
    // store cpsr to spsr_mode
    CPU.spsr = cpsr;
    // disable interrupts
    disable_interrupts(gba);
    // set lr_mode to the next instruction
    set_lr(gba, lr);
    // jump to exception vector address in arm mode
    change_state(gba, State::ARM, vector);
}

auto on_interrupt(Gba& gba)
{
    //printf("performing irq IE: 0x%04X IF 0x%04X lr: 0x%08X pc: 0x%08X mode: %u state: %s\n", REG_IE, REG_IF, get_lr(gba), get_pc(gba), get_mode(gba), get_state(gba) == State::THUMB ? "THUMB" : "ARM");
    exception(gba, Exception::IRQ);
}

#if ENABLE_SCHEDULER == 0
auto poll_interrupts(Gba& gba) -> void
{
    if (REG_IE & REG_IF & 0b11'1111'1111'1111)
    {
        CPU.halted = false;

        if (REG_IME&1 && !CPU.cpsr.I)
        {
            on_interrupt(gba);
        }
    }
}
#endif

} // namespace

auto reset(Gba& gba, const bool skip_bios) -> void
{
    gba.cpu = {};

    CPU.cpsr.M = MODE_SUPERVISOR;
    CPU.cpsr.I = true;
    CPU.cpsr.F = true;
    CPU.cpsr.T = false; // start in arm

    // todo: compare values accross diff bios (normmatt / official)
    if (skip_bios)
    {
        CPU.registers[SP_INDEX] = 0x03007F00;
        CPU.registers[LR_INDEX] = 0x08000000;
        CPU.registers[PC_INDEX] = 0x08000000;

        CPU.banked_reg_irq[0] = 0x3007FA0; // SP
        CPU.banked_reg_svc[0] = 0x3007FE0; // SP

        CPU.cpsr.M = MODE_SYSTEM;
        CPU.cpsr.I = false;
        CPU.cpsr.F = false;
    }

    refill_pipeline(gba);
}

auto check_cond(const Gba& gba, const u8 cond) -> bool
{
    switch (cond & 0xF)
    {
        case COND_EQ: return CPU.cpsr.Z;
        case COND_NE: return !CPU.cpsr.Z;
        case COND_CS: return CPU.cpsr.C;
        case COND_CC: return !CPU.cpsr.C;
        case COND_MI: return CPU.cpsr.N;
        case COND_PL: return !CPU.cpsr.N;
        case COND_VS: return CPU.cpsr.V;
        case COND_VC: return !CPU.cpsr.V;

        case COND_HI: return CPU.cpsr.C && !CPU.cpsr.Z;
        case COND_LS: return !CPU.cpsr.C || CPU.cpsr.Z;
        case COND_GE: return CPU.cpsr.N == CPU.cpsr.V;
        case COND_LT: return CPU.cpsr.N != CPU.cpsr.V;
        case COND_GT: return !CPU.cpsr.Z && (CPU.cpsr.N == CPU.cpsr.V);
        case COND_LE: return CPU.cpsr.Z || (CPU.cpsr.N != CPU.cpsr.V);
        case COND_AL: return true;

        default:
            assert(!"unreachable hit");
            return false;
    }
}

auto refill_pipeline(Gba& gba) -> void
{
    switch (get_state(gba))
    {
        case State::ARM:
            CPU.pipeline[0] = mem::read32(gba, get_pc(gba) + 0);
            CPU.pipeline[1] = mem::read32(gba, get_pc(gba) + 4);
            gba.cpu.registers[PC_INDEX] += 4;
            break;

        case State::THUMB:
            CPU.pipeline[0] = mem::read16(gba, get_pc(gba) + 0);
            CPU.pipeline[1] = mem::read16(gba, get_pc(gba) + 2);
            gba.cpu.registers[PC_INDEX] += 2;
            break;
    }
}

auto change_mode(Gba& gba, const u8 old_mode, const u8 new_mode) -> void
{
    assert(is_valid_mode(new_mode));

    CPU.cpsr.M = new_mode;

    // don't swap mode if modes havent changed or going usr->sys or sys->usr
    if (old_mode == new_mode || (old_mode == MODE_USER && new_mode == MODE_SYSTEM) || (old_mode == MODE_SYSTEM && new_mode == MODE_USER))
    {
        return;
    }

    switch (old_mode)
    {
        case MODE_USER: case MODE_SYSTEM:
            change_mode_save_regs(gba, CPU.banked_reg_usr, nullptr);
            break;
        case MODE_FIQ:
            change_mode_save_regs(gba, CPU.banked_reg_fiq, &CPU.banked_spsr_fiq);
            // if the previous mode was fiq, then we are changing to
            // a mode with only r13-14 banked, so we need to restore
            // r8-12 that fiq banks.
            // SEE: https://github.com/ITotalJustice/notorious_beeg/issues/72
            CPU.registers[8] = CPU.banked_r8_r12[0];
            CPU.registers[9] = CPU.banked_r8_r12[1];
            CPU.registers[10] = CPU.banked_r8_r12[2];
            CPU.registers[11] = CPU.banked_r8_r12[3];
            CPU.registers[12] = CPU.banked_r8_r12[4];
            break;
        case MODE_IRQ:
            change_mode_save_regs(gba, CPU.banked_reg_irq, &CPU.banked_spsr_irq);
            break;
        case MODE_SUPERVISOR:
            change_mode_save_regs(gba, CPU.banked_reg_svc, &CPU.banked_spsr_svc);
            break;
        case MODE_ABORT:
            change_mode_save_regs(gba, CPU.banked_reg_abt, &CPU.banked_spsr_abt);
            break;
        case MODE_UNDEFINED:
            change_mode_save_regs(gba, CPU.banked_reg_und, &CPU.banked_spsr_und);
            break;
    }

    switch (new_mode)
    {
        case MODE_USER: case MODE_SYSTEM:
            change_mode_restore_regs(gba, CPU.banked_reg_usr, nullptr);
            break;
        case MODE_FIQ:
            // before loading fiq regs, bank regs r8-12
            // so that when leaving fiq, r8-12 can be restored.
            CPU.banked_r8_r12[0] = CPU.registers[8];
            CPU.banked_r8_r12[1] = CPU.registers[9];
            CPU.banked_r8_r12[2] = CPU.registers[10];
            CPU.banked_r8_r12[3] = CPU.registers[11];
            CPU.banked_r8_r12[4] = CPU.registers[12];
            change_mode_restore_regs(gba, CPU.banked_reg_fiq, &CPU.banked_spsr_fiq);
            break;
        case MODE_IRQ:
            change_mode_restore_regs(gba, CPU.banked_reg_irq, &CPU.banked_spsr_irq);
            break;
        case MODE_SUPERVISOR:
            change_mode_restore_regs(gba, CPU.banked_reg_svc, &CPU.banked_spsr_svc);
            break;
        case MODE_ABORT:
            change_mode_restore_regs(gba, CPU.banked_reg_abt, &CPU.banked_spsr_abt);
            break;
        case MODE_UNDEFINED:
            change_mode_restore_regs(gba, CPU.banked_reg_und, &CPU.banked_spsr_und);
            break;
    }
}

auto get_u32_from_cpsr(Gba& gba) -> u32
{
    return get_u32_from_psr(CPU.cpsr);
}

auto get_u32_from_spsr(Gba& gba) -> u32
{
    const auto mode = get_mode(gba);

    // assert(mode != MODE_USER && mode != MODE_SYSTEM && "user mode doesn't have spsr");
    if (mode != MODE_USER && mode != MODE_SYSTEM) [[likely]]
    {
        return get_u32_from_psr(CPU.spsr);
    }
    return get_u32_from_psr(CPU.cpsr);
}

auto load_spsr_mode_into_cpsr(Gba& gba) -> void
{
    const auto old_mode = CPU.cpsr.M;
    const auto new_mode = CPU.spsr.M;

    assert(old_mode != MODE_USER && old_mode != MODE_SYSTEM && "user mode doesn't have spsr");
    if (old_mode != MODE_USER && old_mode != MODE_SYSTEM) [[likely]]
    {
        CPU.cpsr.M = CPU.spsr.M;
        change_mode(gba, old_mode, new_mode);
    }
}

auto load_spsr_into_cpsr(Gba& gba) -> void
{
    const auto old_mode = CPU.cpsr.M;
    const auto new_mode = CPU.spsr.M;

    assert(old_mode != MODE_USER && "user mode doesn't have spsr");
    if (old_mode != MODE_USER && old_mode != MODE_SYSTEM) [[likely]]
    {
        CPU.cpsr = CPU.spsr;
        change_mode(gba, old_mode, new_mode);
        schedule_interrupt(gba); // I may now be unset, enabling interrupts
    }
}

auto set_cpsr_from_u32(Gba& gba, const u32 value, const bool flag_write, const bool control_write) -> void
{
    const auto old_mode = get_mode(gba);
    set_psr_from_u32(gba, CPU.cpsr, value, flag_write, control_write);
    const auto new_mode = get_mode(gba);
    change_mode(gba, old_mode, new_mode);
}

auto set_spsr_from_u32(Gba& gba, const u32 value, const bool flag_write, const bool control_write) -> void
{
    const auto mode = get_mode(gba);

    if (mode != MODE_USER && mode != MODE_SYSTEM) [[likely]]
    {
        set_psr_from_u32(gba, CPU.spsr, value, flag_write, control_write);
    }
}

auto get_mode(const Gba& gba) -> u8
{
    return CPU.cpsr.M;
}

auto get_state(const Gba& gba) -> State
{
    return static_cast<State>(CPU.cpsr.T);
}

auto get_lr(const Gba& gba) -> u32
{
    return get_reg(gba, LR_INDEX);
}

auto get_sp(const Gba& gba) -> u32
{
    return get_reg(gba, SP_INDEX);
}

auto get_pc(const Gba& gba) -> u32
{
    return get_reg(gba, PC_INDEX);
}

auto get_reg(const Gba& gba, const u8 reg) -> u32
{
    assert(reg <= 15);
    return CPU.registers[reg];
}

auto set_lr(Gba& gba, const u32 value) -> void
{
    set_reg(gba, LR_INDEX, value);
}

auto set_sp(Gba& gba, const u32 value) -> void
{
    set_reg(gba, SP_INDEX, value);
}

auto set_pc(Gba& gba, const u32 value) -> void
{
    set_reg(gba, PC_INDEX, value);
}

auto set_reg(Gba& gba, const u8 reg, const u32 value) -> void
{
    assert(reg <= 15);
    // todo: add asserts here for different modes
    CPU.registers[reg] = value;

    // hack for thumb, todo, fix
    if (reg == PC_INDEX) [[unlikely]]
    {
        CPU.registers[PC_INDEX] = mem::align<u16>(CPU.registers[PC_INDEX]);
        refill_pipeline(gba);
    }
}

// data processing manually handles refilling
auto set_reg_data_processing(Gba& gba, const u8 reg, const u32 value) -> void
{
    assert(reg <= 15);
    // todo: add asserts here for different modes
    CPU.registers[reg] = value;

    // hack for thumb, todo, fix
    if (reg == PC_INDEX) [[unlikely]]
    {
        CPU.registers[PC_INDEX] = mem::align<u16>(CPU.registers[PC_INDEX]);
    }
}

auto set_reg_thumb(Gba& gba, const u8 reg, const u32 value) -> void
{
    assert(reg <= 7);
    CPU.registers[reg] = value;
}

auto change_state(Gba &gba, const State new_state, const u32 new_pc) -> void
{
    // if bit0 == 0, switch to arm, else thumb
    switch (new_state)
    {
        case State::ARM:
            CPU.cpsr.T = std::to_underlying(arm7tdmi::State::ARM);
            set_pc(gba, mem::align<u32>(new_pc));
            break;

        case State::THUMB:
            CPU.cpsr.T = std::to_underlying(arm7tdmi::State::THUMB);
            set_pc(gba, mem::align<u16>(new_pc));
            break;
    }
}

auto software_interrupt(Gba& gba, const u8 comment_field) -> void
{
    // if not handled, do normal bios handling
    if (!bios::hle(gba, comment_field))
    {
        exception(gba, Exception::Software_Interrupt);
    }
}

auto fire_interrupt(Gba& gba, const Interrupt i) -> void
{
    REG_IF |= std::to_underlying(i);
    schedule_interrupt(gba);
}

auto disable_interrupts(Gba& gba) -> void
{
    CPU.cpsr.I = true; // 1=off
    scheduler::remove(gba, scheduler::Event::INTERRUPT);
}

auto on_interrupt_event(Gba& gba) -> void
{
    on_interrupt(gba);
}

auto schedule_interrupt(Gba& gba) -> void
{
    if (REG_IE & REG_IF & 0b11'1111'1111'1111)
    {
        CPU.halted = false;
        if (REG_IME&1 && !CPU.cpsr.I) [[likely]]
        {
            scheduler::add(gba, scheduler::Event::INTERRUPT, on_interrupt_event, 0);
        }
    }
}

auto on_halt_event(Gba& gba) -> void
{
    assert(gba.scheduler.next_event != scheduler::Event::HALT && "halt bug");

    while (CPU.halted && !gba.scheduler.frame_end)
    {
        assert(gba.scheduler.next_event_cycles >= gba.scheduler.cycles && "unsigned underflow happens!");
        gba.scheduler.cycles = gba.scheduler.next_event_cycles;
        gba.scheduler.elapsed = 0;
        scheduler::fire(gba);
    }
}

auto on_halt_trigger(Gba& gba, const HaltType type) -> void
{
    // halt can actually always be enabled regardless of
    // the value of IE and if interrupts are disabled or not.
    // however, we esnure the halt is valid, in order to catch bugs.
    if (REG_IE && !gba.cpu.cpsr.I) [[likely]]
    {
        if (type == HaltType::write)
        {
            if ((arm7tdmi::get_pc(gba) >> 24) != 0x0) [[unlikely]]
            {
                // fleroviux: I noticed that HALTCNT actually only seems accessible from BIOS code
                assert((arm7tdmi::get_pc(gba) >> 24) == 0x0 && "halt called outside bios region, let fleroviux know the game");
                return;
            }
        }

        assert(gba.cpu.halted == false);
        gba.cpu.halted = true;
        scheduler::add(gba, scheduler::Event::HALT, arm7tdmi::on_halt_event, 0);
    }
    else
    {
        std::printf("[HALT] called when no interrupt can happen!\n");
        assert(!"writing to hltcnt");
    }
}

auto run(Gba& gba) -> void
{
    #if ENABLE_SCHEDULER == 0
    poll_interrupts(gba);

    if (CPU.halted) [[unlikely]]
    {
        gba.scheduler.tick(1);
        return;
    }
    #endif

    // get which state (ARM, THUMB) we are in
    switch (get_state(gba))
    {
        case State::ARM:
            arm::execute(gba);
            break;

        case State::THUMB:
            thumb::execute(gba);
            break;
    }
}

} // namespace gba::arm7tdmi
