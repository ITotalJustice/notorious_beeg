#include "gb.hpp"
#include "internal.hpp"

#include <cassert>
#include <cstring>
#include <utility>

#include "gba.hpp"
#include "scheduler.hpp"

namespace gba::gb {
namespace {

constexpr u8 CYCLE_TABLE[0x100] = {
	4,12,8,8,4,4,8,4,20,8,8,8,4,4,8,4,4,
	12,8,8,4,4,8,4,12,8,8,8,4,4,8,4,8,
	12,8,8,4,4,8,4,8,8,8,8,4,4,8,4,8,
	12,8,8,12,12,12,4,8,8,8,8,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,8,
	8,8,8,8,8,4,8,4,4,4,4,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,4,
	4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,8,
	12,12,16,12,16,8,16,8,16,12,4,12,24,8,16,8,
	12,12,0,12,16,8,16,8,16,12,0,12,0,8,16,12,
	12,8,0,0,16,8,16,16,4,16,0,0,0,8,16,12,
	12,8,4,0,16,8,16,12,8,16,4,0,0,8,16,
};

constexpr u8 CYCLE_TABLE_CB[0x100] = {
	8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,8,
	8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,8,
	8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,8,
	8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,8,
	8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,
};


enum FlagMasks {
    FLAG_C_MASK = 0x10,
    FLAG_H_MASK = 0x20,
    FLAG_N_MASK = 0x40,
    FLAG_Z_MASK = 0x80,
};

// flag getters
#define FLAG_C gba.gameboy.cpu.c
#define FLAG_H gba.gameboy.cpu.h
#define FLAG_N gba.gameboy.cpu.n
#define FLAG_Z gba.gameboy.cpu.z

// flag setters
#define SET_FLAG_C(value) gba.gameboy.cpu.c = !!(value)
#define SET_FLAG_H(value) gba.gameboy.cpu.h = !!(value)
#define SET_FLAG_N(value) gba.gameboy.cpu.n = !!(value)
#define SET_FLAG_Z(value) gba.gameboy.cpu.z = !!(value)

// reg array, indexed by decoding the opcode in most cases
#define REG(v) gba.gameboy.cpu.registers[(v) & 0x7]

// SINGLE REGS
#define REG_B REG(0)
#define REG_C REG(1)
#define REG_D REG(2)
#define REG_E REG(3)
#define REG_H REG(4)
#define REG_L REG(5)
#define REG_A REG(7)

// REG_F getter
#define REG_F_GET() \
    ((FLAG_Z << 7) | (FLAG_N << 6) | (FLAG_H << 5) | (FLAG_C << 4))

// REG_F setter
#define REG_F_SET(v) \
    SET_FLAG_Z((v) & FLAG_Z_MASK); \
    SET_FLAG_N((v) & FLAG_N_MASK); \
    SET_FLAG_H((v) & FLAG_H_MASK); \
    SET_FLAG_C((v) & FLAG_C_MASK)

// getters
#define REG_BC ((REG_B << 8) | REG_C)
#define REG_DE ((REG_D << 8) | REG_E)
#define REG_HL ((REG_H << 8) | REG_L)
#define REG_AF ((REG_A << 8) | REG_F_GET())

// setters
#define SET_REG_BC(v) REG_B = (((v) >> 8) & 0xFF); REG_C = ((v) & 0xFF)
#define SET_REG_DE(v) REG_D = (((v) >> 8) & 0xFF); REG_E = ((v) & 0xFF)
#define SET_REG_HL(v) REG_H = (((v) >> 8) & 0xFF); REG_L = ((v) & 0xFF)
#define SET_REG_AF(v) REG_A = (((v) >> 8) & 0xFF); REG_F_SET(v)

// getters / setters
#define REG_SP gba.gameboy.cpu.SP
#define REG_PC gba.gameboy.cpu.PC


#define SET_FLAGS_HN(h,n) \
    SET_FLAG_H(h); \
    SET_FLAG_N(n);

#define SET_FLAGS_HZ(h,z) \
    SET_FLAG_H(h); \
    SET_FLAG_Z(z);

#define SET_FLAGS_HNZ(h,n,z) \
    SET_FLAG_H(h); \
    SET_FLAG_N(n); \
    SET_FLAG_Z(z);

#define SET_FLAGS_CHN(c,h,n) \
    SET_FLAG_C(c); \
    SET_FLAG_H(h); \
    SET_FLAG_N(n);

#define SET_ALL_FLAGS(c,h,n,z) \
    SET_FLAG_C(c); \
    SET_FLAG_H(h); \
    SET_FLAG_N(n); \
    SET_FLAG_Z(z);

#define read8(addr) read8(gba, addr)
#define read16(addr) read16(gba, addr)
#define write8(addr,value) write8(gba, addr, value)
#define write16(addr,value) write16(gba, addr, value)

inline void PUSH(Gba& gba, u16 value)
{
    write8(--REG_SP, (value >> 8) & 0xFF);
    write8(--REG_SP, value & 0xFF);
}

inline auto POP(Gba& gba) -> u16
{
    const u16 result = read16(REG_SP);
    REG_SP += 2;
    return result;
}

#define PUSH(value) PUSH(gba, value)
#define POP() POP(gba)

#define CALL() do { \
    const u16 result = read16(REG_PC); \
    PUSH(REG_PC + 2); \
    REG_PC = result; \
} while(0)

#define CALL_NZ() do { \
    if (!FLAG_Z) { \
        CALL(); \
        gba.gameboy.cycles += 12; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define CALL_Z() do { \
    if (FLAG_Z) { \
        CALL(); \
        gba.gameboy.cycles += 12; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define CALL_NC() do { \
    if (!FLAG_C) { \
        CALL(); \
        gba.gameboy.cycles += 12; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define CALL_C() do { \
    if (FLAG_C) { \
        CALL(); \
        gba.gameboy.cycles += 12; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define JP() do { \
    REG_PC = read16(REG_PC); \
} while(0)

#define JP_HL() do { REG_PC = REG_HL; } while(0)

#define JP_NZ() do { \
    if (!FLAG_Z) { \
        JP(); \
        gba.gameboy.cycles += 4; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define JP_Z() do { \
    if (FLAG_Z) { \
        JP(); \
        gba.gameboy.cycles += 4; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define JP_NC() do { \
    if (!FLAG_C) { \
        JP(); \
        gba.gameboy.cycles += 4; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define JP_C() do { \
    if (FLAG_C) { \
        JP(); \
        gba.gameboy.cycles += 4; \
    } else { \
        REG_PC += 2; \
    } \
} while(0)

#define JR() do { \
    REG_PC += ((int8_t)read8(REG_PC)) + 1; \
} while(0)

#define JR_NZ() do { \
    if (!FLAG_Z) { \
        JR(); \
        gba.gameboy.cycles += 4; \
    } else { \
        ++REG_PC; \
    } \
} while(0)

#define JR_Z() do { \
    if (FLAG_Z) { \
        JR(); \
        gba.gameboy.cycles += 4; \
    } else { \
        ++REG_PC; \
    } \
} while(0)

#define JR_NC() do { \
    if (!FLAG_C) { \
        JR(); \
        gba.gameboy.cycles += 4; \
    } else { \
        ++REG_PC; \
    } \
} while(0)

#define JR_C() do { \
    if (FLAG_C) { \
        JR(); \
        gba.gameboy.cycles += 4; \
    } else { \
        ++REG_PC; \
    } \
} while(0)

#define RET() do { \
    REG_PC = POP(); \
} while(0)

#define RET_NZ() do { \
    if (!FLAG_Z) { \
        RET(); \
        gba.gameboy.cycles += 12; \
    } \
} while(0)

#define RET_Z() do { \
    if (FLAG_Z) { \
        RET(); \
        gba.gameboy.cycles += 12; \
    } \
} while(0)

#define RET_NC() do { \
    if (!FLAG_C) { \
        RET(); \
        gba.gameboy.cycles += 12; \
    } \
} while(0)

#define RET_C() do { \
    if (FLAG_C) { \
        RET(); \
        gba.gameboy.cycles += 12; \
    } \
} while(0)

#define INC_r() do { \
    REG((opcode >> 3))++; \
    SET_FLAGS_HNZ(((REG((opcode >> 3)) & 0xF) == 0), false, REG((opcode >> 3)) == 0); \
} while(0)

#define INC_HLa() do { \
    const u8 result = read8(REG_HL) + 1; \
    write8(REG_HL, result); \
    SET_FLAGS_HNZ((result & 0xF) == 0, false, result == 0); \
} while (0)

#define DEC_r() do { \
    --REG((opcode >> 3)); \
    SET_FLAGS_HNZ(((REG((opcode >> 3)) & 0xF) == 0xF), true, REG((opcode >> 3)) == 0); \
} while(0)

