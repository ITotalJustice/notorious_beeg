#include "gb.hpp"
#include "internal.hpp"
#include "mbc.hpp"

#include <cstring>
#include <cassert>
#include <utility>

#include "gba.hpp"

namespace gba::gb {
namespace {

struct MbcInfo
{
    u8 type;
    u8 flags;
};

constexpr auto mbc_get_info(u8 index) -> struct MbcInfo
{
    struct MbcInfo info{};

    switch (index)
    {
        case 0x00:
            info.type = MbcType_0;
            info.flags = MBC_FLAGS_NONE;
            break;

        // MBC1
        case 0x01:
            info.type = MbcType_1;
            info.flags = MBC_FLAGS_NONE;
            break;

        case 0x02:
            info.type = MbcType_1;
            info.flags = MBC_FLAGS_RAM;
            break;

        case 0x03:
            info.type = MbcType_1;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_BATTERY;
            break;

        // MBC2
        case 0x05:
            info.type = MbcType_2;
            info.flags = MBC_FLAGS_RAM;
            break;

        case 0x06:
            info.type = MbcType_2;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_BATTERY;
            break;

        // MBC3
        case 0x0F:
            info.type = MbcType_3;
            info.flags = MBC_FLAGS_BATTERY | MBC_FLAGS_RTC;
            break;

        case 0x10:
            info.type = MbcType_3;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_BATTERY | MBC_FLAGS_RTC;
            break;

        case 0x11:
            info.type = MbcType_3;
            info.flags = MBC_FLAGS_NONE;
            break;

        case 0x13:
            info.type = MbcType_3;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_BATTERY;
            break;

        // MBC5
        case 0x19:
            info.type = MbcType_5;
            info.flags = MBC_FLAGS_NONE;
            break;

        case 0x1A:
            info.type = MbcType_5;
            info.flags = MBC_FLAGS_RAM;
            break;

        case 0x1B:
            info.type = MbcType_5;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_BATTERY;
            break;

        case 0x1C:
            info.type = MbcType_5;
            info.flags = MBC_FLAGS_RUMBLE;
            break;

        case 0x1D:
            info.type = MbcType_5;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_RUMBLE;
            break;

        case 0x1E:
            info.type = MbcType_5;
            info.flags = MBC_FLAGS_RAM | MBC_FLAGS_BATTERY;
            break;
    }

    return info;
}

auto mbc_setup_empty_ram() -> struct MBC_RamBankInfo
{
    static const u8 MBC_NO_RAM_READ = 0xFF;
    static u8 MBC_NO_RAM_WRITE = 0x00;

    struct MBC_RamBankInfo info{};

    info.r[0].ptr = &MBC_NO_RAM_READ;
    info.r[0].mask = 0;
    info.r[1].ptr = &MBC_NO_RAM_READ;
    info.r[1].mask = 0;

    info.w[0].ptr = &MBC_NO_RAM_WRITE;
    info.w[0].mask = 0;
    info.w[1].ptr = &MBC_NO_RAM_WRITE;
    info.w[1].mask = 0;

    return info;
}

void mbc0_write([[maybe_unused]] Gba& gba, [[maybe_unused]] u16 addr, [[maybe_unused]] u8 value)
{
}

void mbc1_write(Gba& gba, u16 addr, u8 value)
{
    switch ((addr >> 12) & 0xF)
    {
    // RAM ENABLE
        case 0x0: case 0x1:
            // check the lower 4-bits for any value with 0xA
            gba.gameboy.cart.ram_enabled = (value & 0xF) == 0xA;
            update_ram_banks(gba);
            break;

    // ROM BANK
        case 0x2: case 0x3: {
            // check only 5-bits, cannot be 0
            gba.gameboy.cart.rom_bank_lo = (value & 0x1F) | ((value & 0x1F) == 0);
            gba.gameboy.cart.rom_bank = ((gba.gameboy.cart.rom_bank_hi << 5) | (gba.gameboy.cart.rom_bank_lo)) % gba.gameboy.cart.rom_bank_max;
            update_rom_banks(gba);
        } break;

    // ROM / RAM BANK
        case 0x4: case 0x5:
            if (gba.gameboy.cart.rom_bank_max > 18)
            {
                gba.gameboy.cart.rom_bank_hi = value & 0x3;
                gba.gameboy.cart.rom_bank = ((gba.gameboy.cart.rom_bank_hi << 5) | gba.gameboy.cart.rom_bank_lo) % gba.gameboy.cart.rom_bank_max;
                update_rom_banks(gba);
            }

            if (gba.gameboy.cart.bank_mode)
            {
                if ((gba.gameboy.cart.flags & MBC_FLAGS_RAM))
                {
                    gba.gameboy.cart.ram_bank = (value & 0x3) % gba.gameboy.cart.ram_bank_max;
                    update_ram_banks(gba);
                }
            }
            break;

    // ROM / RAM MODE
        case 0x6: case 0x7:
            gba.gameboy.cart.bank_mode = value & 0x1;

            if (gba.gameboy.cart.rom_bank_max > 18)
            {
                if (gba.gameboy.cart.bank_mode == 0)
                {
                    gba.gameboy.cart.rom_bank = gba.gameboy.cart.rom_bank_lo;
                }
            }
            update_rom_banks(gba);
            update_ram_banks(gba);
            break;

    // RAM BANK X
        case 0xA: case 0xB:
            if (!(gba.gameboy.cart.flags & MBC_FLAGS_RAM) || !gba.gameboy.cart.ram_enabled)
            {
                return;
            }
            gba.gameboy.ram[(addr & 0x1FFF) + (0x2000 * (gba.gameboy.cart.bank_mode == 1 ? gba.gameboy.cart.ram_bank : 0))] = value;
            gba.gameboy.ram_dirty = true;
            break;
    }
}

void mbc2_write(Gba& gba, u16 addr, u8 value)
{
    switch ((addr >> 12) & 0xF)
    {
    // RAM ENABLE / ROM BANK
        case 0x0: case 0x1: case 0x2: case 0x3:
            // if the 8th bit is not set, the value
            // controls the ram enable
            if ((addr & 0x100) == 0)
            {
                gba.gameboy.cart.ram_enabled = (value & 0x0F) == 0x0A;
                update_ram_banks(gba);
            }
            else
            {
                const auto bank = (value & 0x0F) | ((value & 0x0F) == 0);
                gba.gameboy.cart.rom_bank = bank % gba.gameboy.cart.rom_bank_max;
                update_rom_banks(gba);
            }
            break;

    // RAM WRITE
        case 0xA: case 0xB: {
            if (!(gba.gameboy.cart.flags & MBC_FLAGS_RAM) || !gba.gameboy.cart.ram_enabled)
            {
                return;
            }

            const auto masked_value = (value & 0x0F) | 0xF0;
            const auto masked_addr = addr & 0x1FF;

            gba.gameboy.ram[masked_addr] = masked_value;
            gba.gameboy.ram_dirty = true;
        } break;
    }
}

void mbc3_rtc_write(Gba& gba, u8 value)
{
    // TODO: cap the values so they aren't invalid!
    switch (gba.gameboy.cart.rtc_mapped_reg)
    {
        case RTC_MAPPED_REG_S: gba.gameboy.cart.rtc.S = value; break;
        case RTC_MAPPED_REG_M: gba.gameboy.cart.rtc.M = value; break;
        case RTC_MAPPED_REG_H: gba.gameboy.cart.rtc.H = value; break;
        case RTC_MAPPED_REG_DL: gba.gameboy.cart.rtc.DL = value; break;
        case RTC_MAPPED_REG_DH: gba.gameboy.cart.rtc.DH = value; break;
    }
}

void speed_hack_map_rtc_reg(Gba& gba)
{
    u8* ptr = nullptr;

    switch (gba.gameboy.cart.rtc_mapped_reg)
    {
        case RTC_MAPPED_REG_S: ptr = &gba.gameboy.cart.rtc.S; break;
        case RTC_MAPPED_REG_M: ptr = &gba.gameboy.cart.rtc.M; break;
        case RTC_MAPPED_REG_H: ptr = &gba.gameboy.cart.rtc.H; break;
        case RTC_MAPPED_REG_DL: ptr = &gba.gameboy.cart.rtc.DL; break;
        case RTC_MAPPED_REG_DH: ptr = &gba.gameboy.cart.rtc.DH; break;
    }

    gba.gameboy.rmap[0xA].ptr = ptr;
    gba.gameboy.rmap[0xA].mask = 0;
    gba.gameboy.rmap[0xB].ptr = ptr;
    gba.gameboy.rmap[0xB].mask = 0;

    gba.gameboy.wmap[0xA].ptr = ptr;
    gba.gameboy.wmap[0xA].mask = 0;
    gba.gameboy.wmap[0xB].ptr = ptr;
    gba.gameboy.wmap[0xB].mask = 0;
}

void mbc3_write(Gba& gba, u16 addr, u8 value)
{
    switch ((addr >> 12) & 0xF)
    {
    // RAM / RTC REGISTER ENABLE
        case 0x0: case 0x1:
            gba.gameboy.cart.ram_enabled = (value & 0x0F) == 0x0A;
            update_ram_banks(gba);
            break;

    // ROM BANK
        case 0x2: case 0x3:
            gba.gameboy.cart.rom_bank = ((value) | (value == 0)) % gba.gameboy.cart.rom_bank_max;
            update_rom_banks(gba);
            break;

    // RAM BANK / RTC REGISTER
        case 0x4: case 0x5:
            // set bank 0-3
            if (value <= 0x03)
            {
                // NOTE: can this be out of range?
                gba.gameboy.cart.ram_bank = value & 0x3;
                gba.gameboy.cart.in_ram = true;
                update_ram_banks(gba);
            }
            // if we have rtc and in range, set the mapped rtc reg
            else if (has_mbc_flags(gba, MBC_FLAGS_RTC) && value >= 0x08 && value <= 0x0C)
            {
                gba.gameboy.cart.rtc_mapped_reg = value - 0x08;
                gba.gameboy.cart.in_ram = false;
                speed_hack_map_rtc_reg(gba);
            }
            break;

    // LATCH CLOCK DATA
        case 0x6: case 0x7:
            break;

        case 0xA: case 0xB:
            if (has_mbc_flags(gba, MBC_FLAGS_RAM) && gba.gameboy.cart.ram_enabled)
            {
                if (gba.gameboy.cart.in_ram)
                {
                    gba.gameboy.ram[(addr & 0x1FFF) + (0x2000 * gba.gameboy.cart.ram_bank)] = value;
                    gba.gameboy.ram_dirty = true;
                }
                else if (has_mbc_flags(gba, MBC_FLAGS_RTC))
                {
                    mbc3_rtc_write(gba, value);
                }
            }
            break;
    }
}

void mbc5_write(Gba& gba, u16 addr, u8 value)
{
    switch ((addr >> 12) & 0xF)
    {
    // RAM
        case 0x0: case 0x1:
            // gbctr states that only 0xA enables ram
            // every other value disables it...
            gba.gameboy.cart.ram_enabled = value == 0x0A;
            update_ram_banks(gba);
            break;

    // ROM BANK LOW
        case 0x2:
        {
            // sets bits 0-7
            const u16 bank = (gba.gameboy.cart.rom_bank & 0xFF00) | value;
            gba.gameboy.cart.rom_bank = bank % gba.gameboy.cart.rom_bank_max;
            update_rom_banks(gba);
        } break;

    // ROM BANK HIGH
        case 0x3:
        {
            // sets the 8th bit
            const auto bank = (gba.gameboy.cart.rom_bank & 0x00FF) | ((value & 0x1) << 8);
            gba.gameboy.cart.rom_bank = bank % gba.gameboy.cart.rom_bank_max;
            update_rom_banks(gba);
        } break;

    // RAM BANK
        case 0x4: case 0x5:
            if (gba.gameboy.cart.flags & MBC_FLAGS_RAM)
            {
                const auto bank = value & 0x0F;
                gba.gameboy.cart.ram_bank = bank % gba.gameboy.cart.ram_bank_max;
                update_ram_banks(gba);
            }
            break;

        case 0xA: case 0xB:
            if ((gba.gameboy.cart.flags & MBC_FLAGS_RAM) && gba.gameboy.cart.ram_enabled)
            {
                gba.gameboy.ram[(addr & 0x1FFF) + (0x2000 * gba.gameboy.cart.ram_bank)] = value;
                gba.gameboy.ram_dirty = true;
            }
            break;
    }
}

auto is_ascii_char_valid(const char c) -> bool
{
    // always upper case! ignore lower case ascii
    return c >= ' ' && c <= '_';
}

} // namespace

void mbc_write(Gba& gba, u16 addr, u8 value)
{
    switch (gba.gameboy.cart.type)
    {
        case MbcType_0: mbc0_write(gba, addr, value); break;
        case MbcType_1: mbc1_write(gba, addr, value); break;
        case MbcType_2: mbc2_write(gba, addr, value); break;
        case MbcType_3: mbc3_write(gba, addr, value); break;
        case MbcType_5: mbc5_write(gba, addr, value); break;
    }
}

auto mbc_get_rom_bank(Gba& gba, u8 bank) -> struct MBC_RomBankInfo
{
    struct MBC_RomBankInfo info{};
    const u8* ptr = nullptr;

    switch (gba.gameboy.cart.type)
    {
        case MbcType_1:
            if (bank == 0)
            {
                if (gba.gameboy.cart.rom_bank_max > 18 && gba.gameboy.cart.bank_mode == 1)
                {
                    auto b = (gba.gameboy.cart.rom_bank_hi << 5);
                    if (b > gba.gameboy.cart.rom_bank_max)
                    {
                        b %= gba.gameboy.cart.rom_bank_max;
                    }
                    ptr = gba.rom + (b * 0x4000);
                }
                else
                {
                    ptr = gba.rom;
                }
            }
            else
            {
                ptr = gba.rom + (gba.gameboy.cart.rom_bank * 0x4000);
            }

            for (std::size_t i = 0; i < ARRAY_SIZE(info.entries); ++i)
            {
                info.entries[i].ptr = ptr + (0x1000 * i);
                info.entries[i].mask = 0x0FFF;
            }
            break;

        case MbcType_0:
            if (bank == 0)
            {
                ptr = gba.rom;
            }
            else
            {
                ptr = gba.rom + 0x4000;
            }
            break;

        case MbcType_2:
        case MbcType_3:
        case MbcType_5:
            if (bank == 0)
            {
                ptr = gba.rom;
            }
            else
            {
                ptr = gba.rom + (gba.gameboy.cart.rom_bank * 0x4000);
            }
            break;
    }


    for (std::size_t i = 0; i < ARRAY_SIZE(info.entries); ++i)
    {
        info.entries[i].ptr = ptr + (0x1000 * i);
        info.entries[i].mask = 0x0FFF;
    }

    return info;
}

auto mbc_get_ram_bank(Gba& gba) -> struct MBC_RamBankInfo
{
    if (!(gba.gameboy.cart.flags & MBC_FLAGS_RAM) || !gba.gameboy.cart.ram_enabled || !gba.gameboy.cart.in_ram)
    {
        return mbc_setup_empty_ram();
    }

    struct MBC_RamBankInfo info{};
    auto ptr = gba.gameboy.ram + (0x2000 * gba.gameboy.cart.ram_bank);

    switch (gba.gameboy.cart.type)
    {
        case MbcType_0:
            return mbc_setup_empty_ram();

        // special handling is required for mbc2 as the values
        // in ram are only 4bit.
        // because of this, either writes or reads need to be
        // handled in a special case.
        // for now, i have opted for writes as it simplifies
        // the main read() function in bus.cpp
        // however, this can be problamatic if a bad save is loaded
        // that was created by another emulator
        case MbcType_2:
            for (std::size_t i = 0; i < ARRAY_SIZE(info.r); ++i)
            {
                info.r[i].ptr = ptr;
                info.r[i].mask = 0x01FF;

                info.w[i].ptr = nullptr;
                info.w[i].mask = 0;
            }
            break;

        case MbcType_1:
            // in mode 0, always read from bank 0
            ptr = gba.gameboy.ram + (0x2000 * (gba.gameboy.cart.bank_mode == 1 ? gba.gameboy.cart.ram_bank : 0));
            [[fallthrough]];

        case MbcType_3:
        case MbcType_5:
            for (std::size_t i = 0; i < ARRAY_SIZE(info.r); ++i)
            {
                info.r[i].ptr = ptr + (0x1000 * i);
                info.r[i].mask = 0x0FFF;

                info.w[i].ptr = ptr + (0x1000 * i);
                info.w[i].mask = 0x0FFF;
            }
            break;
    }

    return info;
}

auto get_rom_name_from_header(const struct CartHeader* header, struct CartName* name) -> int
{
    // in later games, including all gbc games, the title area was
    // actually reduced in size from 16 bytes down to 15, then 11.
    // as all titles are UPPER_CASE ASCII, it is easy to range check each
    // char and see if its valid, once the end is found, mark the next char NULL.
    // NOTE: it seems that spaces are also valid!

    // set entrie name to NULL
    std::memset(name, 0, sizeof(struct RomName));

    // manually copy the name using range check as explained above...
    for (std::size_t i = 0; i < ARRAY_SIZE(header->title); i++)
    {
        const auto c = header->title[i];

        if (!is_ascii_char_valid(c))
        {
            break;
        }

        name->name[i] = c;
    }

    return 0;
}

auto get_rom_name(const Gba& gba, struct CartName* name) -> int
{
    const auto header = get_rom_header_ptr(gba);

    return get_rom_name_from_header(header, name);
}

auto get_cart_ram_size(const struct CartHeader* header, u32* size) -> bool
{
    // check if mbc2, if so, manually set the ram size!
    if (header->cart_type == 0x5 || header->cart_type == 0x6)
    {
        *size = 0x200;
        return true;
    }

    // i think that more ram sizes are valid, however
    // i have yet to see a ram size bigger than this...
    switch (header->ram_size)
    {
        case 0: *size = SAVE_SIZE_NONE; break;
        case 1: *size = SAVE_SIZE_1; break;
        case 2: *size = SAVE_SIZE_2; break;
        case 3: *size = SAVE_SIZE_3; break;
        case 4: *size = SAVE_SIZE_4; break;
        case 5: *size = SAVE_SIZE_5; break;
        default:
            log_fatal("invalid ram size header->ram_size! %u\n", header->ram_size);
            return false;
    }

    if (header->ram_size == 5 || header->ram_size == 4)
    {
        gb_log("ram is of header->ram_size SAVE_SIZE_5, finally found a game that uses this!\n");
        assert(header->ram_size != 4 && header->ram_size != 5);
        return false;
    }

    return true;
}

auto get_mbc_flags(u8 cart_type, u8* flags_out) -> bool
{
    const auto info =  mbc_get_info(cart_type);

    if (!info.type)
    {
        return false;
    }

    *flags_out = info.flags;

    return true;
}

auto setup_mbc(Gba& gba, const struct CartHeader* header) -> bool
{
    // this won't fail because the type is 8-bit and theres 0x100 entries.
    // though the data inside can be NULL, but this is checked next...
    const auto info = mbc_get_info(header->cart_type);

    // types start at > 0, if 0, then this mbc is invalid
    if (!info.type)
    {
        gb_log("MBC NOT IMPLEMENTED: 0x%02X\n", header->cart_type);
        assert(!"MBC NOT IMPLEMENTED");
        return false;
    }

    gba.gameboy.cart.type = info.type;
    gba.gameboy.cart.flags = info.flags;
    // i think this is default?
    // surely ram is mapped first before rtc? or is nothing mapped?
    gba.gameboy.cart.in_ram = true;

    // todo: create mbcx_init() functions
    if (gba.gameboy.cart.type == MbcType_0)
    {
        gba.gameboy.cart.rom_bank = 0;
        gba.gameboy.cart.rom_bank_lo = 0;
    }
    else
    {
        gba.gameboy.cart.rom_bank = 1;
        gba.gameboy.cart.rom_bank_lo = 1;
    }

    // setup max rom banks
    // this can never be 0 as rom size is already set before.
    assert(gba.gameboy.cart.rom_size > 0 && "you changed where rom size is set!");
    gba.gameboy.cart.rom_bank_max = gba.gameboy.cart.rom_size / 0x4000;

    // todo: setup more flags.
    if (gba.gameboy.cart.flags & MBC_FLAGS_RAM)
    {
        // otherwise get the ram size via a LUT
        if (!get_cart_ram_size(header, &gba.gameboy.cart.ram_size))
        {
            gb_log("rom has ram but the size entry in header is invalid! %u\n", header->ram_size);
            return false;
        }
        else
        {
            gba.gameboy.cart.ram_bank_max = gba.gameboy.cart.ram_size / 0x2000;
        }

        // check that the size (if any) returned is within range of the
        // maximum ram size.
        if (gba.gameboy.cart.ram_size > gba.gameboy.ram_size)
        {
            gb_log("cart-ram size is too big for the maximum size set! got: %u max: %zu", gba.gameboy.cart.ram_size, gba.gameboy.ram_size);
            return false;
        }
    }

    return true;
}

} // namespace gba::gb
