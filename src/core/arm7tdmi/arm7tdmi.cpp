// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm7tdmi.hpp"
#include "arm7tdmi/arm/arm.hpp"
#include "arm7tdmi/thumb/thumb.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "bios_hle.hpp"
#include "scheduler.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace gba::arm7tdmi {

auto reset(Gba& gba) -> void
{
    // skip libc array init crt0 armwrestler.
    // jumps to <https://github.com/destoer/armwrestler-gba-fixed/blob/master/source/armwrestler.s#L23>
    // enum { ROM_START_ADDR = 0x080009E0 };
    enum { ROM_START_ADDR = 0x08000000 };
    // enum { ROM_START_ADDR = 0x080002D8 };
    // enum { ROM_START_ADDR = 0 }; // bios

    // CPU.cpsr.M = MODE_USER;
    CPU.cpsr.M = MODE_SYSTEM;
    CPU.registers[PC_INDEX] = ROM_START_ADDR;
    // these values are from no$gba
    CPU.registers[LR_INDEX] = 0x08000000;
    CPU.registers[SP_INDEX] = 0x03007F00;
    CPU.banked_reg_irq[0] = 0x3007FA0; // SP
    CPU.banked_reg_svc[0] = 0x3007FE0; // SP
    set_cpsr_from_u32(gba, 0x6000001F, true, true);

    refill_pipeline(gba);
    disable_interrupts(gba);
}

auto check_cond(const Gba& gba, uint8_t cond) -> bool
{
    switch (cond)
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
        [[likely]] case COND_AL: return true;
    }

    assert(!"unreachable hit");
    return false;
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

auto change_mode_save_regs(Gba& gba, std::span<std::uint32_t> banked_regs, struct Psr* banked_spsr)
{
    const auto offset = 15 - banked_regs.size();

    for (std::size_t i = 0; i < banked_regs.size(); i++)
    {
        gba_log("storing: 0x%08X from: %zu to %zu\n", CPU.registers[offset + i], offset + i, i);
        banked_regs[i] = CPU.registers[offset + i];
    }

    if (banked_spsr != nullptr)
    {
        *banked_spsr = CPU.spsr;
    }
}

auto change_mode_restore_regs(Gba& gba, std::span<const std::uint32_t> banked_regs, const Psr* banked_spsr, int max)
{
    std::size_t i = 0;
    const auto offset = 15 - banked_regs.size();
    if (max > 0)
    {
        //std::printf("max is %d\n", max);
        i = banked_regs.size() - max;
    }

    for (; i < banked_regs.size(); i++)
    {
        gba_log("restoring: 0x%08X from: %zu to %zu\n", banked_regs[i], i, offset+i);
        CPU.registers[offset + i] = banked_regs[i];
    }

    if (banked_spsr != nullptr)
    {
        CPU.spsr = *banked_spsr;
    }
}