#define DEC_HLa() do { \
    const u8 result = read8(REG_HL) - 1; \
    write8(REG_HL, result); \
    SET_FLAGS_HNZ((result & 0xF) == 0xF, true, result == 0); \
} while(0)

void sprite_ram_bug(Gba& gba, u8 v)
{
    /* bug: if ppu in mode2 and H = FE, oam is corrupt */
    /* SOURCE: numism.gb */
    /* this ONLY happens on DMG */
    /* SOURCE: https://gbdev.io/pandocs/OAM_Corruption_Bug.html */
    /* TODO: does this happen on agb when playing dmg game? */
    #if 0
    if (v == 0xFE && !is_system_gbc(gba))
    {
        if (get_status_mode(gba) == 2)
        {
            std::memset(gba.gameboy.oam + 0x4, 0xFF, 0xA0 - 0x4);
            gb_log("INC_HL oam corrupt bug!\n");
        }
    }
    #endif
}

#define INC_BC() do { \
    sprite_ram_bug(gba, REG_B); \
    SET_REG_BC(REG_BC + 1); \
} while(0)
#define INC_DE() do { \
    sprite_ram_bug(gba, REG_D); \
    SET_REG_DE(REG_DE + 1); \
} while(0)
#define INC_HL() do { \
    sprite_ram_bug(gba, REG_H); \
    SET_REG_HL(REG_HL + 1); \
} while(0)

#define DEC_BC() do { \
    sprite_ram_bug(gba, REG_B); \
    SET_REG_BC(REG_BC - 1); \
} while(0)
#define DEC_DE() do { \
    sprite_ram_bug(gba, REG_D); \
    SET_REG_DE(REG_DE - 1); \
} while(0)
#define DEC_HL() do { \
    sprite_ram_bug(gba, REG_H); \
    SET_REG_HL(REG_HL - 1); \
} while(0)

#define INC_SP() do { ++REG_SP; } while(0)
#define DEC_SP() do { --REG_SP; } while(0)

#define LD_r_r() do { REG(opcode >> 3) = REG(opcode); } while(0)
#define LD_r_u8() do { REG(opcode >> 3) = read8(REG_PC++); } while(0)

#define LD_HLa_r() do { write8(REG_HL, REG(opcode)); } while(0)
#define LD_HLa_u8() do { write8(REG_HL, read8(REG_PC++)); } while(0)

#define LD_r_HLa() do { REG(opcode >> 3) = read8(REG_HL); } while(0)
#define LD_SP_u16() do { REG_SP = read16(REG_PC); REG_PC+=2; } while(0)

#define LD_A_u16() do { REG_A = read8(read16(REG_PC)); REG_PC+=2; } while(0)
#define LD_u16_A() do { write8(read16(REG_PC), REG_A); REG_PC+=2; } while(0)

#define LD_HLi_A() do { write8(REG_HL, REG_A); INC_HL(); } while(0)
#define LD_HLd_A() do { write8(REG_HL, REG_A); DEC_HL(); } while(0)

#define LD_A_HLi() do { REG_A = read8(REG_HL); INC_HL(); } while(0)
#define LD_A_HLd() do { REG_A = read8(REG_HL); DEC_HL(); } while(0)

#define LD_A_BCa() do { REG_A = read8(REG_BC); } while(0)
#define LD_A_DEa() do { REG_A = read8(REG_DE); } while(0)
#define LD_A_HLa() do { REG_A = read8(REG_HL); } while(0)

#define LD_BCa_A() do { write8(REG_BC, REG_A); } while(0)
#define LD_DEa_A() do { write8(REG_DE, REG_A); } while(0)

#define LD_FFRC_A() do { ffwrite8(gba, REG_C, REG_A); } while(0)
#define LD_A_FFRC() do { REG_A = ffread8(gba, REG_C); } while(0)

#define LD_BC_u16() do { \
    const u16 result = read16(REG_PC); \
    SET_REG_BC(result); \
    REG_PC += 2; \
} while(0)

#define LD_DE_u16() do { \
    const u16 result = read16(REG_PC); \
    SET_REG_DE(result); \
    REG_PC += 2; \
} while(0)

#define LD_HL_u16() do { \
    const u16 result = read16(REG_PC); \
    SET_REG_HL(result); \
    REG_PC += 2; \
} while(0)

#define LD_u16_SP() do { write16(read16(REG_PC), REG_SP); REG_PC+=2; } while(0)

#define LD_SP_HL() do { REG_SP = REG_HL; } while(0)

#define LD_FFu8_A() do { ffwrite8(gba, read8(REG_PC++), REG_A); } while(0)
#define LD_A_FFu8() do { REG_A = ffread8(gba, read8(REG_PC++)); } while(0)

