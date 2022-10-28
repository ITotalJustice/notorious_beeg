// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "arm7tdmi/arm/branch.cpp"
#include "arm7tdmi/arm/data_processing.cpp"
#include "arm7tdmi/arm/halfword_data_transfer.cpp"
#include "arm7tdmi/arm/single_data_transfer.cpp"
#include "arm7tdmi/arm/block_data_transfer.cpp"
#include "arm7tdmi/arm/multiply.cpp"
#include "arm7tdmi/arm/software_interrupt.cpp"
#include "arm7tdmi/arm/branch_and_exchange.cpp"
#include "arm7tdmi/arm/multiply_long.cpp"
#include "arm7tdmi/arm/single_data_swap.cpp"
#include "arm7tdmi/arm/msr.cpp"
#include "arm7tdmi/arm/mrs.cpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "gba.hpp"
#include "mem.hpp"
#include "log.hpp"
#include <cassert>
#include <array>
#include <cstdint>

namespace gba::arm7tdmi::arm {
namespace {

constexpr auto decode_template(u32 opcode)
{
    return (bit::get_range<20, 27>(opcode) << 4) | (bit::get_range<4, 7>(opcode));
}

auto undefined(Gba& gba, u32 opcode) -> void
{
    log::print_fatal(gba, log::Type::ARM, "undefined 0x%08X\n", opcode);
    assert(!"[arm] undefined");
}

inline auto execute_switch(Gba& gba, u32 opcode) -> void
{
    switch (decode_template(opcode))
    {
        case 0: data_processing_reg<0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1: data_processing_reg<0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2: data_processing_reg<0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 3: data_processing_reg<0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 4: data_processing_reg<0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 5: data_processing_reg<0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 6: data_processing_reg<0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 7: data_processing_reg<0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 8: data_processing_reg<0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 9: multiply<0, 0>(gba, opcode); break;
        case 10: data_processing_reg<0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 11: halfword_data_transfer_register_offset<0, 0, 0, 0, 0, 1>(gba, opcode); break;
        case 12: data_processing_reg<0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 13: halfword_data_transfer_register_offset<0, 0, 0, 0, 1, 0>(gba, opcode); break;
        case 14: data_processing_reg<0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 15: halfword_data_transfer_register_offset<0, 0, 0, 0, 1, 1>(gba, opcode); break;
        case 16: data_processing_reg<1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 17: data_processing_reg<1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 18: data_processing_reg<1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 19: data_processing_reg<1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 20: data_processing_reg<1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 21: data_processing_reg<1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 22: data_processing_reg<1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 23: data_processing_reg<1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 24: data_processing_reg<1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 25: multiply<0, 1>(gba, opcode); break;
        case 26: data_processing_reg<1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 27: halfword_data_transfer_register_offset<0, 0, 0, 1, 0, 1>(gba, opcode); break;
        case 28: data_processing_reg<1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 29: halfword_data_transfer_register_offset<0, 0, 0, 1, 1, 0>(gba, opcode); break;
        case 30: data_processing_reg<1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 31: halfword_data_transfer_register_offset<0, 0, 0, 1, 1, 1>(gba, opcode); break;
        case 32: data_processing_reg<0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 33: data_processing_reg<0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 34: data_processing_reg<0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 35: data_processing_reg<0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 36: data_processing_reg<0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 37: data_processing_reg<0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 38: data_processing_reg<0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 39: data_processing_reg<0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 40: data_processing_reg<0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 41: multiply<1, 0>(gba, opcode); break;
        case 42: data_processing_reg<0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 43: halfword_data_transfer_register_offset<0, 0, 1, 0, 0, 1>(gba, opcode); break;
        case 44: data_processing_reg<0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 45: halfword_data_transfer_register_offset<0, 0, 1, 0, 1, 0>(gba, opcode); break;
        case 46: data_processing_reg<0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 47: halfword_data_transfer_register_offset<0, 0, 1, 0, 1, 1>(gba, opcode); break;
        case 48: data_processing_reg<1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 49: data_processing_reg<1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 50: data_processing_reg<1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 51: data_processing_reg<1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 52: data_processing_reg<1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 53: data_processing_reg<1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 54: data_processing_reg<1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 55: data_processing_reg<1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 56: data_processing_reg<1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 57: multiply<1, 1>(gba, opcode); break;
        case 58: data_processing_reg<1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 59: halfword_data_transfer_register_offset<0, 0, 1, 1, 0, 1>(gba, opcode); break;
        case 60: data_processing_reg<1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 61: halfword_data_transfer_register_offset<0, 0, 1, 1, 1, 0>(gba, opcode); break;
        case 62: data_processing_reg<1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 63: halfword_data_transfer_register_offset<0, 0, 1, 1, 1, 1>(gba, opcode); break;
        case 64: data_processing_reg<0, 2, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 65: data_processing_reg<0, 2, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 66: data_processing_reg<0, 2, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 67: data_processing_reg<0, 2, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 68: data_processing_reg<0, 2, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 69: data_processing_reg<0, 2, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 70: data_processing_reg<0, 2, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 71: data_processing_reg<0, 2, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 72: data_processing_reg<0, 2, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 73: halfword_data_transfer_immediate_offset<0, 0, 0, 0, 0, 0>(gba, opcode); break;
        case 74: data_processing_reg<0, 2, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 75: halfword_data_transfer_immediate_offset<0, 0, 0, 0, 0, 1>(gba, opcode); break;
        case 76: data_processing_reg<0, 2, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 77: halfword_data_transfer_immediate_offset<0, 0, 0, 0, 1, 0>(gba, opcode); break;
        case 78: data_processing_reg<0, 2, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 79: halfword_data_transfer_immediate_offset<0, 0, 0, 0, 1, 1>(gba, opcode); break;
        case 80: data_processing_reg<1, 2, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 81: data_processing_reg<1, 2, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 82: data_processing_reg<1, 2, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 83: data_processing_reg<1, 2, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 84: data_processing_reg<1, 2, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 85: data_processing_reg<1, 2, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 86: data_processing_reg<1, 2, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 87: data_processing_reg<1, 2, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 88: data_processing_reg<1, 2, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 89: halfword_data_transfer_immediate_offset<0, 0, 0, 1, 0, 0>(gba, opcode); break;
        case 90: data_processing_reg<1, 2, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 91: halfword_data_transfer_immediate_offset<0, 0, 0, 1, 0, 1>(gba, opcode); break;
        case 92: data_processing_reg<1, 2, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 93: halfword_data_transfer_immediate_offset<0, 0, 0, 1, 1, 0>(gba, opcode); break;
        case 94: data_processing_reg<1, 2, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 95: halfword_data_transfer_immediate_offset<0, 0, 0, 1, 1, 1>(gba, opcode); break;
        case 96: data_processing_reg<0, 3, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 97: data_processing_reg<0, 3, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 98: data_processing_reg<0, 3, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 99: data_processing_reg<0, 3, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 100: data_processing_reg<0, 3, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 101: data_processing_reg<0, 3, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 102: data_processing_reg<0, 3, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 103: data_processing_reg<0, 3, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 104: data_processing_reg<0, 3, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 105: halfword_data_transfer_immediate_offset<0, 0, 1, 0, 0, 0>(gba, opcode); break;
        case 106: data_processing_reg<0, 3, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 107: halfword_data_transfer_immediate_offset<0, 0, 1, 0, 0, 1>(gba, opcode); break;
        case 108: data_processing_reg<0, 3, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 109: halfword_data_transfer_immediate_offset<0, 0, 1, 0, 1, 0>(gba, opcode); break;
        case 110: data_processing_reg<0, 3, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 111: halfword_data_transfer_immediate_offset<0, 0, 1, 0, 1, 1>(gba, opcode); break;
        case 112: data_processing_reg<1, 3, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 113: data_processing_reg<1, 3, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 114: data_processing_reg<1, 3, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 115: data_processing_reg<1, 3, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 116: data_processing_reg<1, 3, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 117: data_processing_reg<1, 3, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 118: data_processing_reg<1, 3, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 119: data_processing_reg<1, 3, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 120: data_processing_reg<1, 3, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 121: halfword_data_transfer_immediate_offset<0, 0, 1, 1, 0, 0>(gba, opcode); break;
        case 122: data_processing_reg<1, 3, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 123: halfword_data_transfer_immediate_offset<0, 0, 1, 1, 0, 1>(gba, opcode); break;
        case 124: data_processing_reg<1, 3, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 125: halfword_data_transfer_immediate_offset<0, 0, 1, 1, 1, 0>(gba, opcode); break;
        case 126: data_processing_reg<1, 3, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 127: halfword_data_transfer_immediate_offset<0, 0, 1, 1, 1, 1>(gba, opcode); break;
        case 128: data_processing_reg<0, 4, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 129: data_processing_reg<0, 4, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 130: data_processing_reg<0, 4, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 131: data_processing_reg<0, 4, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 132: data_processing_reg<0, 4, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 133: data_processing_reg<0, 4, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 134: data_processing_reg<0, 4, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 135: data_processing_reg<0, 4, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 136: data_processing_reg<0, 4, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 137: multiply_long<0, 0, 0>(gba, opcode); break;
        case 138: data_processing_reg<0, 4, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 139: halfword_data_transfer_register_offset<0, 1, 0, 0, 0, 1>(gba, opcode); break;
        case 140: data_processing_reg<0, 4, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 141: halfword_data_transfer_register_offset<0, 1, 0, 0, 1, 0>(gba, opcode); break;
        case 142: data_processing_reg<0, 4, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 143: halfword_data_transfer_register_offset<0, 1, 0, 0, 1, 1>(gba, opcode); break;
        case 144: data_processing_reg<1, 4, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 145: data_processing_reg<1, 4, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 146: data_processing_reg<1, 4, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 147: data_processing_reg<1, 4, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 148: data_processing_reg<1, 4, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 149: data_processing_reg<1, 4, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 150: data_processing_reg<1, 4, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 151: data_processing_reg<1, 4, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 152: data_processing_reg<1, 4, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 153: multiply_long<0, 0, 1>(gba, opcode); break;
        case 154: data_processing_reg<1, 4, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 155: halfword_data_transfer_register_offset<0, 1, 0, 1, 0, 1>(gba, opcode); break;
        case 156: data_processing_reg<1, 4, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 157: halfword_data_transfer_register_offset<0, 1, 0, 1, 1, 0>(gba, opcode); break;
        case 158: data_processing_reg<1, 4, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 159: halfword_data_transfer_register_offset<0, 1, 0, 1, 1, 1>(gba, opcode); break;
        case 160: data_processing_reg<0, 5, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 161: data_processing_reg<0, 5, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 162: data_processing_reg<0, 5, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 163: data_processing_reg<0, 5, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 164: data_processing_reg<0, 5, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 165: data_processing_reg<0, 5, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 166: data_processing_reg<0, 5, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 167: data_processing_reg<0, 5, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 168: data_processing_reg<0, 5, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 169: multiply_long<0, 1, 0>(gba, opcode); break;
        case 170: data_processing_reg<0, 5, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 171: halfword_data_transfer_register_offset<0, 1, 1, 0, 0, 1>(gba, opcode); break;
        case 172: data_processing_reg<0, 5, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 173: halfword_data_transfer_register_offset<0, 1, 1, 0, 1, 0>(gba, opcode); break;
        case 174: data_processing_reg<0, 5, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 175: halfword_data_transfer_register_offset<0, 1, 1, 0, 1, 1>(gba, opcode); break;
        case 176: data_processing_reg<1, 5, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 177: data_processing_reg<1, 5, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 178: data_processing_reg<1, 5, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 179: data_processing_reg<1, 5, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 180: data_processing_reg<1, 5, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 181: data_processing_reg<1, 5, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 182: data_processing_reg<1, 5, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 183: data_processing_reg<1, 5, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 184: data_processing_reg<1, 5, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 185: multiply_long<0, 1, 1>(gba, opcode); break;
        case 186: data_processing_reg<1, 5, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 187: halfword_data_transfer_register_offset<0, 1, 1, 1, 0, 1>(gba, opcode); break;
        case 188: data_processing_reg<1, 5, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 189: halfword_data_transfer_register_offset<0, 1, 1, 1, 1, 0>(gba, opcode); break;
        case 190: data_processing_reg<1, 5, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 191: halfword_data_transfer_register_offset<0, 1, 1, 1, 1, 1>(gba, opcode); break;
        case 192: data_processing_reg<0, 6, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 193: data_processing_reg<0, 6, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 194: data_processing_reg<0, 6, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 195: data_processing_reg<0, 6, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 196: data_processing_reg<0, 6, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 197: data_processing_reg<0, 6, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 198: data_processing_reg<0, 6, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 199: data_processing_reg<0, 6, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 200: data_processing_reg<0, 6, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 201: multiply_long<1, 0, 0>(gba, opcode); break;
        case 202: data_processing_reg<0, 6, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 203: halfword_data_transfer_immediate_offset<0, 1, 0, 0, 0, 1>(gba, opcode); break;
        case 204: data_processing_reg<0, 6, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 205: halfword_data_transfer_immediate_offset<0, 1, 0, 0, 1, 0>(gba, opcode); break;
        case 206: data_processing_reg<0, 6, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 207: halfword_data_transfer_immediate_offset<0, 1, 0, 0, 1, 1>(gba, opcode); break;
        case 208: data_processing_reg<1, 6, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 209: data_processing_reg<1, 6, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 210: data_processing_reg<1, 6, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 211: data_processing_reg<1, 6, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 212: data_processing_reg<1, 6, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 213: data_processing_reg<1, 6, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 214: data_processing_reg<1, 6, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 215: data_processing_reg<1, 6, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 216: data_processing_reg<1, 6, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 217: multiply_long<1, 0, 1>(gba, opcode); break;
        case 218: data_processing_reg<1, 6, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 219: halfword_data_transfer_immediate_offset<0, 1, 0, 1, 0, 1>(gba, opcode); break;
        case 220: data_processing_reg<1, 6, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 221: halfword_data_transfer_immediate_offset<0, 1, 0, 1, 1, 0>(gba, opcode); break;
        case 222: data_processing_reg<1, 6, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 223: halfword_data_transfer_immediate_offset<0, 1, 0, 1, 1, 1>(gba, opcode); break;
        case 224: data_processing_reg<0, 7, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 225: data_processing_reg<0, 7, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 226: data_processing_reg<0, 7, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 227: data_processing_reg<0, 7, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 228: data_processing_reg<0, 7, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 229: data_processing_reg<0, 7, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 230: data_processing_reg<0, 7, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 231: data_processing_reg<0, 7, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 232: data_processing_reg<0, 7, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 233: multiply_long<1, 1, 0>(gba, opcode); break;
        case 234: data_processing_reg<0, 7, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 235: halfword_data_transfer_immediate_offset<0, 1, 1, 0, 0, 1>(gba, opcode); break;
        case 236: data_processing_reg<0, 7, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 237: halfword_data_transfer_immediate_offset<0, 1, 1, 0, 1, 0>(gba, opcode); break;
        case 238: data_processing_reg<0, 7, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 239: halfword_data_transfer_immediate_offset<0, 1, 1, 0, 1, 1>(gba, opcode); break;
        case 240: data_processing_reg<1, 7, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 241: data_processing_reg<1, 7, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 242: data_processing_reg<1, 7, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 243: data_processing_reg<1, 7, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 244: data_processing_reg<1, 7, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 245: data_processing_reg<1, 7, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 246: data_processing_reg<1, 7, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 247: data_processing_reg<1, 7, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 248: data_processing_reg<1, 7, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 249: multiply_long<1, 1, 1>(gba, opcode); break;
        case 250: data_processing_reg<1, 7, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 251: halfword_data_transfer_immediate_offset<0, 1, 1, 1, 0, 1>(gba, opcode); break;
        case 252: data_processing_reg<1, 7, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 253: halfword_data_transfer_immediate_offset<0, 1, 1, 1, 1, 0>(gba, opcode); break;
        case 254: data_processing_reg<1, 7, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 255: halfword_data_transfer_immediate_offset<0, 1, 1, 1, 1, 1>(gba, opcode); break;
        case 256: mrs<0>(gba, opcode); break;
        case 257: data_processing_reg<0, 8, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 258: data_processing_reg<0, 8, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 259: data_processing_reg<0, 8, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 260: data_processing_reg<0, 8, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 261: data_processing_reg<0, 8, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 262: data_processing_reg<0, 8, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 263: data_processing_reg<0, 8, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 264: data_processing_reg<0, 8, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 265: single_data_swap<0>(gba, opcode); break;
        case 266: data_processing_reg<0, 8, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 267: halfword_data_transfer_register_offset<1, 0, 0, 0, 0, 1>(gba, opcode); break;
        case 268: data_processing_reg<0, 8, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 269: halfword_data_transfer_register_offset<1, 0, 0, 0, 1, 0>(gba, opcode); break;
        case 270: data_processing_reg<0, 8, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 271: halfword_data_transfer_register_offset<1, 0, 0, 0, 1, 1>(gba, opcode); break;
        case 272: data_processing_reg<1, 8, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 273: data_processing_reg<1, 8, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 274: data_processing_reg<1, 8, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 275: data_processing_reg<1, 8, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 276: data_processing_reg<1, 8, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 277: data_processing_reg<1, 8, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 278: data_processing_reg<1, 8, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 279: data_processing_reg<1, 8, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 280: data_processing_reg<1, 8, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 281: halfword_data_transfer_register_offset<1, 0, 0, 1, 0, 0>(gba, opcode); break;
        case 282: data_processing_reg<1, 8, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 283: halfword_data_transfer_register_offset<1, 0, 0, 1, 0, 1>(gba, opcode); break;
        case 284: data_processing_reg<1, 8, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 285: halfword_data_transfer_register_offset<1, 0, 0, 1, 1, 0>(gba, opcode); break;
        case 286: data_processing_reg<1, 8, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 287: halfword_data_transfer_register_offset<1, 0, 0, 1, 1, 1>(gba, opcode); break;
        case 288: msr<0, 0>(gba, opcode); break;
        case 289: branch_and_exchange(gba, opcode); break;
        case 290: msr<0, 0>(gba, opcode); break;
        case 291: msr<0, 0>(gba, opcode); break;
        case 292: msr<0, 0>(gba, opcode); break;
        case 293: msr<0, 0>(gba, opcode); break;
        case 294: msr<0, 0>(gba, opcode); break;
        case 295: msr<0, 0>(gba, opcode); break;
        case 296: msr<0, 0>(gba, opcode); break;
        case 297: halfword_data_transfer_register_offset<1, 0, 1, 0, 0, 0>(gba, opcode); break;
        case 298: msr<0, 0>(gba, opcode); break;
        case 299: halfword_data_transfer_register_offset<1, 0, 1, 0, 0, 1>(gba, opcode); break;
        case 300: msr<0, 0>(gba, opcode); break;
        case 301: halfword_data_transfer_register_offset<1, 0, 1, 0, 1, 0>(gba, opcode); break;
        case 302: msr<0, 0>(gba, opcode); break;
        case 303: halfword_data_transfer_register_offset<1, 0, 1, 0, 1, 1>(gba, opcode); break;
        case 304: data_processing_reg<1, 9, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 305: data_processing_reg<1, 9, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 306: data_processing_reg<1, 9, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 307: data_processing_reg<1, 9, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 308: data_processing_reg<1, 9, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 309: data_processing_reg<1, 9, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 310: data_processing_reg<1, 9, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 311: data_processing_reg<1, 9, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 312: data_processing_reg<1, 9, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 313: halfword_data_transfer_register_offset<1, 0, 1, 1, 0, 0>(gba, opcode); break;
        case 314: data_processing_reg<1, 9, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 315: halfword_data_transfer_register_offset<1, 0, 1, 1, 0, 1>(gba, opcode); break;
        case 316: data_processing_reg<1, 9, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 317: halfword_data_transfer_register_offset<1, 0, 1, 1, 1, 0>(gba, opcode); break;
        case 318: data_processing_reg<1, 9, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 319: halfword_data_transfer_register_offset<1, 0, 1, 1, 1, 1>(gba, opcode); break;
        case 320: mrs<1>(gba, opcode); break;
        case 321: data_processing_reg<0, 10, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 322: data_processing_reg<0, 10, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 323: data_processing_reg<0, 10, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 324: data_processing_reg<0, 10, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 325: data_processing_reg<0, 10, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 326: data_processing_reg<0, 10, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 327: data_processing_reg<0, 10, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 328: data_processing_reg<0, 10, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 329: single_data_swap<1>(gba, opcode); break;
        case 330: data_processing_reg<0, 10, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 331: halfword_data_transfer_immediate_offset<1, 0, 0, 0, 0, 1>(gba, opcode); break;
        case 332: data_processing_reg<0, 10, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 333: halfword_data_transfer_immediate_offset<1, 0, 0, 0, 1, 0>(gba, opcode); break;
        case 334: data_processing_reg<0, 10, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 335: halfword_data_transfer_immediate_offset<1, 0, 0, 0, 1, 1>(gba, opcode); break;
        case 336: data_processing_reg<1, 10, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 337: data_processing_reg<1, 10, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 338: data_processing_reg<1, 10, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 339: data_processing_reg<1, 10, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 340: data_processing_reg<1, 10, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 341: data_processing_reg<1, 10, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 342: data_processing_reg<1, 10, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 343: data_processing_reg<1, 10, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 344: data_processing_reg<1, 10, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 345: halfword_data_transfer_immediate_offset<1, 0, 0, 1, 0, 0>(gba, opcode); break;
        case 346: data_processing_reg<1, 10, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 347: halfword_data_transfer_immediate_offset<1, 0, 0, 1, 0, 1>(gba, opcode); break;
        case 348: data_processing_reg<1, 10, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 349: halfword_data_transfer_immediate_offset<1, 0, 0, 1, 1, 0>(gba, opcode); break;
        case 350: data_processing_reg<1, 10, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 351: halfword_data_transfer_immediate_offset<1, 0, 0, 1, 1, 1>(gba, opcode); break;
        case 352: msr<0, 1>(gba, opcode); break;
        case 353: msr<0, 1>(gba, opcode); break;
        case 354: msr<0, 1>(gba, opcode); break;
        case 355: msr<0, 1>(gba, opcode); break;
        case 356: msr<0, 1>(gba, opcode); break;
        case 357: msr<0, 1>(gba, opcode); break;
        case 358: msr<0, 1>(gba, opcode); break;
        case 359: msr<0, 1>(gba, opcode); break;
        case 360: msr<0, 1>(gba, opcode); break;
        case 361: halfword_data_transfer_immediate_offset<1, 0, 1, 0, 0, 0>(gba, opcode); break;
        case 362: msr<0, 1>(gba, opcode); break;
        case 363: halfword_data_transfer_immediate_offset<1, 0, 1, 0, 0, 1>(gba, opcode); break;
        case 364: msr<0, 1>(gba, opcode); break;
        case 365: halfword_data_transfer_immediate_offset<1, 0, 1, 0, 1, 0>(gba, opcode); break;
        case 366: msr<0, 1>(gba, opcode); break;
        case 367: halfword_data_transfer_immediate_offset<1, 0, 1, 0, 1, 1>(gba, opcode); break;
        case 368: data_processing_reg<1, 11, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 369: data_processing_reg<1, 11, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 370: data_processing_reg<1, 11, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 371: data_processing_reg<1, 11, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 372: data_processing_reg<1, 11, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 373: data_processing_reg<1, 11, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 374: data_processing_reg<1, 11, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 375: data_processing_reg<1, 11, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 376: data_processing_reg<1, 11, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 377: halfword_data_transfer_immediate_offset<1, 0, 1, 1, 0, 0>(gba, opcode); break;
        case 378: data_processing_reg<1, 11, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 379: halfword_data_transfer_immediate_offset<1, 0, 1, 1, 0, 1>(gba, opcode); break;
        case 380: data_processing_reg<1, 11, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 381: halfword_data_transfer_immediate_offset<1, 0, 1, 1, 1, 0>(gba, opcode); break;
        case 382: data_processing_reg<1, 11, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 383: halfword_data_transfer_immediate_offset<1, 0, 1, 1, 1, 1>(gba, opcode); break;
        case 384: data_processing_reg<0, 12, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 385: data_processing_reg<0, 12, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 386: data_processing_reg<0, 12, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 387: data_processing_reg<0, 12, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 388: data_processing_reg<0, 12, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 389: data_processing_reg<0, 12, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 390: data_processing_reg<0, 12, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 391: data_processing_reg<0, 12, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 392: data_processing_reg<0, 12, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 393: halfword_data_transfer_register_offset<1, 1, 0, 0, 0, 0>(gba, opcode); break;
        case 394: data_processing_reg<0, 12, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 395: halfword_data_transfer_register_offset<1, 1, 0, 0, 0, 1>(gba, opcode); break;
        case 396: data_processing_reg<0, 12, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 397: halfword_data_transfer_register_offset<1, 1, 0, 0, 1, 0>(gba, opcode); break;
        case 398: data_processing_reg<0, 12, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 399: halfword_data_transfer_register_offset<1, 1, 0, 0, 1, 1>(gba, opcode); break;
        case 400: data_processing_reg<1, 12, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 401: data_processing_reg<1, 12, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 402: data_processing_reg<1, 12, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 403: data_processing_reg<1, 12, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 404: data_processing_reg<1, 12, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 405: data_processing_reg<1, 12, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 406: data_processing_reg<1, 12, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 407: data_processing_reg<1, 12, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 408: data_processing_reg<1, 12, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 409: halfword_data_transfer_register_offset<1, 1, 0, 1, 0, 0>(gba, opcode); break;
        case 410: data_processing_reg<1, 12, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 411: halfword_data_transfer_register_offset<1, 1, 0, 1, 0, 1>(gba, opcode); break;
        case 412: data_processing_reg<1, 12, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 413: halfword_data_transfer_register_offset<1, 1, 0, 1, 1, 0>(gba, opcode); break;
        case 414: data_processing_reg<1, 12, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 415: halfword_data_transfer_register_offset<1, 1, 0, 1, 1, 1>(gba, opcode); break;
        case 416: data_processing_reg<0, 13, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 417: data_processing_reg<0, 13, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 418: data_processing_reg<0, 13, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 419: data_processing_reg<0, 13, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 420: data_processing_reg<0, 13, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 421: data_processing_reg<0, 13, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 422: data_processing_reg<0, 13, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 423: data_processing_reg<0, 13, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 424: data_processing_reg<0, 13, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 425: halfword_data_transfer_register_offset<1, 1, 1, 0, 0, 0>(gba, opcode); break;
        case 426: data_processing_reg<0, 13, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 427: halfword_data_transfer_register_offset<1, 1, 1, 0, 0, 1>(gba, opcode); break;
        case 428: data_processing_reg<0, 13, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 429: halfword_data_transfer_register_offset<1, 1, 1, 0, 1, 0>(gba, opcode); break;
        case 430: data_processing_reg<0, 13, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 431: halfword_data_transfer_register_offset<1, 1, 1, 0, 1, 1>(gba, opcode); break;
        case 432: data_processing_reg<1, 13, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 433: data_processing_reg<1, 13, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 434: data_processing_reg<1, 13, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 435: data_processing_reg<1, 13, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 436: data_processing_reg<1, 13, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 437: data_processing_reg<1, 13, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 438: data_processing_reg<1, 13, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 439: data_processing_reg<1, 13, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 440: data_processing_reg<1, 13, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 441: halfword_data_transfer_register_offset<1, 1, 1, 1, 0, 0>(gba, opcode); break;
        case 442: data_processing_reg<1, 13, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 443: halfword_data_transfer_register_offset<1, 1, 1, 1, 0, 1>(gba, opcode); break;
        case 444: data_processing_reg<1, 13, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 445: halfword_data_transfer_register_offset<1, 1, 1, 1, 1, 0>(gba, opcode); break;
        case 446: data_processing_reg<1, 13, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 447: halfword_data_transfer_register_offset<1, 1, 1, 1, 1, 1>(gba, opcode); break;
        case 448: data_processing_reg<0, 14, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 449: data_processing_reg<0, 14, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 450: data_processing_reg<0, 14, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 451: data_processing_reg<0, 14, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 452: data_processing_reg<0, 14, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 453: data_processing_reg<0, 14, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 454: data_processing_reg<0, 14, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 455: data_processing_reg<0, 14, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 456: data_processing_reg<0, 14, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 457: halfword_data_transfer_immediate_offset<1, 1, 0, 0, 0, 0>(gba, opcode); break;
        case 458: data_processing_reg<0, 14, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 459: halfword_data_transfer_immediate_offset<1, 1, 0, 0, 0, 1>(gba, opcode); break;
        case 460: data_processing_reg<0, 14, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 461: halfword_data_transfer_immediate_offset<1, 1, 0, 0, 1, 0>(gba, opcode); break;
        case 462: data_processing_reg<0, 14, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 463: halfword_data_transfer_immediate_offset<1, 1, 0, 0, 1, 1>(gba, opcode); break;
        case 464: data_processing_reg<1, 14, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 465: data_processing_reg<1, 14, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 466: data_processing_reg<1, 14, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 467: data_processing_reg<1, 14, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 468: data_processing_reg<1, 14, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 469: data_processing_reg<1, 14, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 470: data_processing_reg<1, 14, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 471: data_processing_reg<1, 14, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 472: data_processing_reg<1, 14, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 473: halfword_data_transfer_immediate_offset<1, 1, 0, 1, 0, 0>(gba, opcode); break;
        case 474: data_processing_reg<1, 14, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 475: halfword_data_transfer_immediate_offset<1, 1, 0, 1, 0, 1>(gba, opcode); break;
        case 476: data_processing_reg<1, 14, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 477: halfword_data_transfer_immediate_offset<1, 1, 0, 1, 1, 0>(gba, opcode); break;
        case 478: data_processing_reg<1, 14, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 479: halfword_data_transfer_immediate_offset<1, 1, 0, 1, 1, 1>(gba, opcode); break;
        case 480: data_processing_reg<0, 15, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 481: data_processing_reg<0, 15, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 482: data_processing_reg<0, 15, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 483: data_processing_reg<0, 15, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 484: data_processing_reg<0, 15, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 485: data_processing_reg<0, 15, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 486: data_processing_reg<0, 15, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 487: data_processing_reg<0, 15, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 488: data_processing_reg<0, 15, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 489: halfword_data_transfer_immediate_offset<1, 1, 1, 0, 0, 0>(gba, opcode); break;
        case 490: data_processing_reg<0, 15, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 491: halfword_data_transfer_immediate_offset<1, 1, 1, 0, 0, 1>(gba, opcode); break;
        case 492: data_processing_reg<0, 15, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 493: halfword_data_transfer_immediate_offset<1, 1, 1, 0, 1, 0>(gba, opcode); break;
        case 494: data_processing_reg<0, 15, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 495: halfword_data_transfer_immediate_offset<1, 1, 1, 0, 1, 1>(gba, opcode); break;
        case 496: data_processing_reg<1, 15, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 497: data_processing_reg<1, 15, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 498: data_processing_reg<1, 15, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 499: data_processing_reg<1, 15, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 500: data_processing_reg<1, 15, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 501: data_processing_reg<1, 15, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 502: data_processing_reg<1, 15, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 503: data_processing_reg<1, 15, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 504: data_processing_reg<1, 15, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 505: halfword_data_transfer_immediate_offset<1, 1, 1, 1, 0, 0>(gba, opcode); break;
        case 506: data_processing_reg<1, 15, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 507: halfword_data_transfer_immediate_offset<1, 1, 1, 1, 0, 1>(gba, opcode); break;
        case 508: data_processing_reg<1, 15, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 509: halfword_data_transfer_immediate_offset<1, 1, 1, 1, 1, 0>(gba, opcode); break;
        case 510: data_processing_reg<1, 15, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 511: halfword_data_transfer_immediate_offset<1, 1, 1, 1, 1, 1>(gba, opcode); break;
        case 512:
        case 513:
        case 514:
        case 515:
        case 516:
        case 517:
        case 518:
        case 519:
        case 520:
        case 521:
        case 522:
        case 523:
        case 524:
        case 525:
        case 526:
        case 527: data_processing_imm<0, 0>(gba, opcode); break;
        case 528:
        case 529:
        case 530:
        case 531:
        case 532:
        case 533:
        case 534:
        case 535:
        case 536:
        case 537:
        case 538:
        case 539:
        case 540:
        case 541:
        case 542:
        case 543: data_processing_imm<1, 0>(gba, opcode); break;
        case 544:
        case 545:
        case 546:
        case 547:
        case 548:
        case 549:
        case 550:
        case 551:
        case 552:
        case 553:
        case 554:
        case 555:
        case 556:
        case 557:
        case 558:
        case 559: data_processing_imm<0, 1>(gba, opcode); break;
        case 560:
        case 561:
        case 562:
        case 563:
        case 564:
        case 565:
        case 566:
        case 567:
        case 568:
        case 569:
        case 570:
        case 571:
        case 572:
        case 573:
        case 574:
        case 575: data_processing_imm<1, 1>(gba, opcode); break;
        case 576:
        case 577:
        case 578:
        case 579:
        case 580:
        case 581:
        case 582:
        case 583:
        case 584:
        case 585:
        case 586:
        case 587:
        case 588:
        case 589:
        case 590:
        case 591: data_processing_imm<0, 2>(gba, opcode); break;
        case 592:
        case 593:
        case 594:
        case 595:
        case 596:
        case 597:
        case 598:
        case 599:
        case 600:
        case 601:
        case 602:
        case 603:
        case 604:
        case 605:
        case 606:
        case 607: data_processing_imm<1, 2>(gba, opcode); break;
        case 608:
        case 609:
        case 610:
        case 611:
        case 612:
        case 613:
        case 614:
        case 615:
        case 616:
        case 617:
        case 618:
        case 619:
        case 620:
        case 621:
        case 622:
        case 623: data_processing_imm<0, 3>(gba, opcode); break;
        case 624:
        case 625:
        case 626:
        case 627:
        case 628:
        case 629:
        case 630:
        case 631:
        case 632:
        case 633:
        case 634:
        case 635:
        case 636:
        case 637:
        case 638:
        case 639: data_processing_imm<1, 3>(gba, opcode); break;
        case 640:
        case 641:
        case 642:
        case 643:
        case 644:
        case 645:
        case 646:
        case 647:
        case 648:
        case 649:
        case 650:
        case 651:
        case 652:
        case 653:
        case 654:
        case 655: data_processing_imm<0, 4>(gba, opcode); break;
        case 656:
        case 657:
        case 658:
        case 659:
        case 660:
        case 661:
        case 662:
        case 663:
        case 664:
        case 665:
        case 666:
        case 667:
        case 668:
        case 669:
        case 670:
        case 671: data_processing_imm<1, 4>(gba, opcode); break;
        case 672:
        case 673:
        case 674:
        case 675:
        case 676:
        case 677:
        case 678:
        case 679:
        case 680:
        case 681:
        case 682:
        case 683:
        case 684:
        case 685:
        case 686:
        case 687: data_processing_imm<0, 5>(gba, opcode); break;
        case 688:
        case 689:
        case 690:
        case 691:
        case 692:
        case 693:
        case 694:
        case 695:
        case 696:
        case 697:
        case 698:
        case 699:
        case 700:
        case 701:
        case 702:
        case 703: data_processing_imm<1, 5>(gba, opcode); break;
        case 704:
        case 705:
        case 706:
        case 707:
        case 708:
        case 709:
        case 710:
        case 711:
        case 712:
        case 713:
        case 714:
        case 715:
        case 716:
        case 717:
        case 718:
        case 719: data_processing_imm<0, 6>(gba, opcode); break;
        case 720:
        case 721:
        case 722:
        case 723:
        case 724:
        case 725:
        case 726:
        case 727:
        case 728:
        case 729:
        case 730:
        case 731:
        case 732:
        case 733:
        case 734:
        case 735: data_processing_imm<1, 6>(gba, opcode); break;
        case 736:
        case 737:
        case 738:
        case 739:
        case 740:
        case 741:
        case 742:
        case 743:
        case 744:
        case 745:
        case 746:
        case 747:
        case 748:
        case 749:
        case 750:
        case 751: data_processing_imm<0, 7>(gba, opcode); break;
        case 752:
        case 753:
        case 754:
        case 755:
        case 756:
        case 757:
        case 758:
        case 759:
        case 760:
        case 761:
        case 762:
        case 763:
        case 764:
        case 765:
        case 766:
        case 767: data_processing_imm<1, 7>(gba, opcode); break;
        case 768:
        case 769:
        case 770:
        case 771:
        case 772:
        case 773:
        case 774:
        case 775:
        case 776:
        case 777:
        case 778:
        case 779:
        case 780:
        case 781:
        case 782:
        case 783: data_processing_imm<0, 8>(gba, opcode); break;
        case 784:
        case 785:
        case 786:
        case 787:
        case 788:
        case 789:
        case 790:
        case 791:
        case 792:
        case 793:
        case 794:
        case 795:
        case 796:
        case 797:
        case 798:
        case 799: data_processing_imm<1, 8>(gba, opcode); break;
        case 800:
        case 801:
        case 802:
        case 803:
        case 804:
        case 805:
        case 806:
        case 807:
        case 808:
        case 809:
        case 810:
        case 811:
        case 812:
        case 813:
        case 814:
        case 815: msr<1, 0>(gba, opcode); break;
        case 816:
        case 817:
        case 818:
        case 819:
        case 820:
        case 821:
        case 822:
        case 823:
        case 824:
        case 825:
        case 826:
        case 827:
        case 828:
        case 829:
        case 830:
        case 831: data_processing_imm<1, 9>(gba, opcode); break;
        case 832:
        case 833:
        case 834:
        case 835:
        case 836:
        case 837:
        case 838:
        case 839:
        case 840:
        case 841:
        case 842:
        case 843:
        case 844:
        case 845:
        case 846:
        case 847: data_processing_imm<0, 10>(gba, opcode); break;
        case 848:
        case 849:
        case 850:
        case 851:
        case 852:
        case 853:
        case 854:
        case 855:
        case 856:
        case 857:
        case 858:
        case 859:
        case 860:
        case 861:
        case 862:
        case 863: data_processing_imm<1, 10>(gba, opcode); break;
        case 864:
        case 865:
        case 866:
        case 867:
        case 868:
        case 869:
        case 870:
        case 871:
        case 872:
        case 873:
        case 874:
        case 875:
        case 876:
        case 877:
        case 878:
        case 879: msr<1, 1>(gba, opcode); break;
        case 880:
        case 881:
        case 882:
        case 883:
        case 884:
        case 885:
        case 886:
        case 887:
        case 888:
        case 889:
        case 890:
        case 891:
        case 892:
        case 893:
        case 894:
        case 895: data_processing_imm<1, 11>(gba, opcode); break;
        case 896:
        case 897:
        case 898:
        case 899:
        case 900:
        case 901:
        case 902:
        case 903:
        case 904:
        case 905:
        case 906:
        case 907:
        case 908:
        case 909:
        case 910:
        case 911: data_processing_imm<0, 12>(gba, opcode); break;
        case 912:
        case 913:
        case 914:
        case 915:
        case 916:
        case 917:
        case 918:
        case 919:
        case 920:
        case 921:
        case 922:
        case 923:
        case 924:
        case 925:
        case 926:
        case 927: data_processing_imm<1, 12>(gba, opcode); break;
        case 928:
        case 929:
        case 930:
        case 931:
        case 932:
        case 933:
        case 934:
        case 935:
        case 936:
        case 937:
        case 938:
        case 939:
        case 940:
        case 941:
        case 942:
        case 943: data_processing_imm<0, 13>(gba, opcode); break;
        case 944:
        case 945:
        case 946:
        case 947:
        case 948:
        case 949:
        case 950:
        case 951:
        case 952:
        case 953:
        case 954:
        case 955:
        case 956:
        case 957:
        case 958:
        case 959: data_processing_imm<1, 13>(gba, opcode); break;
        case 960:
        case 961:
        case 962:
        case 963:
        case 964:
        case 965:
        case 966:
        case 967:
        case 968:
        case 969:
        case 970:
        case 971:
        case 972:
        case 973:
        case 974:
        case 975: data_processing_imm<0, 14>(gba, opcode); break;
        case 976:
        case 977:
        case 978:
        case 979:
        case 980:
        case 981:
        case 982:
        case 983:
        case 984:
        case 985:
        case 986:
        case 987:
        case 988:
        case 989:
        case 990:
        case 991: data_processing_imm<1, 14>(gba, opcode); break;
        case 992:
        case 993:
        case 994:
        case 995:
        case 996:
        case 997:
        case 998:
        case 999:
        case 1000:
        case 1001:
        case 1002:
        case 1003:
        case 1004:
        case 1005:
        case 1006:
        case 1007: data_processing_imm<0, 15>(gba, opcode); break;
        case 1008:
        case 1009:
        case 1010:
        case 1011:
        case 1012:
        case 1013:
        case 1014:
        case 1015:
        case 1016:
        case 1017:
        case 1018:
        case 1019:
        case 1020:
        case 1021:
        case 1022:
        case 1023: data_processing_imm<1, 15>(gba, opcode); break;
        case 1024:
        case 1025:
        case 1026:
        case 1027:
        case 1028:
        case 1029:
        case 1030:
        case 1031:
        case 1032:
        case 1033:
        case 1034:
        case 1035:
        case 1036:
        case 1037:
        case 1038:
        case 1039: single_data_transfer_imm<0, 0, 0, 0, 0>(gba, opcode); break;
        case 1040:
        case 1041:
        case 1042:
        case 1043:
        case 1044:
        case 1045:
        case 1046:
        case 1047:
        case 1048:
        case 1049:
        case 1050:
        case 1051:
        case 1052:
        case 1053:
        case 1054:
        case 1055: single_data_transfer_imm<0, 0, 1, 0, 0>(gba, opcode); break;
        case 1056:
        case 1057:
        case 1058:
        case 1059:
        case 1060:
        case 1061:
        case 1062:
        case 1063:
        case 1064:
        case 1065:
        case 1066:
        case 1067:
        case 1068:
        case 1069:
        case 1070:
        case 1071: single_data_transfer_imm<0, 0, 0, 0, 1>(gba, opcode); break;
        case 1072:
        case 1073:
        case 1074:
        case 1075:
        case 1076:
        case 1077:
        case 1078:
        case 1079:
        case 1080:
        case 1081:
        case 1082:
        case 1083:
        case 1084:
        case 1085:
        case 1086:
        case 1087: single_data_transfer_imm<0, 0, 1, 0, 1>(gba, opcode); break;
        case 1088:
        case 1089:
        case 1090:
        case 1091:
        case 1092:
        case 1093:
        case 1094:
        case 1095:
        case 1096:
        case 1097:
        case 1098:
        case 1099:
        case 1100:
        case 1101:
        case 1102:
        case 1103: single_data_transfer_imm<0, 0, 0, 1, 0>(gba, opcode); break;
        case 1104:
        case 1105:
        case 1106:
        case 1107:
        case 1108:
        case 1109:
        case 1110:
        case 1111:
        case 1112:
        case 1113:
        case 1114:
        case 1115:
        case 1116:
        case 1117:
        case 1118:
        case 1119: single_data_transfer_imm<0, 0, 1, 1, 0>(gba, opcode); break;
        case 1120:
        case 1121:
        case 1122:
        case 1123:
        case 1124:
        case 1125:
        case 1126:
        case 1127:
        case 1128:
        case 1129:
        case 1130:
        case 1131:
        case 1132:
        case 1133:
        case 1134:
        case 1135: single_data_transfer_imm<0, 0, 0, 1, 1>(gba, opcode); break;
        case 1136:
        case 1137:
        case 1138:
        case 1139:
        case 1140:
        case 1141:
        case 1142:
        case 1143:
        case 1144:
        case 1145:
        case 1146:
        case 1147:
        case 1148:
        case 1149:
        case 1150:
        case 1151: single_data_transfer_imm<0, 0, 1, 1, 1>(gba, opcode); break;
        case 1152:
        case 1153:
        case 1154:
        case 1155:
        case 1156:
        case 1157:
        case 1158:
        case 1159:
        case 1160:
        case 1161:
        case 1162:
        case 1163:
        case 1164:
        case 1165:
        case 1166:
        case 1167: single_data_transfer_imm<0, 1, 0, 0, 0>(gba, opcode); break;
        case 1168:
        case 1169:
        case 1170:
        case 1171:
        case 1172:
        case 1173:
        case 1174:
        case 1175:
        case 1176:
        case 1177:
        case 1178:
        case 1179:
        case 1180:
        case 1181:
        case 1182:
        case 1183: single_data_transfer_imm<0, 1, 1, 0, 0>(gba, opcode); break;
        case 1184:
        case 1185:
        case 1186:
        case 1187:
        case 1188:
        case 1189:
        case 1190:
        case 1191:
        case 1192:
        case 1193:
        case 1194:
        case 1195:
        case 1196:
        case 1197:
        case 1198:
        case 1199: single_data_transfer_imm<0, 1, 0, 0, 1>(gba, opcode); break;
        case 1200:
        case 1201:
        case 1202:
        case 1203:
        case 1204:
        case 1205:
        case 1206:
        case 1207:
        case 1208:
        case 1209:
        case 1210:
        case 1211:
        case 1212:
        case 1213:
        case 1214:
        case 1215: single_data_transfer_imm<0, 1, 1, 0, 1>(gba, opcode); break;
        case 1216:
        case 1217:
        case 1218:
        case 1219:
        case 1220:
        case 1221:
        case 1222:
        case 1223:
        case 1224:
        case 1225:
        case 1226:
        case 1227:
        case 1228:
        case 1229:
        case 1230:
        case 1231: single_data_transfer_imm<0, 1, 0, 1, 0>(gba, opcode); break;
        case 1232:
        case 1233:
        case 1234:
        case 1235:
        case 1236:
        case 1237:
        case 1238:
        case 1239:
        case 1240:
        case 1241:
        case 1242:
        case 1243:
        case 1244:
        case 1245:
        case 1246:
        case 1247: single_data_transfer_imm<0, 1, 1, 1, 0>(gba, opcode); break;
        case 1248:
        case 1249:
        case 1250:
        case 1251:
        case 1252:
        case 1253:
        case 1254:
        case 1255:
        case 1256:
        case 1257:
        case 1258:
        case 1259:
        case 1260:
        case 1261:
        case 1262:
        case 1263: single_data_transfer_imm<0, 1, 0, 1, 1>(gba, opcode); break;
        case 1264:
        case 1265:
        case 1266:
        case 1267:
        case 1268:
        case 1269:
        case 1270:
        case 1271:
        case 1272:
        case 1273:
        case 1274:
        case 1275:
        case 1276:
        case 1277:
        case 1278:
        case 1279: single_data_transfer_imm<0, 1, 1, 1, 1>(gba, opcode); break;
        case 1280:
        case 1281:
        case 1282:
        case 1283:
        case 1284:
        case 1285:
        case 1286:
        case 1287:
        case 1288:
        case 1289:
        case 1290:
        case 1291:
        case 1292:
        case 1293:
        case 1294:
        case 1295: single_data_transfer_imm<1, 0, 0, 0, 0>(gba, opcode); break;
        case 1296:
        case 1297:
        case 1298:
        case 1299:
        case 1300:
        case 1301:
        case 1302:
        case 1303:
        case 1304:
        case 1305:
        case 1306:
        case 1307:
        case 1308:
        case 1309:
        case 1310:
        case 1311: single_data_transfer_imm<1, 0, 1, 0, 0>(gba, opcode); break;
        case 1312:
        case 1313:
        case 1314:
        case 1315:
        case 1316:
        case 1317:
        case 1318:
        case 1319:
        case 1320:
        case 1321:
        case 1322:
        case 1323:
        case 1324:
        case 1325:
        case 1326:
        case 1327: single_data_transfer_imm<1, 0, 0, 0, 1>(gba, opcode); break;
        case 1328:
        case 1329:
        case 1330:
        case 1331:
        case 1332:
        case 1333:
        case 1334:
        case 1335:
        case 1336:
        case 1337:
        case 1338:
        case 1339:
        case 1340:
        case 1341:
        case 1342:
        case 1343: single_data_transfer_imm<1, 0, 1, 0, 1>(gba, opcode); break;
        case 1344:
        case 1345:
        case 1346:
        case 1347:
        case 1348:
        case 1349:
        case 1350:
        case 1351:
        case 1352:
        case 1353:
        case 1354:
        case 1355:
        case 1356:
        case 1357:
        case 1358:
        case 1359: single_data_transfer_imm<1, 0, 0, 1, 0>(gba, opcode); break;
        case 1360:
        case 1361:
        case 1362:
        case 1363:
        case 1364:
        case 1365:
        case 1366:
        case 1367:
        case 1368:
        case 1369:
        case 1370:
        case 1371:
        case 1372:
        case 1373:
        case 1374:
        case 1375: single_data_transfer_imm<1, 0, 1, 1, 0>(gba, opcode); break;
        case 1376:
        case 1377:
        case 1378:
        case 1379:
        case 1380:
        case 1381:
        case 1382:
        case 1383:
        case 1384:
        case 1385:
        case 1386:
        case 1387:
        case 1388:
        case 1389:
        case 1390:
        case 1391: single_data_transfer_imm<1, 0, 0, 1, 1>(gba, opcode); break;
        case 1392:
        case 1393:
        case 1394:
        case 1395:
        case 1396:
        case 1397:
        case 1398:
        case 1399:
        case 1400:
        case 1401:
        case 1402:
        case 1403:
        case 1404:
        case 1405:
        case 1406:
        case 1407: single_data_transfer_imm<1, 0, 1, 1, 1>(gba, opcode); break;
        case 1408:
        case 1409:
        case 1410:
        case 1411:
        case 1412:
        case 1413:
        case 1414:
        case 1415:
        case 1416:
        case 1417:
        case 1418:
        case 1419:
        case 1420:
        case 1421:
        case 1422:
        case 1423: single_data_transfer_imm<1, 1, 0, 0, 0>(gba, opcode); break;
        case 1424:
        case 1425:
        case 1426:
        case 1427:
        case 1428:
        case 1429:
        case 1430:
        case 1431:
        case 1432:
        case 1433:
        case 1434:
        case 1435:
        case 1436:
        case 1437:
        case 1438:
        case 1439: single_data_transfer_imm<1, 1, 1, 0, 0>(gba, opcode); break;
        case 1440:
        case 1441:
        case 1442:
        case 1443:
        case 1444:
        case 1445:
        case 1446:
        case 1447:
        case 1448:
        case 1449:
        case 1450:
        case 1451:
        case 1452:
        case 1453:
        case 1454:
        case 1455: single_data_transfer_imm<1, 1, 0, 0, 1>(gba, opcode); break;
        case 1456:
        case 1457:
        case 1458:
        case 1459:
        case 1460:
        case 1461:
        case 1462:
        case 1463:
        case 1464:
        case 1465:
        case 1466:
        case 1467:
        case 1468:
        case 1469:
        case 1470:
        case 1471: single_data_transfer_imm<1, 1, 1, 0, 1>(gba, opcode); break;
        case 1472:
        case 1473:
        case 1474:
        case 1475:
        case 1476:
        case 1477:
        case 1478:
        case 1479:
        case 1480:
        case 1481:
        case 1482:
        case 1483:
        case 1484:
        case 1485:
        case 1486:
        case 1487: single_data_transfer_imm<1, 1, 0, 1, 0>(gba, opcode); break;
        case 1488:
        case 1489:
        case 1490:
        case 1491:
        case 1492:
        case 1493:
        case 1494:
        case 1495:
        case 1496:
        case 1497:
        case 1498:
        case 1499:
        case 1500:
        case 1501:
        case 1502:
        case 1503: single_data_transfer_imm<1, 1, 1, 1, 0>(gba, opcode); break;
        case 1504:
        case 1505:
        case 1506:
        case 1507:
        case 1508:
        case 1509:
        case 1510:
        case 1511:
        case 1512:
        case 1513:
        case 1514:
        case 1515:
        case 1516:
        case 1517:
        case 1518:
        case 1519: single_data_transfer_imm<1, 1, 0, 1, 1>(gba, opcode); break;
        case 1520:
        case 1521:
        case 1522:
        case 1523:
        case 1524:
        case 1525:
        case 1526:
        case 1527:
        case 1528:
        case 1529:
        case 1530:
        case 1531:
        case 1532:
        case 1533:
        case 1534:
        case 1535: single_data_transfer_imm<1, 1, 1, 1, 1>(gba, opcode); break;
        case 1536: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1537: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1538: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1539: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1540: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1541: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1542: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1543: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1544: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1545: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1546: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1547: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1548: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1549: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1550: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1551: single_data_transfer_reg<0, 0, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1552: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1553: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1554: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1555: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1556: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1557: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1558: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1559: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1560: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1561: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1562: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1563: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1564: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1565: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1566: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1567: single_data_transfer_reg<0, 0, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1568: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1569: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1570: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1571: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1572: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1573: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1574: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1575: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1576: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1577: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1578: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1579: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1580: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1581: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1582: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1583: single_data_transfer_reg<0, 0, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1584: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1585: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1586: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1587: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1588: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1589: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1590: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1591: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1592: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1593: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1594: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1595: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1596: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1597: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1598: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1599: single_data_transfer_reg<0, 0, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1600: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1601: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1602: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1603: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1604: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1605: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1606: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1607: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1608: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1609: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1610: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1611: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1612: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1613: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1614: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1615: single_data_transfer_reg<0, 0, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1616: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1617: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1618: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1619: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1620: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1621: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1622: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1623: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1624: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1625: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1626: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1627: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1628: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1629: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1630: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1631: single_data_transfer_reg<0, 0, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1632: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1633: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1634: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1635: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1636: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1637: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1638: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1639: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1640: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1641: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1642: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1643: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1644: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1645: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1646: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1647: single_data_transfer_reg<0, 0, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1648: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1649: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1650: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1651: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1652: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1653: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1654: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1655: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1656: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1657: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1658: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1659: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1660: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1661: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1662: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1663: single_data_transfer_reg<0, 0, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1664: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1665: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1666: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1667: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1668: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1669: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1670: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1671: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1672: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1673: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1674: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1675: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1676: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1677: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1678: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1679: single_data_transfer_reg<0, 1, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1680: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1681: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1682: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1683: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1684: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1685: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1686: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1687: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1688: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1689: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1690: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1691: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1692: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1693: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1694: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1695: single_data_transfer_reg<0, 1, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1696: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1697: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1698: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1699: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1700: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1701: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1702: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1703: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1704: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1705: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1706: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1707: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1708: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1709: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1710: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1711: single_data_transfer_reg<0, 1, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1712: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1713: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1714: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1715: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1716: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1717: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1718: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1719: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1720: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1721: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1722: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1723: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1724: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1725: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1726: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1727: single_data_transfer_reg<0, 1, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1728: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1729: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1730: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1731: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1732: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1733: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1734: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1735: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1736: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1737: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1738: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1739: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1740: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1741: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1742: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1743: single_data_transfer_reg<0, 1, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1744: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1745: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1746: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1747: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1748: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1749: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1750: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1751: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1752: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1753: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1754: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1755: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1756: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1757: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1758: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1759: single_data_transfer_reg<0, 1, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1760: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1761: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1762: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1763: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1764: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1765: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1766: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1767: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1768: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1769: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1770: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1771: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1772: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1773: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1774: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1775: single_data_transfer_reg<0, 1, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1776: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1777: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1778: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1779: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1780: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1781: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1782: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1783: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1784: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1785: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1786: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1787: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1788: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1789: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1790: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1791: single_data_transfer_reg<0, 1, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1792: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1793: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1794: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1795: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1796: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1797: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1798: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1799: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1800: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1801: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1802: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1803: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1804: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1805: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1806: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1807: single_data_transfer_reg<1, 0, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1808: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1809: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1810: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1811: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1812: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1813: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1814: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1815: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1816: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1817: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1818: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1819: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1820: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1821: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1822: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1823: single_data_transfer_reg<1, 0, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1824: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1825: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1826: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1827: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1828: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1829: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1830: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1831: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1832: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1833: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1834: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1835: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1836: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1837: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1838: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1839: single_data_transfer_reg<1, 0, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1840: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1841: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1842: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1843: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1844: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1845: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1846: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1847: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1848: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1849: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1850: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1851: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1852: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1853: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1854: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1855: single_data_transfer_reg<1, 0, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1856: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1857: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1858: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1859: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1860: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1861: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1862: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1863: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1864: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1865: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1866: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1867: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1868: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1869: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1870: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1871: single_data_transfer_reg<1, 0, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1872: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1873: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1874: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1875: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1876: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1877: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1878: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1879: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1880: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1881: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1882: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1883: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1884: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1885: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1886: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1887: single_data_transfer_reg<1, 0, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1888: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1889: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1890: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1891: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1892: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1893: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1894: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1895: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1896: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1897: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1898: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1899: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1900: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1901: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1902: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1903: single_data_transfer_reg<1, 0, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1904: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1905: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1906: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1907: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1908: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1909: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1910: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1911: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1912: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1913: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1914: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1915: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1916: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1917: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1918: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1919: single_data_transfer_reg<1, 0, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1920: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1921: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1922: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1923: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1924: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1925: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1926: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1927: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1928: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1929: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1930: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1931: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1932: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1933: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1934: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1935: single_data_transfer_reg<1, 1, 0, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1936: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1937: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1938: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1939: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1940: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1941: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1942: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1943: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1944: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1945: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1946: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1947: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1948: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1949: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1950: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1951: single_data_transfer_reg<1, 1, 1, 0, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1952: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1953: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1954: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1955: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1956: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1957: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1958: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1959: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1960: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1961: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1962: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1963: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1964: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1965: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1966: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1967: single_data_transfer_reg<1, 1, 0, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1968: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1969: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1970: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1971: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1972: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1973: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1974: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1975: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1976: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1977: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1978: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1979: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1980: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1981: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1982: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1983: single_data_transfer_reg<1, 1, 1, 0, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1984: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1985: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1986: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1987: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1988: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1989: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1990: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1991: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 1992: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 1993: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 1994: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 1995: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 1996: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 1997: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 1998: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 1999: single_data_transfer_reg<1, 1, 0, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2000: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 2001: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2002: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 2003: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 2004: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 2005: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 2006: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 2007: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2008: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 2009: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2010: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 2011: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 2012: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 2013: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 2014: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 2015: single_data_transfer_reg<1, 1, 1, 1, 0, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2016: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 2017: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2018: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 2019: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 2020: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 2021: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 2022: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 2023: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2024: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 2025: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2026: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 2027: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 2028: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 2029: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 2030: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 2031: single_data_transfer_reg<1, 1, 0, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2032: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 2033: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2034: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 2035: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 2036: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 2037: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 2038: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 2039: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2040: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(0), 0>(gba, opcode); break;
        case 2041: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(0), 1>(gba, opcode); break;
        case 2042: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(1), 0>(gba, opcode); break;
        case 2043: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(1), 1>(gba, opcode); break;
        case 2044: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(2), 0>(gba, opcode); break;
        case 2045: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(2), 1>(gba, opcode); break;
        case 2046: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(3), 0>(gba, opcode); break;
        case 2047: single_data_transfer_reg<1, 1, 1, 1, 1, static_cast<barrel::type>(3), 1>(gba, opcode); break;
        case 2048:
        case 2049:
        case 2050:
        case 2051:
        case 2052:
        case 2053:
        case 2054:
        case 2055:
        case 2056:
        case 2057:
        case 2058:
        case 2059:
        case 2060:
        case 2061:
        case 2062:
        case 2063: block_data_transfer<0, 0, 0, 0, 0>(gba, opcode); break;
        case 2064:
        case 2065:
        case 2066:
        case 2067:
        case 2068:
        case 2069:
        case 2070:
        case 2071:
        case 2072:
        case 2073:
        case 2074:
        case 2075:
        case 2076:
        case 2077:
        case 2078:
        case 2079: block_data_transfer<0, 0, 0, 0, 1>(gba, opcode); break;
        case 2080:
        case 2081:
        case 2082:
        case 2083:
        case 2084:
        case 2085:
        case 2086:
        case 2087:
        case 2088:
        case 2089:
        case 2090:
        case 2091:
        case 2092:
        case 2093:
        case 2094:
        case 2095: block_data_transfer<0, 0, 0, 1, 0>(gba, opcode); break;
        case 2096:
        case 2097:
        case 2098:
        case 2099:
        case 2100:
        case 2101:
        case 2102:
        case 2103:
        case 2104:
        case 2105:
        case 2106:
        case 2107:
        case 2108:
        case 2109:
        case 2110:
        case 2111: block_data_transfer<0, 0, 0, 1, 1>(gba, opcode); break;
        case 2112:
        case 2113:
        case 2114:
        case 2115:
        case 2116:
        case 2117:
        case 2118:
        case 2119:
        case 2120:
        case 2121:
        case 2122:
        case 2123:
        case 2124:
        case 2125:
        case 2126:
        case 2127: block_data_transfer<0, 0, 1, 0, 0>(gba, opcode); break;
        case 2128:
        case 2129:
        case 2130:
        case 2131:
        case 2132:
        case 2133:
        case 2134:
        case 2135:
        case 2136:
        case 2137:
        case 2138:
        case 2139:
        case 2140:
        case 2141:
        case 2142:
        case 2143: block_data_transfer<0, 0, 1, 0, 1>(gba, opcode); break;
        case 2144:
        case 2145:
        case 2146:
        case 2147:
        case 2148:
        case 2149:
        case 2150:
        case 2151:
        case 2152:
        case 2153:
        case 2154:
        case 2155:
        case 2156:
        case 2157:
        case 2158:
        case 2159: block_data_transfer<0, 0, 1, 1, 0>(gba, opcode); break;
        case 2160:
        case 2161:
        case 2162:
        case 2163:
        case 2164:
        case 2165:
        case 2166:
        case 2167:
        case 2168:
        case 2169:
        case 2170:
        case 2171:
        case 2172:
        case 2173:
        case 2174:
        case 2175: block_data_transfer<0, 0, 1, 1, 1>(gba, opcode); break;
        case 2176:
        case 2177:
        case 2178:
        case 2179:
        case 2180:
        case 2181:
        case 2182:
        case 2183:
        case 2184:
        case 2185:
        case 2186:
        case 2187:
        case 2188:
        case 2189:
        case 2190:
        case 2191: block_data_transfer<0, 1, 0, 0, 0>(gba, opcode); break;
        case 2192:
        case 2193:
        case 2194:
        case 2195:
        case 2196:
        case 2197:
        case 2198:
        case 2199:
        case 2200:
        case 2201:
        case 2202:
        case 2203:
        case 2204:
        case 2205:
        case 2206:
        case 2207: block_data_transfer<0, 1, 0, 0, 1>(gba, opcode); break;
        case 2208:
        case 2209:
        case 2210:
        case 2211:
        case 2212:
        case 2213:
        case 2214:
        case 2215:
        case 2216:
        case 2217:
        case 2218:
        case 2219:
        case 2220:
        case 2221:
        case 2222:
        case 2223: block_data_transfer<0, 1, 0, 1, 0>(gba, opcode); break;
        case 2224:
        case 2225:
        case 2226:
        case 2227:
        case 2228:
        case 2229:
        case 2230:
        case 2231:
        case 2232:
        case 2233:
        case 2234:
        case 2235:
        case 2236:
        case 2237:
        case 2238:
        case 2239: block_data_transfer<0, 1, 0, 1, 1>(gba, opcode); break;
        case 2240:
        case 2241:
        case 2242:
        case 2243:
        case 2244:
        case 2245:
        case 2246:
        case 2247:
        case 2248:
        case 2249:
        case 2250:
        case 2251:
        case 2252:
        case 2253:
        case 2254:
        case 2255: block_data_transfer<0, 1, 1, 0, 0>(gba, opcode); break;
        case 2256:
        case 2257:
        case 2258:
        case 2259:
        case 2260:
        case 2261:
        case 2262:
        case 2263:
        case 2264:
        case 2265:
        case 2266:
        case 2267:
        case 2268:
        case 2269:
        case 2270:
        case 2271: block_data_transfer<0, 1, 1, 0, 1>(gba, opcode); break;
        case 2272:
        case 2273:
        case 2274:
        case 2275:
        case 2276:
        case 2277:
        case 2278:
        case 2279:
        case 2280:
        case 2281:
        case 2282:
        case 2283:
        case 2284:
        case 2285:
        case 2286:
        case 2287: block_data_transfer<0, 1, 1, 1, 0>(gba, opcode); break;
        case 2288:
        case 2289:
        case 2290:
        case 2291:
        case 2292:
        case 2293:
        case 2294:
        case 2295:
        case 2296:
        case 2297:
        case 2298:
        case 2299:
        case 2300:
        case 2301:
        case 2302:
        case 2303: block_data_transfer<0, 1, 1, 1, 1>(gba, opcode); break;
        case 2304:
        case 2305:
        case 2306:
        case 2307:
        case 2308:
        case 2309:
        case 2310:
        case 2311:
        case 2312:
        case 2313:
        case 2314:
        case 2315:
        case 2316:
        case 2317:
        case 2318:
        case 2319: block_data_transfer<1, 0, 0, 0, 0>(gba, opcode); break;
        case 2320:
        case 2321:
        case 2322:
        case 2323:
        case 2324:
        case 2325:
        case 2326:
        case 2327:
        case 2328:
        case 2329:
        case 2330:
        case 2331:
        case 2332:
        case 2333:
        case 2334:
        case 2335: block_data_transfer<1, 0, 0, 0, 1>(gba, opcode); break;
        case 2336:
        case 2337:
        case 2338:
        case 2339:
        case 2340:
        case 2341:
        case 2342:
        case 2343:
        case 2344:
        case 2345:
        case 2346:
        case 2347:
        case 2348:
        case 2349:
        case 2350:
        case 2351: block_data_transfer<1, 0, 0, 1, 0>(gba, opcode); break;
        case 2352:
        case 2353:
        case 2354:
        case 2355:
        case 2356:
        case 2357:
        case 2358:
        case 2359:
        case 2360:
        case 2361:
        case 2362:
        case 2363:
        case 2364:
        case 2365:
        case 2366:
        case 2367: block_data_transfer<1, 0, 0, 1, 1>(gba, opcode); break;
        case 2368:
        case 2369:
        case 2370:
        case 2371:
        case 2372:
        case 2373:
        case 2374:
        case 2375:
        case 2376:
        case 2377:
        case 2378:
        case 2379:
        case 2380:
        case 2381:
        case 2382:
        case 2383: block_data_transfer<1, 0, 1, 0, 0>(gba, opcode); break;
        case 2384:
        case 2385:
        case 2386:
        case 2387:
        case 2388:
        case 2389:
        case 2390:
        case 2391:
        case 2392:
        case 2393:
        case 2394:
        case 2395:
        case 2396:
        case 2397:
        case 2398:
        case 2399: block_data_transfer<1, 0, 1, 0, 1>(gba, opcode); break;
        case 2400:
        case 2401:
        case 2402:
        case 2403:
        case 2404:
        case 2405:
        case 2406:
        case 2407:
        case 2408:
        case 2409:
        case 2410:
        case 2411:
        case 2412:
        case 2413:
        case 2414:
        case 2415: block_data_transfer<1, 0, 1, 1, 0>(gba, opcode); break;
        case 2416:
        case 2417:
        case 2418:
        case 2419:
        case 2420:
        case 2421:
        case 2422:
        case 2423:
        case 2424:
        case 2425:
        case 2426:
        case 2427:
        case 2428:
        case 2429:
        case 2430:
        case 2431: block_data_transfer<1, 0, 1, 1, 1>(gba, opcode); break;
        case 2432:
        case 2433:
        case 2434:
        case 2435:
        case 2436:
        case 2437:
        case 2438:
        case 2439:
        case 2440:
        case 2441:
        case 2442:
        case 2443:
        case 2444:
        case 2445:
        case 2446:
        case 2447: block_data_transfer<1, 1, 0, 0, 0>(gba, opcode); break;
        case 2448:
        case 2449:
        case 2450:
        case 2451:
        case 2452:
        case 2453:
        case 2454:
        case 2455:
        case 2456:
        case 2457:
        case 2458:
        case 2459:
        case 2460:
        case 2461:
        case 2462:
        case 2463: block_data_transfer<1, 1, 0, 0, 1>(gba, opcode); break;
        case 2464:
        case 2465:
        case 2466:
        case 2467:
        case 2468:
        case 2469:
        case 2470:
        case 2471:
        case 2472:
        case 2473:
        case 2474:
        case 2475:
        case 2476:
        case 2477:
        case 2478:
        case 2479: block_data_transfer<1, 1, 0, 1, 0>(gba, opcode); break;
        case 2480:
        case 2481:
        case 2482:
        case 2483:
        case 2484:
        case 2485:
        case 2486:
        case 2487:
        case 2488:
        case 2489:
        case 2490:
        case 2491:
        case 2492:
        case 2493:
        case 2494:
        case 2495: block_data_transfer<1, 1, 0, 1, 1>(gba, opcode); break;
        case 2496:
        case 2497:
        case 2498:
        case 2499:
        case 2500:
        case 2501:
        case 2502:
        case 2503:
        case 2504:
        case 2505:
        case 2506:
        case 2507:
        case 2508:
        case 2509:
        case 2510:
        case 2511: block_data_transfer<1, 1, 1, 0, 0>(gba, opcode); break;
        case 2512:
        case 2513:
        case 2514:
        case 2515:
        case 2516:
        case 2517:
        case 2518:
        case 2519:
        case 2520:
        case 2521:
        case 2522:
        case 2523:
        case 2524:
        case 2525:
        case 2526:
        case 2527: block_data_transfer<1, 1, 1, 0, 1>(gba, opcode); break;
        case 2528:
        case 2529:
        case 2530:
        case 2531:
        case 2532:
        case 2533:
        case 2534:
        case 2535:
        case 2536:
        case 2537:
        case 2538:
        case 2539:
        case 2540:
        case 2541:
        case 2542:
        case 2543: block_data_transfer<1, 1, 1, 1, 0>(gba, opcode); break;
        case 2544:
        case 2545:
        case 2546:
        case 2547:
        case 2548:
        case 2549:
        case 2550:
        case 2551:
        case 2552:
        case 2553:
        case 2554:
        case 2555:
        case 2556:
        case 2557:
        case 2558:
        case 2559: block_data_transfer<1, 1, 1, 1, 1>(gba, opcode); break;
        case 2560:
        case 2561:
        case 2562:
        case 2563:
        case 2564:
        case 2565:
        case 2566:
        case 2567:
        case 2568:
        case 2569:
        case 2570:
        case 2571:
        case 2572:
        case 2573:
        case 2574:
        case 2575:
        case 2576:
        case 2577:
        case 2578:
        case 2579:
        case 2580:
        case 2581:
        case 2582:
        case 2583:
        case 2584:
        case 2585:
        case 2586:
        case 2587:
        case 2588:
        case 2589:
        case 2590:
        case 2591:
        case 2592:
        case 2593:
        case 2594:
        case 2595:
        case 2596:
        case 2597:
        case 2598:
        case 2599:
        case 2600:
        case 2601:
        case 2602:
        case 2603:
        case 2604:
        case 2605:
        case 2606:
        case 2607:
        case 2608:
        case 2609:
        case 2610:
        case 2611:
        case 2612:
        case 2613:
        case 2614:
        case 2615:
        case 2616:
        case 2617:
        case 2618:
        case 2619:
        case 2620:
        case 2621:
        case 2622:
        case 2623:
        case 2624:
        case 2625:
        case 2626:
        case 2627:
        case 2628:
        case 2629:
        case 2630:
        case 2631:
        case 2632:
        case 2633:
        case 2634:
        case 2635:
        case 2636:
        case 2637:
        case 2638:
        case 2639:
        case 2640:
        case 2641:
        case 2642:
        case 2643:
        case 2644:
        case 2645:
        case 2646:
        case 2647:
        case 2648:
        case 2649:
        case 2650:
        case 2651:
        case 2652:
        case 2653:
        case 2654:
        case 2655:
        case 2656:
        case 2657:
        case 2658:
        case 2659:
        case 2660:
        case 2661:
        case 2662:
        case 2663:
        case 2664:
        case 2665:
        case 2666:
        case 2667:
        case 2668:
        case 2669:
        case 2670:
        case 2671:
        case 2672:
        case 2673:
        case 2674:
        case 2675:
        case 2676:
        case 2677:
        case 2678:
        case 2679:
        case 2680:
        case 2681:
        case 2682:
        case 2683:
        case 2684:
        case 2685:
        case 2686:
        case 2687:
        case 2688:
        case 2689:
        case 2690:
        case 2691:
        case 2692:
        case 2693:
        case 2694:
        case 2695:
        case 2696:
        case 2697:
        case 2698:
        case 2699:
        case 2700:
        case 2701:
        case 2702:
        case 2703:
        case 2704:
        case 2705:
        case 2706:
        case 2707:
        case 2708:
        case 2709:
        case 2710:
        case 2711:
        case 2712:
        case 2713:
        case 2714:
        case 2715:
        case 2716:
        case 2717:
        case 2718:
        case 2719:
        case 2720:
        case 2721:
        case 2722:
        case 2723:
        case 2724:
        case 2725:
        case 2726:
        case 2727:
        case 2728:
        case 2729:
        case 2730:
        case 2731:
        case 2732:
        case 2733:
        case 2734:
        case 2735:
        case 2736:
        case 2737:
        case 2738:
        case 2739:
        case 2740:
        case 2741:
        case 2742:
        case 2743:
        case 2744:
        case 2745:
        case 2746:
        case 2747:
        case 2748:
        case 2749:
        case 2750:
        case 2751:
        case 2752:
        case 2753:
        case 2754:
        case 2755:
        case 2756:
        case 2757:
        case 2758:
        case 2759:
        case 2760:
        case 2761:
        case 2762:
        case 2763:
        case 2764:
        case 2765:
        case 2766:
        case 2767:
        case 2768:
        case 2769:
        case 2770:
        case 2771:
        case 2772:
        case 2773:
        case 2774:
        case 2775:
        case 2776:
        case 2777:
        case 2778:
        case 2779:
        case 2780:
        case 2781:
        case 2782:
        case 2783:
        case 2784:
        case 2785:
        case 2786:
        case 2787:
        case 2788:
        case 2789:
        case 2790:
        case 2791:
        case 2792:
        case 2793:
        case 2794:
        case 2795:
        case 2796:
        case 2797:
        case 2798:
        case 2799:
        case 2800:
        case 2801:
        case 2802:
        case 2803:
        case 2804:
        case 2805:
        case 2806:
        case 2807:
        case 2808:
        case 2809:
        case 2810:
        case 2811:
        case 2812:
        case 2813:
        case 2814:
        case 2815: branch<0>(gba, opcode); break;
        case 2816:
        case 2817:
        case 2818:
        case 2819:
        case 2820:
        case 2821:
        case 2822:
        case 2823:
        case 2824:
        case 2825:
        case 2826:
        case 2827:
        case 2828:
        case 2829:
        case 2830:
        case 2831:
        case 2832:
        case 2833:
        case 2834:
        case 2835:
        case 2836:
        case 2837:
        case 2838:
        case 2839:
        case 2840:
        case 2841:
        case 2842:
        case 2843:
        case 2844:
        case 2845:
        case 2846:
        case 2847:
        case 2848:
        case 2849:
        case 2850:
        case 2851:
        case 2852:
        case 2853:
        case 2854:
        case 2855:
        case 2856:
        case 2857:
        case 2858:
        case 2859:
        case 2860:
        case 2861:
        case 2862:
        case 2863:
        case 2864:
        case 2865:
        case 2866:
        case 2867:
        case 2868:
        case 2869:
        case 2870:
        case 2871:
        case 2872:
        case 2873:
        case 2874:
        case 2875:
        case 2876:
        case 2877:
        case 2878:
        case 2879:
        case 2880:
        case 2881:
        case 2882:
        case 2883:
        case 2884:
        case 2885:
        case 2886:
        case 2887:
        case 2888:
        case 2889:
        case 2890:
        case 2891:
        case 2892:
        case 2893:
        case 2894:
        case 2895:
        case 2896:
        case 2897:
        case 2898:
        case 2899:
        case 2900:
        case 2901:
        case 2902:
        case 2903:
        case 2904:
        case 2905:
        case 2906:
        case 2907:
        case 2908:
        case 2909:
        case 2910:
        case 2911:
        case 2912:
        case 2913:
        case 2914:
        case 2915:
        case 2916:
        case 2917:
        case 2918:
        case 2919:
        case 2920:
        case 2921:
        case 2922:
        case 2923:
        case 2924:
        case 2925:
        case 2926:
        case 2927:
        case 2928:
        case 2929:
        case 2930:
        case 2931:
        case 2932:
        case 2933:
        case 2934:
        case 2935:
        case 2936:
        case 2937:
        case 2938:
        case 2939:
        case 2940:
        case 2941:
        case 2942:
        case 2943:
        case 2944:
        case 2945:
        case 2946:
        case 2947:
        case 2948:
        case 2949:
        case 2950:
        case 2951:
        case 2952:
        case 2953:
        case 2954:
        case 2955:
        case 2956:
        case 2957:
        case 2958:
        case 2959:
        case 2960:
        case 2961:
        case 2962:
        case 2963:
        case 2964:
        case 2965:
        case 2966:
        case 2967:
        case 2968:
        case 2969:
        case 2970:
        case 2971:
        case 2972:
        case 2973:
        case 2974:
        case 2975:
        case 2976:
        case 2977:
        case 2978:
        case 2979:
        case 2980:
        case 2981:
        case 2982:
        case 2983:
        case 2984:
        case 2985:
        case 2986:
        case 2987:
        case 2988:
        case 2989:
        case 2990:
        case 2991:
        case 2992:
        case 2993:
        case 2994:
        case 2995:
        case 2996:
        case 2997:
        case 2998:
        case 2999:
        case 3000:
        case 3001:
        case 3002:
        case 3003:
        case 3004:
        case 3005:
        case 3006:
        case 3007:
        case 3008:
        case 3009:
        case 3010:
        case 3011:
        case 3012:
        case 3013:
        case 3014:
        case 3015:
        case 3016:
        case 3017:
        case 3018:
        case 3019:
        case 3020:
        case 3021:
        case 3022:
        case 3023:
        case 3024:
        case 3025:
        case 3026:
        case 3027:
        case 3028:
        case 3029:
        case 3030:
        case 3031:
        case 3032:
        case 3033:
        case 3034:
        case 3035:
        case 3036:
        case 3037:
        case 3038:
        case 3039:
        case 3040:
        case 3041:
        case 3042:
        case 3043:
        case 3044:
        case 3045:
        case 3046:
        case 3047:
        case 3048:
        case 3049:
        case 3050:
        case 3051:
        case 3052:
        case 3053:
        case 3054:
        case 3055:
        case 3056:
        case 3057:
        case 3058:
        case 3059:
        case 3060:
        case 3061:
        case 3062:
        case 3063:
        case 3064:
        case 3065:
        case 3066:
        case 3067:
        case 3068:
        case 3069:
        case 3070:
        case 3071: branch<1>(gba, opcode); break;
        case 3072:
        case 3073:
        case 3074:
        case 3075:
        case 3076:
        case 3077:
        case 3078:
        case 3079:
        case 3080:
        case 3081:
        case 3082:
        case 3083:
        case 3084:
        case 3085:
        case 3086:
        case 3087:
        case 3088:
        case 3089:
        case 3090:
        case 3091:
        case 3092:
        case 3093:
        case 3094:
        case 3095:
        case 3096:
        case 3097:
        case 3098:
        case 3099:
        case 3100:
        case 3101:
        case 3102:
        case 3103:
        case 3104:
        case 3105:
        case 3106:
        case 3107:
        case 3108:
        case 3109:
        case 3110:
        case 3111:
        case 3112:
        case 3113:
        case 3114:
        case 3115:
        case 3116:
        case 3117:
        case 3118:
        case 3119:
        case 3120:
        case 3121:
        case 3122:
        case 3123:
        case 3124:
        case 3125:
        case 3126:
        case 3127:
        case 3128:
        case 3129:
        case 3130:
        case 3131:
        case 3132:
        case 3133:
        case 3134:
        case 3135:
        case 3136:
        case 3137:
        case 3138:
        case 3139:
        case 3140:
        case 3141:
        case 3142:
        case 3143:
        case 3144:
        case 3145:
        case 3146:
        case 3147:
        case 3148:
        case 3149:
        case 3150:
        case 3151:
        case 3152:
        case 3153:
        case 3154:
        case 3155:
        case 3156:
        case 3157:
        case 3158:
        case 3159:
        case 3160:
        case 3161:
        case 3162:
        case 3163:
        case 3164:
        case 3165:
        case 3166:
        case 3167:
        case 3168:
        case 3169:
        case 3170:
        case 3171:
        case 3172:
        case 3173:
        case 3174:
        case 3175:
        case 3176:
        case 3177:
        case 3178:
        case 3179:
        case 3180:
        case 3181:
        case 3182:
        case 3183:
        case 3184:
        case 3185:
        case 3186:
        case 3187:
        case 3188:
        case 3189:
        case 3190:
        case 3191:
        case 3192:
        case 3193:
        case 3194:
        case 3195:
        case 3196:
        case 3197:
        case 3198:
        case 3199:
        case 3200:
        case 3201:
        case 3202:
        case 3203:
        case 3204:
        case 3205:
        case 3206:
        case 3207:
        case 3208:
        case 3209:
        case 3210:
        case 3211:
        case 3212:
        case 3213:
        case 3214:
        case 3215:
        case 3216:
        case 3217:
        case 3218:
        case 3219:
        case 3220:
        case 3221:
        case 3222:
        case 3223:
        case 3224:
        case 3225:
        case 3226:
        case 3227:
        case 3228:
        case 3229:
        case 3230:
        case 3231:
        case 3232:
        case 3233:
        case 3234:
        case 3235:
        case 3236:
        case 3237:
        case 3238:
        case 3239:
        case 3240:
        case 3241:
        case 3242:
        case 3243:
        case 3244:
        case 3245:
        case 3246:
        case 3247:
        case 3248:
        case 3249:
        case 3250:
        case 3251:
        case 3252:
        case 3253:
        case 3254:
        case 3255:
        case 3256:
        case 3257:
        case 3258:
        case 3259:
        case 3260:
        case 3261:
        case 3262:
        case 3263:
        case 3264:
        case 3265:
        case 3266:
        case 3267:
        case 3268:
        case 3269:
        case 3270:
        case 3271:
        case 3272:
        case 3273:
        case 3274:
        case 3275:
        case 3276:
        case 3277:
        case 3278:
        case 3279:
        case 3280:
        case 3281:
        case 3282:
        case 3283:
        case 3284:
        case 3285:
        case 3286:
        case 3287:
        case 3288:
        case 3289:
        case 3290:
        case 3291:
        case 3292:
        case 3293:
        case 3294:
        case 3295:
        case 3296:
        case 3297:
        case 3298:
        case 3299:
        case 3300:
        case 3301:
        case 3302:
        case 3303:
        case 3304:
        case 3305:
        case 3306:
        case 3307:
        case 3308:
        case 3309:
        case 3310:
        case 3311:
        case 3312:
        case 3313:
        case 3314:
        case 3315:
        case 3316:
        case 3317:
        case 3318:
        case 3319:
        case 3320:
        case 3321:
        case 3322:
        case 3323:
        case 3324:
        case 3325:
        case 3326:
        case 3327:
        case 3328:
        case 3329:
        case 3330:
        case 3331:
        case 3332:
        case 3333:
        case 3334:
        case 3335:
        case 3336:
        case 3337:
        case 3338:
        case 3339:
        case 3340:
        case 3341:
        case 3342:
        case 3343:
        case 3344:
        case 3345:
        case 3346:
        case 3347:
        case 3348:
        case 3349:
        case 3350:
        case 3351:
        case 3352:
        case 3353:
        case 3354:
        case 3355:
        case 3356:
        case 3357:
        case 3358:
        case 3359:
        case 3360:
        case 3361:
        case 3362:
        case 3363:
        case 3364:
        case 3365:
        case 3366:
        case 3367:
        case 3368:
        case 3369:
        case 3370:
        case 3371:
        case 3372:
        case 3373:
        case 3374:
        case 3375:
        case 3376:
        case 3377:
        case 3378:
        case 3379:
        case 3380:
        case 3381:
        case 3382:
        case 3383:
        case 3384:
        case 3385:
        case 3386:
        case 3387:
        case 3388:
        case 3389:
        case 3390:
        case 3391:
        case 3392:
        case 3393:
        case 3394:
        case 3395:
        case 3396:
        case 3397:
        case 3398:
        case 3399:
        case 3400:
        case 3401:
        case 3402:
        case 3403:
        case 3404:
        case 3405:
        case 3406:
        case 3407:
        case 3408:
        case 3409:
        case 3410:
        case 3411:
        case 3412:
        case 3413:
        case 3414:
        case 3415:
        case 3416:
        case 3417:
        case 3418:
        case 3419:
        case 3420:
        case 3421:
        case 3422:
        case 3423:
        case 3424:
        case 3425:
        case 3426:
        case 3427:
        case 3428:
        case 3429:
        case 3430:
        case 3431:
        case 3432:
        case 3433:
        case 3434:
        case 3435:
        case 3436:
        case 3437:
        case 3438:
        case 3439:
        case 3440:
        case 3441:
        case 3442:
        case 3443:
        case 3444:
        case 3445:
        case 3446:
        case 3447:
        case 3448:
        case 3449:
        case 3450:
        case 3451:
        case 3452:
        case 3453:
        case 3454:
        case 3455:
        case 3456:
        case 3457:
        case 3458:
        case 3459:
        case 3460:
        case 3461:
        case 3462:
        case 3463:
        case 3464:
        case 3465:
        case 3466:
        case 3467:
        case 3468:
        case 3469:
        case 3470:
        case 3471:
        case 3472:
        case 3473:
        case 3474:
        case 3475:
        case 3476:
        case 3477:
        case 3478:
        case 3479:
        case 3480:
        case 3481:
        case 3482:
        case 3483:
        case 3484:
        case 3485:
        case 3486:
        case 3487:
        case 3488:
        case 3489:
        case 3490:
        case 3491:
        case 3492:
        case 3493:
        case 3494:
        case 3495:
        case 3496:
        case 3497:
        case 3498:
        case 3499:
        case 3500:
        case 3501:
        case 3502:
        case 3503:
        case 3504:
        case 3505:
        case 3506:
        case 3507:
        case 3508:
        case 3509:
        case 3510:
        case 3511:
        case 3512:
        case 3513:
        case 3514:
        case 3515:
        case 3516:
        case 3517:
        case 3518:
        case 3519:
        case 3520:
        case 3521:
        case 3522:
        case 3523:
        case 3524:
        case 3525:
        case 3526:
        case 3527:
        case 3528:
        case 3529:
        case 3530:
        case 3531:
        case 3532:
        case 3533:
        case 3534:
        case 3535:
        case 3536:
        case 3537:
        case 3538:
        case 3539:
        case 3540:
        case 3541:
        case 3542:
        case 3543:
        case 3544:
        case 3545:
        case 3546:
        case 3547:
        case 3548:
        case 3549:
        case 3550:
        case 3551:
        case 3552:
        case 3553:
        case 3554:
        case 3555:
        case 3556:
        case 3557:
        case 3558:
        case 3559:
        case 3560:
        case 3561:
        case 3562:
        case 3563:
        case 3564:
        case 3565:
        case 3566:
        case 3567:
        case 3568:
        case 3569:
        case 3570:
        case 3571:
        case 3572:
        case 3573:
        case 3574:
        case 3575:
        case 3576:
        case 3577:
        case 3578:
        case 3579:
        case 3580:
        case 3581:
        case 3582:
        case 3583:
        case 3584:
        case 3585:
        case 3586:
        case 3587:
        case 3588:
        case 3589:
        case 3590:
        case 3591:
        case 3592:
        case 3593:
        case 3594:
        case 3595:
        case 3596:
        case 3597:
        case 3598:
        case 3599:
        case 3600:
        case 3601:
        case 3602:
        case 3603:
        case 3604:
        case 3605:
        case 3606:
        case 3607:
        case 3608:
        case 3609:
        case 3610:
        case 3611:
        case 3612:
        case 3613:
        case 3614:
        case 3615:
        case 3616:
        case 3617:
        case 3618:
        case 3619:
        case 3620:
        case 3621:
        case 3622:
        case 3623:
        case 3624:
        case 3625:
        case 3626:
        case 3627:
        case 3628:
        case 3629:
        case 3630:
        case 3631:
        case 3632:
        case 3633:
        case 3634:
        case 3635:
        case 3636:
        case 3637:
        case 3638:
        case 3639:
        case 3640:
        case 3641:
        case 3642:
        case 3643:
        case 3644:
        case 3645:
        case 3646:
        case 3647:
        case 3648:
        case 3649:
        case 3650:
        case 3651:
        case 3652:
        case 3653:
        case 3654:
        case 3655:
        case 3656:
        case 3657:
        case 3658:
        case 3659:
        case 3660:
        case 3661:
        case 3662:
        case 3663:
        case 3664:
        case 3665:
        case 3666:
        case 3667:
        case 3668:
        case 3669:
        case 3670:
        case 3671:
        case 3672:
        case 3673:
        case 3674:
        case 3675:
        case 3676:
        case 3677:
        case 3678:
        case 3679:
        case 3680:
        case 3681:
        case 3682:
        case 3683:
        case 3684:
        case 3685:
        case 3686:
        case 3687:
        case 3688:
        case 3689:
        case 3690:
        case 3691:
        case 3692:
        case 3693:
        case 3694:
        case 3695:
        case 3696:
        case 3697:
        case 3698:
        case 3699:
        case 3700:
        case 3701:
        case 3702:
        case 3703:
        case 3704:
        case 3705:
        case 3706:
        case 3707:
        case 3708:
        case 3709:
        case 3710:
        case 3711:
        case 3712:
        case 3713:
        case 3714:
        case 3715:
        case 3716:
        case 3717:
        case 3718:
        case 3719:
        case 3720:
        case 3721:
        case 3722:
        case 3723:
        case 3724:
        case 3725:
        case 3726:
        case 3727:
        case 3728:
        case 3729:
        case 3730:
        case 3731:
        case 3732:
        case 3733:
        case 3734:
        case 3735:
        case 3736:
        case 3737:
        case 3738:
        case 3739:
        case 3740:
        case 3741:
        case 3742:
        case 3743:
        case 3744:
        case 3745:
        case 3746:
        case 3747:
        case 3748:
        case 3749:
        case 3750:
        case 3751:
        case 3752:
        case 3753:
        case 3754:
        case 3755:
        case 3756:
        case 3757:
        case 3758:
        case 3759:
        case 3760:
        case 3761:
        case 3762:
        case 3763:
        case 3764:
        case 3765:
        case 3766:
        case 3767:
        case 3768:
        case 3769:
        case 3770:
        case 3771:
        case 3772:
        case 3773:
        case 3774:
        case 3775:
        case 3776:
        case 3777:
        case 3778:
        case 3779:
        case 3780:
        case 3781:
        case 3782:
        case 3783:
        case 3784:
        case 3785:
        case 3786:
        case 3787:
        case 3788:
        case 3789:
        case 3790:
        case 3791:
        case 3792:
        case 3793:
        case 3794:
        case 3795:
        case 3796:
        case 3797:
        case 3798:
        case 3799:
        case 3800:
        case 3801:
        case 3802:
        case 3803:
        case 3804:
        case 3805:
        case 3806:
        case 3807:
        case 3808:
        case 3809:
        case 3810:
        case 3811:
        case 3812:
        case 3813:
        case 3814:
        case 3815:
        case 3816:
        case 3817:
        case 3818:
        case 3819:
        case 3820:
        case 3821:
        case 3822:
        case 3823:
        case 3824:
        case 3825:
        case 3826:
        case 3827:
        case 3828:
        case 3829:
        case 3830:
        case 3831:
        case 3832:
        case 3833:
        case 3834:
        case 3835:
        case 3836:
        case 3837:
        case 3838:
        case 3839: undefined(gba, opcode); break;
        case 3840:
        case 3841:
        case 3842:
        case 3843:
        case 3844:
        case 3845:
        case 3846:
        case 3847:
        case 3848:
        case 3849:
        case 3850:
        case 3851:
        case 3852:
        case 3853:
        case 3854:
        case 3855:
        case 3856:
        case 3857:
        case 3858:
        case 3859:
        case 3860:
        case 3861:
        case 3862:
        case 3863:
        case 3864:
        case 3865:
        case 3866:
        case 3867:
        case 3868:
        case 3869:
        case 3870:
        case 3871:
        case 3872:
        case 3873:
        case 3874:
        case 3875:
        case 3876:
        case 3877:
        case 3878:
        case 3879:
        case 3880:
        case 3881:
        case 3882:
        case 3883:
        case 3884:
        case 3885:
        case 3886:
        case 3887:
        case 3888:
        case 3889:
        case 3890:
        case 3891:
        case 3892:
        case 3893:
        case 3894:
        case 3895:
        case 3896:
        case 3897:
        case 3898:
        case 3899:
        case 3900:
        case 3901:
        case 3902:
        case 3903:
        case 3904:
        case 3905:
        case 3906:
        case 3907:
        case 3908:
        case 3909:
        case 3910:
        case 3911:
        case 3912:
        case 3913:
        case 3914:
        case 3915:
        case 3916:
        case 3917:
        case 3918:
        case 3919:
        case 3920:
        case 3921:
        case 3922:
        case 3923:
        case 3924:
        case 3925:
        case 3926:
        case 3927:
        case 3928:
        case 3929:
        case 3930:
        case 3931:
        case 3932:
        case 3933:
        case 3934:
        case 3935:
        case 3936:
        case 3937:
        case 3938:
        case 3939:
        case 3940:
        case 3941:
        case 3942:
        case 3943:
        case 3944:
        case 3945:
        case 3946:
        case 3947:
        case 3948:
        case 3949:
        case 3950:
        case 3951:
        case 3952:
        case 3953:
        case 3954:
        case 3955:
        case 3956:
        case 3957:
        case 3958:
        case 3959:
        case 3960:
        case 3961:
        case 3962:
        case 3963:
        case 3964:
        case 3965:
        case 3966:
        case 3967:
        case 3968:
        case 3969:
        case 3970:
        case 3971:
        case 3972:
        case 3973:
        case 3974:
        case 3975:
        case 3976:
        case 3977:
        case 3978:
        case 3979:
        case 3980:
        case 3981:
        case 3982:
        case 3983:
        case 3984:
        case 3985:
        case 3986:
        case 3987:
        case 3988:
        case 3989:
        case 3990:
        case 3991:
        case 3992:
        case 3993:
        case 3994:
        case 3995:
        case 3996:
        case 3997:
        case 3998:
        case 3999:
        case 4000:
        case 4001:
        case 4002:
        case 4003:
        case 4004:
        case 4005:
        case 4006:
        case 4007:
        case 4008:
        case 4009:
        case 4010:
        case 4011:
        case 4012:
        case 4013:
        case 4014:
        case 4015:
        case 4016:
        case 4017:
        case 4018:
        case 4019:
        case 4020:
        case 4021:
        case 4022:
        case 4023:
        case 4024:
        case 4025:
        case 4026:
        case 4027:
        case 4028:
        case 4029:
        case 4030:
        case 4031:
        case 4032:
        case 4033:
        case 4034:
        case 4035:
        case 4036:
        case 4037:
        case 4038:
        case 4039:
        case 4040:
        case 4041:
        case 4042:
        case 4043:
        case 4044:
        case 4045:
        case 4046:
        case 4047:
        case 4048:
        case 4049:
        case 4050:
        case 4051:
        case 4052:
        case 4053:
        case 4054:
        case 4055:
        case 4056:
        case 4057:
        case 4058:
        case 4059:
        case 4060:
        case 4061:
        case 4062:
        case 4063:
        case 4064:
        case 4065:
        case 4066:
        case 4067:
        case 4068:
        case 4069:
        case 4070:
        case 4071:
        case 4072:
        case 4073:
        case 4074:
        case 4075:
        case 4076:
        case 4077:
        case 4078:
        case 4079:
        case 4080:
        case 4081:
        case 4082:
        case 4083:
        case 4084:
        case 4085:
        case 4086:
        case 4087:
        case 4088:
        case 4089:
        case 4090:
        case 4091:
        case 4092:
        case 4093:
        case 4094:
        case 4095: software_interrupt(gba, opcode); break;
    }
}

inline auto fetch(Gba& gba) -> u32
{
    const auto opcode = CPU.pipeline[0];
    CPU.pipeline[0] = CPU.pipeline[1];
    gba.cpu.registers[PC_INDEX] += 4;
    CPU.pipeline[1] = mem::read32(gba, get_pc(gba));

    return opcode;
}
} // namespace

auto execute(Gba& gba) -> void
{
    const auto opcode = fetch(gba);
    const auto cond = bit::get_range<28, 31>(opcode);

    // it's highly likely that cond == 0xE, so we optimise for that
    // before hitting the switch (slower).
    if (cond == COND_AL || check_cond(gba, cond)) [[likely]]
    {
        execute_switch(gba, opcode);
    }
}

} // namespace gba::arm7tdmi::arm
