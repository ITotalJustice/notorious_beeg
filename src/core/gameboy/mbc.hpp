#pragma once

#include "types.hpp"
#include "fwd.hpp"

namespace gba::gb {

STATIC void mbc_write(Gba& gba, u16 addr, u8 value);
STATIC auto mbc_get_rom_bank(Gba& gba, u8 bank) -> struct MBC_RomBankInfo;
STATIC auto mbc_get_ram_bank(Gba& gba) -> struct MBC_RamBankInfo;

} // namespace gba::gb