#define CP_r() do { \
    const u8 value = REG(opcode); \
    const u8 result = REG_A - value; \
    SET_ALL_FLAGS(value > REG_A, (REG_A & 0xF) < (value & 0xF), true, result == 0); \
} while(0)

#define CP_u8() do { \
    const u8 value = read8(REG_PC++); \
    const u8 result = REG_A - value; \
    SET_ALL_FLAGS(value > REG_A, (REG_A & 0xF) < (value & 0xF), true, result == 0); \
} while(0)

#define CP_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = REG_A - value; \
    SET_ALL_FLAGS(value > REG_A, (REG_A & 0xF) < (value & 0xF), true, result == 0); \
} while(0)

#define ADD_INTERNAL(value, carry) do { \
    const u8 result = REG_A + value + carry; \
    SET_ALL_FLAGS((REG_A + value + carry) > 0xFF, ((REG_A & 0xF) + (value & 0xF) + carry) > 0xF, false, result == 0); \
    REG_A = result; \
} while (0)

#define ADD_r() do { \
    ADD_INTERNAL(REG(opcode), false); \
} while(0)

#define ADD_u8() do { \
    const u8 value = read8(REG_PC++); \
    ADD_INTERNAL(value, false); \
} while(0)

#define ADD_HLa() do { \
    const u8 value = read8(REG_HL); \
    ADD_INTERNAL(value, false); \
} while(0)

#define ADD_HL_INTERNAL(value) do { \
    const u16 result = REG_HL + value; \
    SET_FLAGS_CHN((REG_HL + value) > 0xFFFF, (REG_HL & 0xFFF) + (value & 0xFFF) > 0xFFF, false); \
    SET_REG_HL(result); \
} while(0)

#define ADD_HL_BC() do { ADD_HL_INTERNAL(REG_BC); } while(0)
#define ADD_HL_DE() do { ADD_HL_INTERNAL(REG_DE); } while(0)
#define ADD_HL_HL() do { ADD_HL_INTERNAL(REG_HL); } while(0)
#define ADD_HL_SP() do { ADD_HL_INTERNAL(REG_SP); } while(0)

#define ADD_SP_i8() do { \
    const u8 value = read8(REG_PC++); \
    const u16 result = REG_SP + (int8_t)value; \
    SET_ALL_FLAGS(((REG_SP & 0xFF) + value) > 0xFF, ((REG_SP & 0xF) + (value & 0xF)) > 0xF, false, false); \
    REG_SP = result; \
} while (0)

#define LD_HL_SP_i8() do { \
    const u8 value = read8(REG_PC++); \
    const u16 result = REG_SP + (int8_t)value; \
    SET_ALL_FLAGS(((REG_SP & 0xFF) + value) > 0xFF, ((REG_SP & 0xF) + (value & 0xF)) > 0xF, false, false); \
    SET_REG_HL(result); \
} while (0)

#define ADC_r() do { \
    const bool fc = FLAG_C; \
    ADD_INTERNAL(REG(opcode), fc); \
} while(0)

#define ADC_u8() do { \
    const u8 value = read8(REG_PC++); \
    const bool fc = FLAG_C; \
    ADD_INTERNAL(value, fc); \
} while(0)

#define ADC_HLa() do { \
    const u8 value = read8(REG_HL); \
    const bool fc = FLAG_C; \
    ADD_INTERNAL(value, fc); \
} while(0)

#define SUB_INTERNAL(value, carry) do { \
    const u8 result = REG_A - value - carry; \
    SET_ALL_FLAGS((value + carry) > REG_A, (REG_A & 0xF) < ((value & 0xF) + carry), true, result == 0); \
    REG_A = result; \
} while (0)

#define SUB_r() do { \
    SUB_INTERNAL(REG(opcode), false); \
} while(0)

#define SUB_u8() do { \
    const u8 value = read8(REG_PC++); \
    SUB_INTERNAL(value, false); \
} while(0)

#define SUB_HLa() do { \
    const u8 value = read8(REG_HL); \
    SUB_INTERNAL(value, false); \
} while(0)

#define SBC_r() do { \
    const bool fc = FLAG_C; \
    SUB_INTERNAL(REG(opcode), fc); \
} while(0)

#define SBC_u8() do { \
    const u8 value = read8(REG_PC++); \
    const bool fc = FLAG_C; \
    SUB_INTERNAL(value, fc); \
} while(0)

#define SBC_HLa() do { \
    const u8 value = read8(REG_HL); \
    const bool fc = FLAG_C; \
    SUB_INTERNAL(value, fc); \
} while(0)

#define AND_r() do { \
    REG_A &= REG(opcode); \
    SET_ALL_FLAGS(false, true, false, REG_A == 0); \
} while(0)

#define AND_u8() do { \
    REG_A &= read8(REG_PC++); \
    SET_ALL_FLAGS(false, true, false, REG_A == 0); \
} while(0)

#define AND_HLa() do { \
    REG_A &= read8(REG_HL); \
    SET_ALL_FLAGS(false, true, false, REG_A == 0); \
} while(0)

#define XOR_r() do { \
    REG_A ^= REG(opcode); \
    SET_ALL_FLAGS(false, false, false, REG_A == 0); \
} while(0)

#define XOR_u8() do { \
    REG_A ^= read8(REG_PC++); \
    SET_ALL_FLAGS(false, false, false, REG_A == 0); \
} while(0)

#define XOR_HLa() do { \
    REG_A ^= read8(REG_HL); \
    SET_ALL_FLAGS(false, false, false, REG_A == 0); \
} while(0)

#define OR_r() do { \
    REG_A |= REG(opcode); \
    SET_ALL_FLAGS(false, false, false, REG_A == 0); \
} while(0)

#define OR_u8() do { \
    REG_A |= read8(REG_PC++); \
    SET_ALL_FLAGS(false, false, false, REG_A == 0); \
} while(0)

#define OR_HLa() do { \
    REG_A |= read8(REG_HL); \
    SET_ALL_FLAGS(false, false, false, REG_A == 0); \
} while(0)

#define DI() do { gba.gameboy.cpu.ime = false; } while(0)
#if !USE_SCHED
#define EI() do { gba.gameboy.cpu.ime_delay = true; } while(0)
#else
#define EI() do { \
    gba.gameboy.cpu.ime_delay = true; \
    gba.scheduler.add(scheduler::ID::INTERRUPT, 0, on_interrupt_event, &gba); \
} while(0)
#endif

#define POP_BC() do { \
    const u16 result = POP(); \
    SET_REG_BC(result); \
} while(0)

#define POP_DE() do { \
    const u16 result = POP(); \
    SET_REG_DE(result); \
} while(0)

#define POP_HL() do { \
    const u16 result = POP(); \
    SET_REG_HL(result); \
} while(0)