auto change_mode(Gba& gba, uint8_t old_mode, uint8_t new_mode) -> void
{
    CPU.cpsr.M = new_mode;

    // don't swap mode if modes havent changed or going usr->sys or sys->usr
    if (old_mode == new_mode || (old_mode == MODE_USER && new_mode == MODE_SYSTEM) || (old_mode == MODE_SYSTEM && new_mode == MODE_USER))
    {
        return;
    }

    int max = -1;
    switch (old_mode)
    {
        case MODE_USER: case MODE_SYSTEM:
            change_mode_save_regs(gba, CPU.banked_reg_usr, nullptr);
            break;
        case MODE_FIQ:
            change_mode_save_regs(gba, CPU.banked_reg_fiq, &CPU.banked_spsr_fiq);
            max = sizeof(CPU.banked_reg_fiq) / sizeof(CPU.banked_reg_fiq[0]);
            break;
        case MODE_IRQ:
            change_mode_save_regs(gba, CPU.banked_reg_irq, &CPU.banked_spsr_irq);
            max = sizeof(CPU.banked_reg_irq) / sizeof(CPU.banked_reg_irq[0]);
            break;
        case MODE_SUPERVISOR:
            change_mode_save_regs(gba, CPU.banked_reg_svc, &CPU.banked_spsr_svc);
            max = sizeof(CPU.banked_reg_svc) / sizeof(CPU.banked_reg_svc[0]);
            break;
        case MODE_ABORT:
            change_mode_save_regs(gba, CPU.banked_reg_abt, &CPU.banked_spsr_abt);
            max = sizeof(CPU.banked_reg_abt) / sizeof(CPU.banked_reg_abt[0]);
            break;
        case MODE_UNDEFINED:
            change_mode_save_regs(gba, CPU.banked_reg_und, &CPU.banked_spsr_und);
            max = sizeof(CPU.banked_reg_und) / sizeof(CPU.banked_reg_und[0]);
            break;
    }

    switch (new_mode)
    {
        case MODE_USER: case MODE_SYSTEM:
            change_mode_restore_regs(gba, CPU.banked_reg_usr, nullptr, max);
            break;
        case MODE_FIQ:
            change_mode_restore_regs(gba, CPU.banked_reg_fiq, &CPU.banked_spsr_fiq, -1);
            break;
        case MODE_IRQ:
            change_mode_restore_regs(gba, CPU.banked_reg_irq, &CPU.banked_spsr_irq, -1);
            break;
        case MODE_SUPERVISOR:
            change_mode_restore_regs(gba, CPU.banked_reg_svc, &CPU.banked_spsr_svc, -1);
            break;
        case MODE_ABORT:
            change_mode_restore_regs(gba, CPU.banked_reg_abt, &CPU.banked_spsr_abt, -1);
            break;
        case MODE_UNDEFINED:
            change_mode_restore_regs(gba, CPU.banked_reg_und, &CPU.banked_spsr_und, -1);
            break;
    }

    gba_log("swapped mode from: %u to %u\n", old_mode, new_mode);
}


auto get_u32_from_psr(Psr& psr) -> std::uint32_t
{
    std::uint32_t value = 0;
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

auto get_u32_from_cpsr(Gba& gba) -> std::uint32_t
{
    return get_u32_from_psr(CPU.cpsr);
}

auto get_u32_from_spsr(Gba& gba) -> std::uint32_t
{
    const auto mode = get_mode(gba);
    // std::printf("mode: %u\n", mode);
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
    }
}

auto set_psr_from_u32(Gba& gba, Psr& psr, uint32_t value, bool flag_write, bool control_write) -> void
{
    if (flag_write)
    {
        psr.N = bit::is_set<31>(value);
        psr.Z = bit::is_set<30>(value);
        psr.C = bit::is_set<29>(value);
        psr.V = bit::is_set<28>(value);
    }

    // control flags can only be changed in non-privilaged mode
    if (control_write && get_mode(gba) != MODE_USER)
    {
        psr.I = bit::is_set<7>(value);
        psr.F = bit::is_set<6>(value);
        psr.T = bit::is_set<5>(value);
        psr.M = bit::get_range<0, 4>(value);
    }
}

auto set_cpsr_from_u32(Gba& gba, uint32_t value, bool flag_write, bool control_write) -> void
{
    const auto old_mode = get_mode(gba);
    set_psr_from_u32(gba, CPU.cpsr, value, flag_write, control_write);
    const auto new_mode = get_mode(gba);
    change_mode(gba, old_mode, new_mode);
}

auto set_spsr_from_u32(Gba& gba, uint32_t value, bool flag_write, bool control_write) -> void
{
    assert(get_mode(gba) != MODE_USER && "user mode doesn't have spsr");
    if (get_mode(gba) != MODE_USER) [[likely]]
    {
        set_psr_from_u32(gba, CPU.spsr, value, flag_write, control_write);
    }
}

auto get_mode(const Gba& gba) -> std::uint8_t
{
    return CPU.cpsr.M;
}

auto get_state(const Gba& gba) -> State
{
    return static_cast<State>(CPU.cpsr.T);
}

auto get_lr(const Gba& gba) -> uint32_t
{
    return get_reg(gba, LR_INDEX);
}

auto get_sp(const Gba& gba) -> uint32_t
{
    return get_reg(gba, SP_INDEX);
}

auto get_pc(const Gba& gba) -> uint32_t
{
    return get_reg(gba, PC_INDEX);
}

auto get_reg(const Gba& gba, uint8_t reg) -> uint32_t
{
    assert(reg <= 15);
    return CPU.registers[reg];
}

auto set_lr(Gba& gba, uint32_t value) -> void
{
    set_reg(gba, LR_INDEX, value);
}

