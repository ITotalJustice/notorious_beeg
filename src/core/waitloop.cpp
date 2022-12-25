// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "waitloop.hpp"
#include "gba.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "bit.hpp"
#include "mem.hpp"
#include <bit>
#include <cstring>
#include <utility>

namespace gba::waitloop {
namespace {

auto on_idle_event(void* user, s32 id = 0, s32 late = 0) -> void
{
    auto& gba = *static_cast<Gba*>(user);

    while (gba.waitloop.is_in_waitloop() && !gba.frame_end)
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

// list of valid instructions
constexpr auto is_valid(const u16 opcode) -> bool
{
    constexpr auto move_shifted_register_mask_a = 0b111'00'00000'000'000;
    constexpr auto move_shifted_register_mask_b = 0b000'00'00000'000'000;

    constexpr auto hi_register_operations_mask_a = 0b111111'00'0'0'000'000;
    constexpr auto hi_register_operations_mask_b = 0b010001'00'0'0'000'000;

    constexpr auto move_compare_add_subtract_immediate_mask_a = 0b111'00'000'00000000;
    constexpr auto move_compare_add_subtract_immediate_mask_b = 0b001'00'000'00000000;

    constexpr auto add_subtract_mask_a = 0b11111'0'0'000'000'000;
    constexpr auto add_subtract_mask_b = 0b00011'0'0'000'000'000;

    constexpr auto alu_operations_mask_a = 0b111111'0000'000'000;
    constexpr auto alu_operations_mask_b = 0b010000'0000'000'000;

    if ((opcode & hi_register_operations_mask_a) == hi_register_operations_mask_b)
    {
        return bit::get_range<8, 9>(opcode) != 0x3; // BX
    }
    else if ((opcode & move_compare_add_subtract_immediate_mask_a) == move_compare_add_subtract_immediate_mask_b)
    {
        return true;
    }
    else if ((opcode & add_subtract_mask_a) == add_subtract_mask_b)
    {
        return true;
    }
    else if ((opcode & move_shifted_register_mask_a) == move_shifted_register_mask_b)
    {
        return true;
    }
    else if ((opcode & alu_operations_mask_a) == alu_operations_mask_b)
    {
        return true;
    }

    return false;
}

constexpr auto is_cmp(const u16 opcode) -> bool
{
    constexpr auto move_compare_add_subtract_immediate_mask_a = 0b111'00'000'00000000 | (0x3 << 11);
    constexpr auto move_compare_add_subtract_immediate_mask_b = 0b001'00'000'00000000 | (0x1 << 11);

    constexpr auto hi_register_operations_mask_a = 0b111111'00'0'0'000'000 | (0x3 << 8);
    constexpr auto hi_register_operations_mask_b = 0b010001'00'0'0'000'000 | (0x1 << 8); // only allow cmp for now

    constexpr auto alu_operations_mask_a = 0b111111'0000'000'000 | (0xF << 6);
    constexpr auto alu_operations_mask_b = 0b010000'0000'000'000 | (0xA << 6);

    if ((opcode & hi_register_operations_mask_a) == hi_register_operations_mask_b)
    {
        return true;
    }
    else if ((opcode & move_compare_add_subtract_immediate_mask_a) == move_compare_add_subtract_immediate_mask_b)
    {
        return true;
    }
    else if ((opcode & alu_operations_mask_a) == alu_operations_mask_b)
    {
        return true;
    }

    return false;
}

constexpr auto get_poll_addr(const Gba& gba, const u16 opcode) -> u32
{
    constexpr auto load_store_halfword_mask_a = 0b1111'0'00000'000'000 | (1 << 11);
    constexpr auto load_store_halfword_mask_b = 0b1000'0'00000'000'000 | (1 << 11);

    constexpr auto load_store_with_immediate_offset_mask_a = 0b111'0'0'00000'000'000 | (1 << 11);
    constexpr auto load_store_with_immediate_offset_mask_b = 0b011'0'0'00000'000'000 | (1 << 11);

    constexpr auto load_store_with_register_offset_mask_a = 0b1111'0'0'1'000'000'000 | (1 << 11);
    constexpr auto load_store_with_register_offset_mask_b = 0b0101'0'0'0'000'000'000 | (1 << 11);

    constexpr auto load_store_sign_extended_byte_halfword_mask_a = 0b1111'0'0'1'000'000'000 | (1 << 11);
    constexpr auto load_store_sign_extended_byte_halfword_mask_b = 0b0101'0'0'1'000'000'000 | (1 << 11);

    // only scan select few instructions because these are by far the most
    // commonly used, and speeds up decoding.
    if ((opcode & load_store_halfword_mask_a) == load_store_halfword_mask_b)
    {
        const auto Rb = bit::get_range<3, 5>(opcode);
        const auto offset = bit::get_range<6, 10>(opcode) << 1; // 6bit
        const auto base = arm7tdmi::get_reg(gba, Rb);
        return base + offset;
    }
    else if ((opcode & load_store_with_immediate_offset_mask_a) == load_store_with_immediate_offset_mask_b)
    {
        const auto Rb = bit::get_range<3, 5>(opcode);
        const auto offset = bit::get_range<6, 10>(opcode);
        const auto base = arm7tdmi::get_reg(gba, Rb);

        if (bit::is_set<12>(opcode)) // byte
        {
            return base + offset;
        }
        else // word
        {
            return base + (offset << 2);
        }
    }
    else if ((opcode & load_store_with_register_offset_mask_a) == load_store_with_register_offset_mask_b)
    {
        const auto Ro = bit::get_range<6, 8>(opcode);
        const auto Rb = bit::get_range<3, 5>(opcode);
        const auto base = arm7tdmi::get_reg(gba, Rb);
        const auto offset = arm7tdmi::get_reg(gba, Ro);
        return (base + offset) & ~0x1;
    }
    else if ((opcode & load_store_sign_extended_byte_halfword_mask_a) == load_store_sign_extended_byte_halfword_mask_b)
    {
        const auto Ro = bit::get_range<6, 8>(opcode);
        const auto Rb = bit::get_range<3, 5>(opcode);
        const auto base = arm7tdmi::get_reg(gba, Rb);
        const auto offset = arm7tdmi::get_reg(gba, Ro);
        return (base + offset) & ~0x1;
    }

    return 0xFFFFFFFF;
}

inline auto read16(const u8* array, u32 addr) -> u16
{
    u16 data;
    std::memcpy(&data, array + addr, sizeof(data));

    if constexpr(std::endian::native == std::endian::big)
    {
        return std::byteswap(data);
    }

    return data;
}

} // namespace

// CASTLEVANIA: https://discord.com/channels/465585922579103744/472481254911115266/1037621944591323146
// TESTROM: https://discord.com/channels/465585922579103744/472481254911115266/1037612636898066522
// POKEMON: https://discord.com/channels/465585922579103744/472481254911115266/1037753629534343199
auto Waitloop::evaluate_loop_step1(Gba& gba, const u32 current_pc, u32 new_jump_pc) -> bool
{
    // checks that the pc jumps back and that the pc is within rom range
    if (current_pc <= new_jump_pc || new_jump_pc < 0x8000000 || new_jump_pc > 0x8FFFFFF)
    {
        return false;
    }

    const auto length = current_pc - new_jump_pc;

    if (length > 0xE)
    {
        return false;
    }

    new_jump_pc &= mem::ROM_MASK;
    const auto first_opcode = read16(gba.rom, new_jump_pc);
    const auto last_opcode = read16(gba.rom, (new_jump_pc + length) - 6);

    poll_address = get_poll_addr(gba, first_opcode);
    poll_address &= mem::align<u16>(poll_address);

    if (poll_address == 0xFFFFFFFF)
    {
        return false;
    }
    #if 1
    if (!is_cmp(last_opcode))
    #else
    if (!is_valid(last_opcode)) // this works but often has false positives
    #endif
    {
        return false;
    }

    switch ((poll_address >> 24) & 0xF)
    {
        case 0x2: // EWRAM
        case 0x3: // IWRAM
        case 0x5: // pram
        case 0x6: // vram
        case 0x7: // oam
            break;

        case 0x4:
            // todo: support polling of more io registers
            switch (poll_address)
            {
                case mem::IO_VCOUNT:
                case mem::IO_DISPSTAT:
                case mem::IO_DMA0CNT_L:
                case mem::IO_DMA0CNT_H:
                case mem::IO_DMA1CNT_L:
                case mem::IO_DMA1CNT_H:
                case mem::IO_DMA2CNT_L:
                case mem::IO_DMA2CNT_H:
                case mem::IO_DMA3CNT_L:
                case mem::IO_DMA3CNT_H:
                    break;

                default:
                    // std::printf("IO[0x%08X] waitloop\n", poll_address);
                    return false;
            }
            break;

        default:
            return false;
    }

    if (length == 0x8) // 3 instructions, 0 to evaluate
    {
        return true;
    }
    else if (length == 0xA) // 4 instructions, 1 to evaluate
    {
        const auto second_opcode = read16(gba.rom, (new_jump_pc + 2));
        return is_valid(second_opcode);
    }
    else if (length == 0xC) // 5 instructions, 2 to evaluate
    {
        const auto second_opcode = read16(gba.rom, (new_jump_pc + 2));
        const auto third_opcode = read16(gba.rom, (new_jump_pc + 4));
        return is_valid(second_opcode) && is_valid(third_opcode);
    }
    else if (length == 0xE) // 6 instructions, 3 to evaluate
    {
        const auto second_opcode = read16(gba.rom, (new_jump_pc + 2));
        const auto third_opcode = read16(gba.rom, (new_jump_pc + 4));
        const auto forth_opcode = read16(gba.rom, (new_jump_pc + 6));
        return is_valid(second_opcode) && is_valid(third_opcode) && is_valid(forth_opcode);
    }

    std::unreachable();
}

// on the second step, check if the registers change, if true
auto Waitloop::evaluate_loop_step2(Gba& gba, const u32 current_pc, const u32 new_jump_pc) -> bool
{
    return 0 == std::memcmp(wait_loop_registers, gba.cpu.registers, sizeof(wait_loop_registers));
}

void Waitloop::evaluate_loop(Gba& gba, const u32 current_pc, const u32 new_jump_pc)
{
    switch (step)
    {
        case WAITLOOP_STEP_1:
            if (evaluate_loop_step1(gba, current_pc, new_jump_pc))
            {
                std::memcpy(wait_loop_registers, gba.cpu.registers, sizeof(wait_loop_registers));
                step = WAITLOOP_STEP_2;
            }
            else
            {
                step = WAITLOOP_STEP_INVALID;
            }
            break;

        case WAITLOOP_STEP_2:
            if (evaluate_loop_step2(gba, current_pc, new_jump_pc))
            {
                if (event_changed)
                {
                    step = WAITLOOP_STEP_2;
                    event_changed = false;
                }
                else
                {
                    gba.waitloop.in_waitloop = true;
                    gba.scheduler.add(scheduler::ID::IDLE_LOOP, 0, on_idle_event, &gba);
                    step = WAITLOOP_STEP_1;
                }
            }
            else
            {
                // std::printf("[WAITLOOP] failed step 2: 0x%08X\n", new_jump_pc);
                step = WAITLOOP_STEP_INVALID;
            }
            break;

        case WAITLOOP_STEP_INVALID:
            break;
    }
}

void Waitloop::on_thumb_loop(Gba& gba, const u32 current_pc, const u32 new_jump_pc)
{
    if (!is_enabled()) [[unlikely]]
    {
        return;
    }

    if (pc != new_jump_pc)
    {
        pc = new_jump_pc;
        step = WAITLOOP_STEP_1;
    }

    evaluate_loop(gba, current_pc, new_jump_pc);
}

void Waitloop::on_event_change(Gba& gba, const WaitloopEvent event, const u32 addr_start, const u32 addr_end)
{
    if (is_in_waitloop())
    {
        switch (event)
        {
            // and irq instantly exits the loop as the pc is changed
            case WAITLOOP_EVENT_IRQ:
                in_waitloop = false;
                break;

            // check if the dma was in range of the address that is polled.
            case WAITLOOP_EVENT_DMA:
                // dma addr was incremented
                if (addr_start < addr_end)
                {
                    if (poll_address >= addr_start && poll_address <= addr_end)
                    {
                        in_waitloop = false;
                    }
                }
                // otherwise the addr was decremented
                else
                {
                    if (poll_address >= addr_end && poll_address <= addr_start)
                    {
                        in_waitloop = false;
                    }
                }
                break;

            case WAITLOOP_EVENT_IO:
                if (addr_start == poll_address)
                {
                    in_waitloop = false;
                }
                break;
        }
    }
    else
    {
        event_changed = true;
    }
}

void Waitloop::reset(Gba& gba, const bool enable)
{
    pc = 0;
    poll_address = 0;
    std::memset(wait_loop_registers, 0, sizeof(wait_loop_registers));
    step = WAITLOOP_STEP_1;
    in_waitloop = false;
    event_changed = false;
    enabled = enable;
}

} // namespace gba::waitloop