#define POP_AF() do { \
    const u16 result = POP(); \
    SET_REG_AF(result); \
} while(0)

#define RL_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) << 1) | (FLAG_C); \
    SET_ALL_FLAGS(value >> 7, false, false, REG(opcode) == 0); \
} while(0)

#define RLA() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) << 1) | (FLAG_C); \
    SET_ALL_FLAGS(value >> 7, false, false, false); \
} while(0)

#define RL_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value << 1) | (FLAG_C); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value >> 7, false, false, result == 0); \
} while (0)

#define RLC_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) << 1) | ((REG(opcode) >> 7) & 1); \
    SET_ALL_FLAGS(value >> 7, false, false, REG(opcode) == 0); \
} while(0)

#define RLC_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value << 1) | ((value >> 7) & 1); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value >> 7, false, false, result == 0); \
} while(0)

#define RLCA() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) << 1) | ((REG(opcode) >> 7) & 1); \
    SET_ALL_FLAGS(value >> 7, false, false, false); \
} while(0)

#define RR_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) >> 1) | (FLAG_C << 7); \
    SET_ALL_FLAGS(value & 1, false, false, REG(opcode) == 0); \
} while(0)

#define RR_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value >> 1) | (FLAG_C << 7); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value & 1, false, false, result == 0); \
} while(0)

#define RRA() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) >> 1) | (FLAG_C << 7); \
    SET_ALL_FLAGS(value & 1, false, false, false); \
} while(0)

#define RRC_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) >> 1) | (REG(opcode) << 7); \
    SET_ALL_FLAGS(value & 1, false, false, REG(opcode) == 0); \
} while(0)

#define RRCA() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) >> 1) | (REG(opcode) << 7); \
    SET_ALL_FLAGS(value & 1, false, false, false); \
} while(0)

#define RRC_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value >> 1) | (value << 7); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value & 1, false, false, result == 0); \
} while (0)

#define SLA_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) <<= 1; \
    SET_ALL_FLAGS(value >> 7, false, false, REG(opcode) == 0); \
} while(0)

#define SLA_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = value << 1; \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value >> 7, false, false, result == 0); \
} while(0)

#define SRA_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) = (REG(opcode) >> 1) | (REG(opcode) & 0x80); \
    SET_ALL_FLAGS(value & 1, false, false, REG(opcode) == 0); \
} while(0)

#define SRA_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value >> 1) | (value & 0x80); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value & 1, false, false, result == 0); \
} while(0)

#define SRL_r() do { \
    const u8 value = REG(opcode); \
    REG(opcode) >>= 1; \
    SET_ALL_FLAGS(value & 1, false, false, REG(opcode) == 0); \
} while(0)

#define SRL_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value >> 1); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(value & 1, false, false, result == 0); \
} while(0)

#define SWAP_r() do { \
    REG(opcode) = (REG(opcode) << 4) | (REG(opcode) >> 4); \
    SET_ALL_FLAGS(false, false, false, REG(opcode) == 0); \
} while(0)

#define SWAP_HLa() do { \
    const u8 value = read8(REG_HL); \
    const u8 result = (value << 4) | (value >> 4); \
    write8(REG_HL, result); \
    SET_ALL_FLAGS(false, false, false, result == 0); \
} while(0)

#define BIT_r() do { \
    SET_FLAGS_HNZ(true, false, (REG(opcode) & (1 << ((opcode >> 3) & 0x7))) == 0); \
} while(0)

#define BIT_HLa() do { \
    SET_FLAGS_HNZ(true, false, (read8(REG_HL) & (1 << ((opcode >> 3) & 0x7))) == 0); \
} while(0)

#define RES_r() do { \
    REG(opcode) &= ~(1 << ((opcode >> 3) & 0x7)); \
} while(0)

#define RES_HLa() do { \
    write8(REG_HL, (read8(REG_HL)) & ~(1 << ((opcode >> 3) & 0x7))); \
} while(0)

#define SET_r() do { \
    REG(opcode) |= (1 << ((opcode >> 3) & 0x7)); \
} while(0)

#define SET_HLa() do { \
    write8(REG_HL, (read8(REG_HL)) | (1 << ((opcode >> 3) & 0x7))); \
} while(0)

#define DAA() do { \
    if (FLAG_N) { \
        if (FLAG_C) { \
            REG_A -= 0x60; \
            SET_FLAG_C(true); \
        } \
        if (FLAG_H) { \
            REG_A -= 0x6; \
        } \
    } else { \
        if (FLAG_C || REG_A > 0x99) { \
            REG_A += 0x60; \
            SET_FLAG_C(true); \
        } \
        if (FLAG_H || (REG_A & 0x0F) > 0x09) { \
            REG_A += 0x6; \
        } \
    } \
    SET_FLAGS_HZ(false, REG_A == 0); \
} while(0)

#define RETI() do { \
    REG_PC = POP(); \
    gba.gameboy.cpu.ime = true; /* not delayed! */ \
    schedule_interrupt(gba); \
} while (0)

#define RST(value) do { \
    PUSH(REG_PC); \
    REG_PC = value; \
} while (0)

#define CPL() do { REG_A = ~REG_A; SET_FLAGS_HN(true, true); } while(0)
#define SCF() do { SET_FLAGS_CHN(true, false, false); } while (0)
#define CCF() do { SET_FLAGS_CHN(FLAG_C ^ 1, false, false); } while(0)

inline void HALT(Gba& gba)
{
    if (gba.gameboy.cpu.ime)
    {
        // normal halt
        gba.gameboy.cpu.halt = true;
        gba.scheduler.add(scheduler::ID::HALT, 0, on_halt_event, &gba);
    }
    else
    {
        if (GB_IO_IF & GB_IO_IE & 0x1F)
        {
            gba.gameboy.cpu.halt_bug = true;
        }
        else
        {
            assert(GB_IO_IE && "never ending halt");
            gba.gameboy.cpu.halt = true;
            gba.scheduler.add(scheduler::ID::HALT, 0, on_halt_event, &gba);
        }
    }
}