auto set_sp(Gba& gba, uint32_t value) -> void
{
    set_reg(gba, SP_INDEX, value);
}

auto set_pc(Gba& gba, uint32_t value) -> void
{
    set_reg(gba, PC_INDEX, value);
}

auto set_reg(Gba& gba, uint8_t reg, uint32_t value) -> void
{
    assert(reg <= 15);
    // todo: add asserts here for different modes
    CPU.registers[reg] = value;

    // hack for thumb, todo, fix
    if (reg == PC_INDEX) [[unlikely]]
    {
        // CPU.registers[PC_INDEX] &= ~0x1;
        // if (get_state(gba) == State::THUMB)
            CPU.registers[PC_INDEX] &= ~0x1;
        // else
            // CPU.registers[PC_INDEX] &= ~0x3;
        refill_pipeline(gba);
    }
}

// data processing manually handles refilling
auto set_reg_data_processing(Gba& gba, uint8_t reg, uint32_t value) -> void
{
    assert(reg <= 15);
    // todo: add asserts here for different modes
    CPU.registers[reg] = value;

    // hack for thumb, todo, fix
    if (reg == PC_INDEX) [[unlikely]]
    {
        // if (get_state(gba) == State::THUMB)
        // if (get_state(gba) == State::THUMB)
            CPU.registers[PC_INDEX] &= ~0x1;
        // else
            // CPU.registers[PC_INDEX] &= ~0x3;
        // CPU.registers[reg] &= ~0x1;
    }
}

auto set_reg_thumb(Gba& gba, uint8_t reg, uint32_t value) -> void
{
    assert(reg <= 7);
    CPU.registers[reg] = value;
}

auto software_interrupt(Gba& gba, std::uint8_t comment_field) -> void
{
    //toggle_breakpoint(gba, true);

    // if not handled, do normal bios handling
    if (!bios::hle(gba, comment_field))
    {
        // CPU.breakpoint = true;

        const auto pc_offset = get_state(gba) == State::THUMB ? 2 : 4;
        const auto lr = get_pc(gba) - pc_offset;
        const auto pc = 0x8; // SVC handler
        const auto cpsr = CPU.cpsr; // store cpsr into spsr

        //std::printf("swi lr: 0x%08X pc: 0x%08X\n", lr, get_pc(gba));

        change_mode(gba, get_mode(gba), MODE_SUPERVISOR);
        disable_interrupts(gba); // not sure about this
        CPU.cpsr.T = 0; // we need to be in arm
        CPU.spsr = cpsr; // stave cpsr so it can be restored later
        set_lr(gba, lr); //
        set_pc(gba, pc); // jump to SVC handler
        gba_log("in swi mode: %u\n", get_mode(gba));
    }
}

auto skip_ctr0_loop = false;

auto wallmart_debugger(Gba& gba) -> void
{
    if (CPU.breakpoint)
    {
        for (;;)
        {
            const auto c = std::getchar();

            switch (c)
            {
                case ' ': case '\n':
                    return;

                // jump to the end of crt0
                case 'j':
                    // set_pc(gba, 0x080009E0);
                    set_pc(gba, 0x0800017C);
                    return;

                case 'e':
                    CPU.breakpoint = false;
                    return;

                // skip ctr0 loop
                case 'c':
                    CPU.breakpoint = false;
                    skip_ctr0_loop = true;
                    return;

                case 'd':
                    std::exit(0);
                    break;

                default:
                    std::printf("in debug mode, press valid key: %d\n", c);
                    break;
            }
        }
    }
}

auto calc_vflag(const uint32_t a, const uint32_t b, const uint32_t r) -> bool
{
    return (bit::is_set<31>(a) == bit::is_set<31>(b)) && (bit::is_set<31>(a) != bit::is_set<31>(r));
}

auto internal_add(Gba& gba, bool S, uint32_t a, uint32_t b, bool carry) -> uint32_t
{
    const uint32_t result = a + b + carry;

    // only update cpsr if S bit was set
    if (S) [[likely]]
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = (static_cast<uint64_t>(a) + static_cast<uint64_t>(b) + carry) > UINT32_MAX;
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, b, result);
    }

    return result;
}

