// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/arm/arm.hpp"
#include "arm7tdmi/thumb/thumb.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "bios_hle.hpp"
#include "mem.hpp"
#include "ppu/render.hpp"
#include "scheduler.hpp"
#include "log.hpp"

#include <bit>
#include <cassert>
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
    log::print_info(gba, log::Type::INTERRUPT, "num: %u lr: 0x%08X pc: 0x%08X mode: %u state: %s\n", std::countr_zero(static_cast<u16>(REG_IE & REG_IF)), get_lr(gba), get_pc(gba), get_mode(gba), get_state(gba) == State::THUMB ? "THUMB" : "ARM");
    gba.waitloop.on_event_change(gba, waitloop::WAITLOOP_EVENT_IRQ);
    exception(gba, Exception::IRQ);
}

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
    gba.scheduler.remove(scheduler::ID::INTERRUPT);
}

auto on_interrupt_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);
    on_interrupt(gba);
}

// todo: test if its possible for an irq to be set and then
// cancelled within the 3 second window.
auto schedule_interrupt(Gba& gba) -> void
{
    if (REG_IE & REG_IF & 0b11'1111'1111'1111)
    {
        CPU.halted = false;
        if (REG_IME&1 && !CPU.cpsr.I) [[likely]]
        {
            if (!gba.scheduler.has_event(scheduler::ID::INTERRUPT))
            {
                // TODO: fix me
                #if 1
                // delaying by 3 cycles breaks LOZMC :/
                gba.scheduler.add(scheduler::ID::INTERRUPT, 2, on_interrupt_event, &gba);
                #else
                // irq is delayed by 3 cycles, who would've known!
                // SOURCE: suite.gba timer-irq test
                gba.scheduler.add(scheduler::ID::INTERRUPT, 3, on_interrupt_event, &gba);
                #endif
            }
            else
            {
                log::print_warn(gba, log::Type::INTERRUPT, "skipping adding event: ticks: %d ev_ticks: %d\n", gba.scheduler.get_ticks(), gba.scheduler.get_event_cycles_absolute(scheduler::ID::INTERRUPT));
            }
        }
    }
}

auto on_halt_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);

    while (CPU.halted && !gba.frame_end)
    {
        const auto event_cycles = gba.scheduler.get_next_event_cycles();
        const auto event_cycles_abs = gba.scheduler.get_next_event_cycles_absolute();
        if (event_cycles > 0)
        {
            gba.cycles_spent_in_halt += event_cycles;
        }

        gba.scheduler.advance_to_next_event();
        gba.scheduler.fire();

        // mightve spent some cycles in dma which should not contribute
        // to cpu cycles whilst halted!
        if (gba.scheduler.get_ticks() - event_cycles_abs > 0)
        {
            gba.cycles_spent_in_halt += gba.scheduler.get_ticks() - event_cycles_abs;
        }
    }
}

// stop mode stops basically everything.
// no audio, ppu, cpu, timers, dma!
// only (known) way to exit is key, cart or sio interrupt
auto on_stop_event(void* user, s32 id, s32 late) -> void
{
    auto& gba = *static_cast<Gba*>(user);

    // here i make sure that
    // before anything, make sure that this stop is valid
    // by checking the bits in IE.
    // if nothing valid is set, we will be stopped forever!
    const auto sio = REG_IE & std::to_underlying(Interrupt::Serial);
    const auto key = REG_IE & std::to_underlying(Interrupt::Key);
    const auto cart = REG_IE & std::to_underlying(Interrupt::Cassette);

    if (!(REG_IME & 1))
    {
        log::print_fatal(gba, log::Type::ARM, "[STOP] called without IME enabled!\n");
    }

    if (!bit::is_set<7>(REG_DISPCNT))
    {
        log::print_fatal(gba, log::Type::ARM, "[STOP] screen not blanked!\n");
    }

    if (!sio && !key && !cart)
    {
        log::print_fatal(gba, log::Type::ARM, "[STOP] called without sio, key or cart set in IE!\n");
    }
    else
    {
        log::print_info(gba, log::Type::ARM, "[STOP] sio: %d key: %d cart: %d\n", sio, key, cart);
    }

    // to emulate this, we need to break of the running loop
    // from there, the main loop should call is_stop_mode() and
    // if true, do not run any code!
    gba.cpu.stopped = true;
    gba.frame_end = true;
    gba.scheduler.remove(scheduler::ID::FRAME);

    // mimick the lcd being disabled
    ppu::clear_screen(gba);
}

// this checks if bit15 is set of HALTCNT, if true,
// it then checks if
auto is_stop_mode(Gba& gba) -> bool
{
    return gba.cpu.stopped;
}

void leave_stop_mode(Gba& gba, Interrupt i)
{
    const auto interrupt = std::to_underlying(i);

    if (interrupt & REG_IE & std::to_underlying(Interrupt::Serial))
    {
        log::print_info(gba, log::Type::ARM, "[STOP] leaving via Serial\n");
        REG_HALTCNT = bit::unset<0xF>(REG_HALTCNT);
        gba.cpu.stopped = false;
    }
    else if (interrupt & REG_IE & std::to_underlying(Interrupt::Key))
    {
        log::print_info(gba, log::Type::ARM, "[STOP] leaving via Key\n");
        REG_HALTCNT = bit::unset<0xF>(REG_HALTCNT);
        gba.cpu.stopped = false;
    }
    else if (interrupt & REG_IE & std::to_underlying(Interrupt::Cassette))
    {
        log::print_info(gba, log::Type::ARM, "[STOP] leaving via Cassette\n");
        REG_HALTCNT = bit::unset<0xF>(REG_HALTCNT);
        gba.cpu.stopped = false;
    }
    else
    {
        log::print_error(gba, log::Type::ARM, "[STOP] invalid way of leaving: %u\n", interrupt);
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
        log::print_info(gba, log::Type::HALT, "halt called, pc: 0x%08X mode: %u state: %s\n", get_pc(gba), get_mode(gba), get_state(gba) == State::THUMB ? "THUMB" : "ARM");

        gba.cpu.halted = true;
        gba.scheduler.add(scheduler::ID::HALT, 0, arm7tdmi::on_halt_event, &gba);
    }
    else
    {
        log::print_fatal(gba, log::Type::HALT, "called when no interrupt can happen!\n");
        assert(!"writing to hltcnt");
    }
}

auto run(Gba& gba) -> void
{
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