void STOP(Gba& gba)
{
    if (is_system_gbc(gba))
    {
        // only set if speed-switch is requested
        if (IO_KEY1 & 0x1)
        {
            gb_log("changing speed mode");

            // switch speed state.
            gba.gameboy.cpu.double_speed = !gba.gameboy.cpu.double_speed;
            // this clears bit-0 and sets bit-7 to whether we are in double
            // or normal speed mode.
            IO_KEY1 = (gba.gameboy.cpu.double_speed << 7);
            // STOP does take a lot of time, but this isn't the correct
            // amount!
            // this value was needed in order to pass:
            // - cpu_instrs/03-op sp,hl
            // - cpu_instrs/06-ld r,r
            // - cpu_instrs/11-op a,(hl)
            // the tests themselves actually pass but the output isn't
            // correctly rendered to the screen as vram access is locked
            // in ppu mode3.
            // cpu_instrs/03-op sp,hl on cgb doesnt render the "passed!"
            gba.gameboy.cycles += 636;
        }

        if (!gba.gameboy.cpu.double_speed)
        {
            // printf("double speed mode was disabled!\n");
        }
    }

    // TODO: still left to handle:
    // - stop mode in dmg
    // - proper stop mode usage where it effectively goes into halt
    //   until a button is pressed.
    // - handle stop mode when a button is already pressed.
    const auto has_interrupt = GB_IO_IE & GB_IO_IF;
    const auto has_ime = gba.gameboy.cpu.ime | gba.gameboy.cpu.ime_delay;
    bool reset_div = true;
    bool increment_pc = true;

    if (!has_interrupt)
    {
        // enter halt mode only if able to
        if (GB_IO_IE)
        {
            HALT(gba);
        }
        // unable to enter halt mode, cpu will automatically leave
        // halt mode after 0x20000 cycles.
        else
        {
            // to similaute the above, fast forward the ppu
            // TODO: properly handle this!
            // NOTE: cpu_instr 06 triggers this case, if not handled
            // then it breaks text rendering as it tries to write
            // to vram whilst the ppu is in mode3
            // if (is_lcd_enabled(gba))
            // {
            //     printf("mode: %u ly: %u ly-wrapped: %u\n", get_status_mode(gba), IO_LY, (IO_LY + 287) % 154);
            //     IO_LY = (IO_LY + 287) % 154;
            // }
        }
    }
    else
    {
        if (has_ime)
        {
            // idk what happens here
            assert(!"stop edge case hit edge");
        }
        else
        {
            increment_pc = false;
        }
    }

    if (reset_div)
    {
        ffwrite8(gba, 0x04, 0);
    }

    if (increment_pc)
    {
        REG_PC++; // skip next byte
    }
}

void UNK_OP(Gba& gba, u8 opcode, bool cb_prefix)
{
    (void)gba; (void)opcode; (void)cb_prefix;

    if (cb_prefix)
    {
        gb_log("unk instruction: 0xCB%02X\n", opcode);
        assert(!"unknown CB instruction");
    }
    else
    {
        gb_log("unk instruction: 0x%02X\n", opcode);
        assert(!"unknown instruction");
    }
}

inline void interrupt_handler(Gba& gba)
{
    if (!gba.gameboy.cpu.ime && !gba.gameboy.cpu.halt)
    {
        #if USE_SCHED
        assert(0);
        #endif
        return;
    }

    const auto live_interrupts = GB_IO_IF & GB_IO_IE & 0x1F;

    if (!live_interrupts)
    {
        #if USE_SCHED
        assert(0);
        #endif
        return;
    }

    // halt is always diabled at this point, this takes 4 cycles.
    if (gba.gameboy.cpu.halt)
    {
        gba.gameboy.cpu.halt = false;
        gba.gameboy.cycles += 4;
        #if USE_SCHED
        // todo: this can trigger, how?
        // assert(0);
        schedule_interrupt(gba);
        #endif
    }

    if (!gba.gameboy.cpu.ime)
    {
        #if USE_SCHED
        assert(0);
        #endif
        return;
    }

    gba.gameboy.cpu.ime = false;

    if (live_interrupts & INTERRUPT_VBLANK)
    {
        RST(64);
        disable_interrupt(gba, INTERRUPT_VBLANK);
    }
    else if (live_interrupts & INTERRUPT_LCD_STAT)
    {
        RST(72);
        disable_interrupt(gba, INTERRUPT_LCD_STAT);
    }
    else if (live_interrupts & INTERRUPT_TIMER)
    {
        RST(80);
        disable_interrupt(gba, INTERRUPT_TIMER);
    }
    else if (live_interrupts & INTERRUPT_SERIAL)
    {
        RST(88);
        disable_interrupt(gba, INTERRUPT_SERIAL);
    }
    else if (live_interrupts & INTERRUPT_JOYPAD)
    {
        RST(96);
        disable_interrupt(gba, INTERRUPT_JOYPAD);
    }

    gba.gameboy.cycles += 20;
}