auto internal_sub(Gba& gba, bool S, uint32_t a, uint32_t b, bool carry) -> uint32_t
{
    const uint32_t result = a - (b + carry);

    // only update cpsr if S bit was set
    if (S) [[likely]]
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = !(a < (b + carry));
        CPU.cpsr.N = bit::is_set<31>(result);
        CPU.cpsr.V = calc_vflag(a, ~b, result);
    }

    return result;
}

auto set_logical_flags(Gba& gba, bool S, std::uint32_t result, bool carry) -> void
{
    if (S) [[likely]]
    {
        CPU.cpsr.Z = result == 0;
        CPU.cpsr.C = carry;
        CPU.cpsr.N = bit::is_set<31>(result);
    }
};

auto fire_interrupt(Gba& gba, Interrupt i) -> void
{
    REG_IF |= static_cast<std::uint16_t>(i);
    schedule_interrupt(gba);
}

auto disable_interrupts(Gba& gba) -> void
{
    CPU.cpsr.I = true; // 1=off
}

auto toggle_breakpoint(Gba& gba, bool enable) -> void
{
    CPU.breakpoint = enable;
    gba_log("\nbreakpoint hit PC: 0x%08X LR: 0x%08X SP: 0x%08X CPSR: 0x%08X mode: %u r0: 0x%08X, r1: 0x%08X, r6: 0x%08X\n", get_pc(gba) - (CPU.cpsr.T ? 2*2 : 4*2), get_lr(gba), get_sp(gba), get_u32_from_cpsr(gba), get_mode(gba), get_reg(gba, 0), get_reg(gba, 1), get_reg(gba, 6));
}

auto on_int(Gba& gba)
{
    //toggle_breakpoint(gba, true);
    // CPU.breakpoint=true;
    //printf("performing irq IE: 0x%04X IF 0x%04X lr: 0x%08X pc: 0x%08X mode: %u state: %s\n", REG_IE, REG_IF, get_lr(gba), get_pc(gba), get_mode(gba), get_state(gba) == State::THUMB ? "THUMB" : "ARM");

    const auto state = get_state(gba);
    const auto lr = get_pc(gba) + (state == State::THUMB ? 2 : 0);
    const auto pc = 0x18;

    // save cpsr
    const auto cpsr = CPU.cpsr;
    CPU.banked_spsr_irq = cpsr;
    // swap to irq mode
    change_mode(gba, get_mode(gba), MODE_IRQ);
    CPU.spsr = cpsr;
    // disable interrupts in irq mode
    disable_interrupts(gba);
    // we have to be start in arm state
    CPU.cpsr.T = static_cast<bool>(State::ARM);
    // set lr_irq to the next instruction
    set_lr(gba, lr);
    // jump to irq handler
    set_pc(gba, pc);
}

auto on_interrupt_event(Gba& gba) -> void
{
    on_int(gba);
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

auto poll_interrupts(Gba& gba) -> void
{
    if (REG_IE & REG_IF & 0b11'1111'1111'1111)
    {
        CPU.halted = false;

        if (REG_IME&1 && !CPU.cpsr.I)
        {
            on_int(gba);
        }
    }
}

auto on_halt_event(Gba& gba) -> void
{
    while (CPU.halted && gba.cycles2 < 280896)
    {
        assert(gba.scheduler.next_event_cycles >= gba.scheduler.cycles && "unsigned underflow happens!");
        gba.cycles2 += gba.scheduler.next_event_cycles - gba.scheduler.cycles;
        gba.scheduler.cycles = gba.scheduler.next_event_cycles;
        scheduler::fire(gba);
    }

    // gba.cycles = 0;
}

auto on_halt_trigger(Gba& gba, HaltType type) -> void
{
    if ((REG_IME & 1) && REG_IE && !gba.cpu.cpsr.I)
    {
        if (type == HaltType::write)
        {
            // fleroviux: I noticed that HALTCNT actually only seems accessible from BIOS code
            assert((arm7tdmi::get_pc(gba) >> 24) == 0x0 && "halt called outside bios region, let fleroviux know the game");
            return;
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
    //CPU.breakpoint=false;

    // wallmart_debugger(gba);

    #if ENABLE_SCHEDULER == 0
    poll_interrupts(gba);

    if (CPU.halted) [[unlikely]]
    {
        gba.cycles += 1;
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
