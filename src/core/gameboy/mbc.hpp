#pragma once

#include "types.hpp"
#include "fwd.hpp"

namespace gba::gb {

void mbc_write(Gba& gba, u16 addr, u8 value);
auto mbc_get_rom_bank(Gba& gba, u8 bank) -> struct MBC_RomBankInfo;
auto mbc_get_ram_bank(Gba& gba) -> struct MBC_RamBankInfo;

} // namespace gba::gb