inline void execute_cb(Gba& gba)
{
    const auto opcode = read8(REG_PC++);

    switch (opcode)
    {
        case 0x00: case 0x01: case 0x02: case 0x03:
        case 0x04: case 0x05: case 0x07:
            RLC_r();
            break;

        case 0x06:
            RLC_HLa();
            break;

        case 0x08: case 0x09: case 0x0A: case 0x0B:
        case 0x0C: case 0x0D: case 0x0F:
            RRC_r();
            break;

        case 0x0E:
            RRC_HLa();
            break;

        case 0x10: case 0x11: case 0x12: case 0x13:
        case 0x14: case 0x15: case 0x17:
            RL_r();
            break;

        case 0x16:
            RL_HLa();
            break;

        case 0x18: case 0x19: case 0x1A: case 0x1B:
        case 0x1C: case 0x1D: case 0x1F:
            RR_r();
            break;

        case 0x1E:
            RR_HLa();
            break;

        case 0x20: case 0x21: case 0x22: case 0x23:
        case 0x24: case 0x25: case 0x27:
            SLA_r();
            break;

        case 0x26:
            SLA_HLa();
            break;

        case 0x28: case 0x29: case 0x2A: case 0x2B:
        case 0x2C: case 0x2D: case 0x2F:
            SRA_r();
            break;

        case 0x2E:
            SRA_HLa();
            break;

        case 0x30: case 0x31: case 0x32: case 0x33:
        case 0x34: case 0x35: case 0x37: SWAP_r(); break;

        case 0x36: SWAP_HLa(); break;

        case 0x38: case 0x39: case 0x3A: case 0x3B:
        case 0x3C: case 0x3D: case 0x3F:
            SRL_r();
            break;

        case 0x3E:
            SRL_HLa();
            break;

        case 0x40: case 0x41: case 0x42: case 0x43:
        case 0x44: case 0x45: case 0x47: case 0x48:
        case 0x49: case 0x4A: case 0x4B: case 0x4C:
        case 0x4D: case 0x4F: case 0x50: case 0x51:
        case 0x52: case 0x53: case 0x54: case 0x55:
        case 0x57: case 0x58: case 0x59: case 0x5A:
        case 0x5B: case 0x5C: case 0x5D: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: case 0x67: case 0x68:
        case 0x69: case 0x6A: case 0x6B: case 0x6C:
        case 0x6D: case 0x6F: case 0x70: case 0x71:
        case 0x72: case 0x73: case 0x74: case 0x75:
        case 0x77: case 0x78: case 0x79: case 0x7A:
        case 0x7B: case 0x7C: case 0x7D: case 0x7F:
            BIT_r();
            break;

        case 0x46: case 0x4E: case 0x56: case 0x5E:
        case 0x66: case 0x6E: case 0x76: case 0x7E:
            BIT_HLa();
            break;

        case 0x80: case 0x81: case 0x82: case 0x83:
        case 0x84: case 0x85: case 0x87: case 0x88:
        case 0x89: case 0x8A: case 0x8B: case 0x8C:
        case 0x8D: case 0x8F: case 0x90: case 0x91:
        case 0x92: case 0x93: case 0x94: case 0x95:
        case 0x97: case 0x98: case 0x99: case 0x9A:
        case 0x9B: case 0x9C: case 0x9D: case 0x9F:
        case 0xA0: case 0xA1: case 0xA2: case 0xA3:
        case 0xA4: case 0xA5: case 0xA7: case 0xA8:
        case 0xA9: case 0xAA: case 0xAB: case 0xAC:
        case 0xAD: case 0xAF: case 0xB0: case 0xB1:
        case 0xB2: case 0xB3: case 0xB4: case 0xB5:
        case 0xB7: case 0xB8: case 0xB9: case 0xBA:
        case 0xBB: case 0xBC: case 0xBD: case 0xBF:
            RES_r();
            break;

        case 0x86: case 0x8E: case 0x96: case 0x9E:
        case 0xA6: case 0xAE: case 0xB6: case 0xBE:
            RES_HLa();
            break;


        case 0xC6: case 0xCE: case 0xD6: case 0xDE:
        case 0xE6: case 0xEE: case 0xF6: case 0xFE:
            SET_HLa();
            break;

        case 0xC0: case 0xC1: case 0xC2: case 0xC3:
        case 0xC4: case 0xC5: case 0xC7: case 0xC8:
        case 0xC9: case 0xCA: case 0xCB: case 0xCC:
        case 0xCD: case 0xCF: case 0xD0: case 0xD1:
        case 0xD2: case 0xD3: case 0xD4: case 0xD5:
        case 0xD7: case 0xD8: case 0xD9: case 0xDA:
        case 0xDB: case 0xDC: case 0xDD: case 0xDF:
        case 0xE0: case 0xE1: case 0xE2: case 0xE3:
        case 0xE4: case 0xE5: case 0xE7: case 0xE8:
        case 0xE9: case 0xEA: case 0xEB: case 0xEC:
        case 0xED: case 0xEF: case 0xF0: case 0xF1:
        case 0xF2: case 0xF3: case 0xF4: case 0xF5:
        case 0xF7: case 0xF8: case 0xF9: case 0xFA:
        case 0xFB: case 0xFC: case 0xFD: case 0xFF:
            SET_r();
            break;

        default:
            UNK_OP(gba, opcode, true);
            break;
    }

    gba.gameboy.cycles += CYCLE_TABLE_CB[opcode];
}

inline void execute(Gba& gba)
{
    const auto opcode = read8(REG_PC);

    // TODO: is the halt bug a thing on agb?

    #if 1
    // if halt bug happens, the same instruction is executed twice
    // so to emulate this, we DON'T advance the pc if set (PC += !1)
    REG_PC += !gba.gameboy.cpu.halt_bug;
    // reset halt bug after!
    gba.gameboy.cpu.halt_bug = false;
    #else
    REG_PC += 1;
    #endif

    switch (opcode)
    {
        case 0x00: break; // nop
        case 0x01: LD_BC_u16(); break;
        case 0x02: LD_BCa_A(); break;
        case 0x03: INC_BC(); break;

        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x3C: INC_r(); break;
        case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x3D: DEC_r(); break;

        case 0x06: LD_r_u8(); break;
        case 0x07: RLCA(); break;
        case 0x08: LD_u16_SP(); break;
        case 0x0A: LD_A_BCa(); break;
        case 0x09: ADD_HL_BC(); break;
        case 0x0B: DEC_BC(); break;
        case 0x0E: LD_r_u8(); break;
        case 0x0F: RRCA(); break;
        case 0x10: STOP(gba); break;
        case 0x11: LD_DE_u16(); break;
        case 0x12: LD_DEa_A(); break;
        case 0x13: INC_DE(); break;
        case 0x16: LD_r_u8(); break;
        case 0x17: RLA(); break;
        case 0x18: JR(); break;
        case 0x19: ADD_HL_DE(); break;
        case 0x1A: LD_A_DEa(); break;
        case 0x1B: DEC_DE(); break;
        case 0x1E: LD_r_u8(); break;
        case 0x1F: RRA(); break;
        case 0x20: JR_NZ(); break;
        case 0x21: LD_HL_u16(); break;
        case 0x22: LD_HLi_A(); break;
        case 0x23: INC_HL(); break;
        case 0x26: LD_r_u8(); break;
        case 0x27: DAA(); break;
        case 0x28: JR_Z(); break;
        case 0x29: ADD_HL_HL(); break;
        case 0x2A: LD_A_HLi(); break;
        case 0x2B: DEC_HL(); break;
        case 0x2E: LD_r_u8(); break;
        case 0x2F: CPL(); break;
        case 0x30: JR_NC(); break;
        case 0x31: LD_SP_u16(); break;
        case 0x32: LD_HLd_A(); break;
        case 0x33: INC_SP(); break;
        case 0x34: INC_HLa(); break;
        case 0x35: DEC_HLa(); break;
        case 0x36: LD_HLa_u8(); break;
        case 0x37: SCF(); break;
        case 0x38: JR_C(); break;
        case 0x39: ADD_HL_SP(); break;
        case 0x3A: LD_A_HLd(); break;
        case 0x3B: DEC_SP(); break;
        case 0x3E: LD_r_u8(); break;
        case 0x3F: CCF(); break;

        case 0x41: case 0x42: case 0x43: case 0x44:
        case 0x45: case 0x47: case 0x48: case 0x4A:
        case 0x4B: case 0x4C: case 0x4D: case 0x4F:
        case 0x50: case 0x51: case 0x53: case 0x54:
        case 0x55: case 0x57: case 0x58: case 0x59:
        case 0x5A: case 0x5C: case 0x5D: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x65: case 0x67: case 0x68: case 0x69:
        case 0x6A: case 0x6B: case 0x6C: case 0x6F:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D:
            LD_r_r();
            break;

        case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E: LD_r_HLa(); break;

        case 0x40: break; // nop LD b,b
        case 0x49: break; // nop LD c,c
        case 0x52: break; // nop LD d,d
        case 0x5B: break; // nop LD e,e
        case 0x64: break; // nop LD h,h
        case 0x6D: break; // nop LD l,l
        case 0x7F: break; // nop LD a,a

        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77: LD_HLa_r(); break;

        case 0x76: HALT(gba); break;
        case 0x7E: LD_A_HLa(); break;

        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87: ADD_r(); break;
        case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8F: ADC_r(); break;
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x97: SUB_r(); break;
        case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9F: SBC_r(); break;
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA7: AND_r(); break;
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAF: XOR_r(); break;
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB7: OR_r(); break;
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBF: CP_r(); break;

        case 0x86: ADD_HLa(); break;
        case 0x8E: ADC_HLa(); break;
        case 0x96: SUB_HLa(); break;
        case 0x9E: SBC_HLa(); break;
        case 0xA6: AND_HLa(); break;
        case 0xAE: XOR_HLa(); break;
        case 0xB6: OR_HLa(); break;
        case 0xBE: CP_HLa(); break;
        case 0xC0: RET_NZ(); break;
        case 0xC1: POP_BC(); break;
        case 0xC2: JP_NZ(); break;
        case 0xC3: JP();  break;
        case 0xC4: CALL_NZ(); break;
        case 0xC5: PUSH(REG_BC); break;
        case 0xC6: ADD_u8(); break;
        case 0xC7: RST(0x00); break;
        case 0xC8: RET_Z(); break;
        case 0xC9: RET(); break;
        case 0xCA: JP_Z(); break;
        // return here as to not increase the cycles from this opcode!
        case 0xCB: execute_cb(gba); return;
        case 0xCC: CALL_Z(); break;
        case 0xCD: CALL(); break;
        case 0xCE: ADC_u8(); break;
        case 0xCF: RST(0x08); break;
        case 0xD0: RET_NC(); break;
        case 0xD1: POP_DE();  break;
        case 0xD2: JP_NC(); break;
        case 0xD4: CALL_NC(); break;
        case 0xD5: PUSH(REG_DE); break;
        case 0xD6: SUB_u8(); break;
        case 0xD7: RST(0x10); break;
        case 0xD8: RET_C(); break;
        case 0xD9: RETI(); break;
        case 0xDA: JP_C(); break;
        case 0xDC: CALL_C(); break;
        case 0xDE: SBC_u8(); break;
        case 0xDF: RST(0x18); break;
        case 0xE0: LD_FFu8_A(); break;
        case 0xE1: POP_HL(); break;
        case 0xE2: LD_FFRC_A(); break;
        case 0xE5: PUSH(REG_HL); break;
        case 0xE6: AND_u8(); break;
        case 0xE7: RST(0x20); break;
        case 0xE8: ADD_SP_i8(); break;
        case 0xE9: JP_HL(); break;
        case 0xEA: LD_u16_A(); break;
        case 0xEE: XOR_u8(); break;
        case 0xEF: RST(0x28); break;
        case 0xF0: LD_A_FFu8(); break;
        case 0xF1: POP_AF(); break;
        case 0xF2: LD_A_FFRC(); break;
        case 0xF3: DI(); break;
        case 0xF5: PUSH(REG_AF); break;
        case 0xF6: OR_u8(); break;
        case 0xF7: RST(0x30); break;
        case 0xF8: LD_HL_SP_i8(); break;
        case 0xF9: LD_SP_HL(); break;
        case 0xFA: LD_A_u16(); break;
        case 0xFB: EI(); break;
        case 0xFE: CP_u8(); break;
        case 0xFF: RST(0x38); break;

        default:
            UNK_OP(gba, opcode, false);
            break;
    }

    gba.gameboy.cycles += CYCLE_TABLE[opcode];
}

} // namespace

// [API] not used by the cpu core
void cpu_set_flag(Gba& gba, enum CpuFlags flag, bool value)
{
    switch (flag)
    {
        case CPU_FLAG_C: SET_FLAG_C(value); break;
        case CPU_FLAG_H: SET_FLAG_H(value); break;
        case CPU_FLAG_N: SET_FLAG_N(value); break;
        case CPU_FLAG_Z: SET_FLAG_Z(value); break;
    }
}

auto cpu_get_flag(const Gba& gba, enum CpuFlags flag) -> bool
{
    switch (flag)
    {
        case CPU_FLAG_C: return FLAG_C;
        case CPU_FLAG_H: return FLAG_H;
        case CPU_FLAG_N: return FLAG_N;
        case CPU_FLAG_Z: return FLAG_Z;
    }

    std::unreachable();
}

void cpu_set_register(Gba& gba, enum CpuRegisters reg, u8 value)
{
    switch (reg)
    {
        case CPU_REGISTER_B: REG_B = value; break;
        case CPU_REGISTER_C: REG_C = value; break;
        case CPU_REGISTER_D: REG_D = value; break;
        case CPU_REGISTER_E: REG_E = value; break;
        case CPU_REGISTER_H: REG_H = value; break;
        case CPU_REGISTER_L: REG_L = value; break;
        case CPU_REGISTER_A: REG_A = value; break;
        case CPU_REGISTER_F: REG_F_SET(value); break;
    }
}

auto cpu_get_register(const Gba& gba, enum CpuRegisters reg) -> u8
{
    switch (reg)
    {
        case CPU_REGISTER_B: return REG_B;
        case CPU_REGISTER_C: return REG_C;
        case CPU_REGISTER_D: return REG_D;
        case CPU_REGISTER_E: return REG_E;
        case CPU_REGISTER_H: return REG_H;
        case CPU_REGISTER_L: return REG_L;
        case CPU_REGISTER_A: return REG_A;
        case CPU_REGISTER_F: return REG_F_GET();
    }

    std::unreachable();
}

void cpu_set_register_pair(Gba& gba, enum CpuRegisterPairs pair, u16 value)
{
    switch (pair)
    {
        case CPU_REGISTER_PAIR_BC: SET_REG_BC(value); break;
        case CPU_REGISTER_PAIR_DE: SET_REG_DE(value); break;
        case CPU_REGISTER_PAIR_HL: SET_REG_HL(value); break;
        case CPU_REGISTER_PAIR_AF: SET_REG_AF(value); break;
        case CPU_REGISTER_PAIR_SP: REG_SP = value; break;
        case CPU_REGISTER_PAIR_PC: REG_PC = value; break;
    }
}

auto cpu_get_register_pair(const Gba& gba, enum CpuRegisterPairs pair) -> u16
{
    switch (pair)
    {
        case CPU_REGISTER_PAIR_BC: return REG_BC;
        case CPU_REGISTER_PAIR_DE: return REG_DE;
        case CPU_REGISTER_PAIR_HL: return REG_HL;
        case CPU_REGISTER_PAIR_AF: return REG_AF;
        case CPU_REGISTER_PAIR_SP: return REG_SP;
        case CPU_REGISTER_PAIR_PC: return REG_PC;
    }

    std::unreachable();
}

void on_halt_event(void* user, s32 id, s32 late)
{
    #if USE_SCHED
    auto& gba = *static_cast<Gba*>(user);

    // assert(gba.scheduler.next_event != scheduler::ID::HALT && "halt bug");

    while (gba.gameboy.cpu.halt && !gba.frame_end)
    {
        gba.scheduler.advance_to_next_event();
        gba.scheduler.fire();
    }
    #endif
}

void on_interrupt_event(void* user, s32 id, s32 late)
{
    #if USE_SCHED
    auto& gba = *static_cast<Gba*>(user);

    if (gba.gameboy.cpu.ime_delay)
    {
        if (gba.gameboy.cpu.ime && GB_IO_IF & GB_IO_IE & 0x1F)
        {
            interrupt_handler(gba);
        }
        // assert(!gba.gameboy.cpu.ime);
        gba.gameboy.cpu.ime_delay = false;
        gba.gameboy.cpu.ime = true;
        schedule_interrupt(gba, 1);
    }
    else if (gba.gameboy.cpu.ime)
    {
        interrupt_handler(gba);
    }
    #endif
}

void schedule_interrupt(Gba& gba, u8 cycles_delay)
{
    #if USE_SCHED
    if (GB_IO_IF & GB_IO_IE & 0x1F)
    {
        if (gba.gameboy.cpu.halt)
        {
            gba.gameboy.cycles += 4;
            gba.gameboy.cpu.halt = false;
        }

        if (gba.gameboy.cpu.ime)
        {
            gba.scheduler.add(scheduler::ID::INTERRUPT, cycles_delay, on_interrupt_event, &gba);
        }
    }
    #endif
}

void cpu_run(Gba& gba)
{
    gba.gameboy.cycles = 0;
    #if !USE_SCHED
    // check and handle interrupts
    interrupt_handler(gba);

    // EI overlaps with the next fetch and ISR, meaning it hasn't yet
    // set ime during that time, hense the delay.
    // this is important as it means games can do:
    // EI -> ADD -> ISR, whereas without the delay, it would EI -> ISR.
    // this breaks bubble bobble if ime is not delayed!
    // SEE: https://github.com/ITotalJustice/TotalGB/issues/42
    gba.gameboy.cpu.ime |= gba.gameboy.cpu.ime_delay;
    gba.gameboy.cpu.ime_delay = false;

    // if halted, return early
    if (gba.gameboy.cpu.halt) [[unlikely]]
    {
        gba.gameboy.cycles = 4;
        return;
    }

    #endif
    execute(gba);
    assert(gba.gameboy.cycles != 0);
}

#undef FLAG_C
#undef FLAG_H
#undef FLAG_N
#undef FLAG_Z
#undef SET_FLAG_C
#undef SET_FLAG_H
#undef SET_FLAG_N
#undef SET_FLAG_Z
#undef REG
#undef REG_B
#undef REG_C
#undef REG_D
#undef REG_E
#undef REG_H
#undef REG_L
#undef REG_A
#undef REG_F_GET
#undef REG_F_SET
#undef REG_BC
#undef REG_DE
#undef REG_HL
#undef REG_AF
#undef SET_REG_BC
#undef SET_REG_DE
#undef SET_REG_HL
#undef SET_REG_AF
#undef REG_SP
#undef REG_PC
#undef SET_FLAGS_HN
#undef SET_FLAGS_HZ
#undef SET_FLAGS_HNZ
#undef SET_FLAGS_CHN
#undef SET_ALL_FLAGS
#undef read8
#undef read16
#undef write8
#undef write16
#undef PUSH
#undef POP
#undef CALL
#undef CALL_NZ
#undef CALL_Z
#undef CALL_NC
#undef CALL_C
#undef JP
#undef JP_HL
#undef JP_NZ
#undef JP_Z
#undef JP_NC
#undef JP_C
#undef JR
#undef JR_NZ
#undef JR_Z
#undef JR_NC
#undef JR_C
#undef RET
#undef RET_NZ
#undef RET_Z
#undef RET_NC
#undef RET_C
#undef INC_r
#undef INC_HLa
#undef DEC_r
#undef DEC_HLa
#undef INC_BC
#undef INC_DE
#undef INC_HL
#undef DEC_BC
#undef DEC_DE
#undef DEC_HL
#undef INC_SP
#undef DEC_SP
#undef LD_r_r
#undef LD_r_u8
#undef LD_HLa_r
#undef LD_HLa_u8
#undef LD_r_HLa
#undef LD_SP_u16
#undef LD_A_u16
#undef LD_u16_A
#undef LD_HLi_A
#undef LD_HLd_A
#undef LD_A_HLi
#undef LD_A_HLd
#undef LD_A_BCa
#undef LD_A_DEa
#undef LD_A_HLa
#undef LD_BCa_A
#undef LD_DEa_A
#undef LD_FFRC_A
#undef LD_A_FFRC
#undef LD_BC_u16
#undef LD_DE_u16
#undef LD_HL_u16
#undef LD_u16_SP
#undef LD_SP_HL
#undef LD_FFu8_A
#undef LD_A_FFu8
#undef CP_r
#undef CP_u8
#undef CP_HLa
#undef ADD_INTERNAL
#undef ADD_r
#undef ADD_u8
#undef ADD_HLa
#undef ADD_HL_INTERNAL
#undef ADD_HL_BC
#undef ADD_HL_DE
#undef ADD_HL_HL
#undef ADD_HL_SP
#undef ADD_SP_i8
#undef LD_HL_SP_i8
#undef ADC_r
#undef ADC_u8
#undef ADC_HLa
#undef SUB_INTERNAL
#undef SUB_r
#undef SUB_u8
#undef SUB_HLa
#undef SBC_r
#undef SBC_u8
#undef SBC_HLa
#undef AND_r
#undef AND_u8
#undef AND_HLa
#undef XOR_r
#undef XOR_u8
#undef XOR_HLa
#undef OR_r
#undef OR_u8
#undef OR_HLa
#undef DI
#undef EI
#undef POP_BC
#undef POP_DE
#undef POP_HL
#undef POP_AF
#undef RL_r
#undef RLA
#undef RL_HLa
#undef RLC_r
#undef RLC_HLa
#undef RLCA
#undef RR_r
#undef RR_HLa
#undef RRA
#undef RRC_r
#undef RRCA
#undef RRC_HLa
#undef SLA_r
#undef SLA_HLa
#undef SRA_r
#undef SRA_HLa
#undef SRL_r
#undef SRL_HLa
#undef SWAP_r
#undef SWAP_HLa
#undef BIT_r
#undef BIT_HLa
#undef RES_r
#undef RES_HLa
#undef SET_r
#undef SET_HLa
#undef DAA
#undef RETI
#undef RST
#undef CPL
#undef SCF
#undef CCF

} // namespace gba::gb
