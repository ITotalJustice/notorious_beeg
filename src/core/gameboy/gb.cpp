// TODO: cleanup this file, its a mess.
// overtime i've just dumped functions here that i wanted to add,
// with the idea that i'd eventually cleanup and organise this file.

#include "gb.hpp"
#include "ppu/ppu.hpp"
#include "apu/apu.hpp"
#include "internal.hpp"
#include "palette_table.hpp"
#include "scheduler.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <ranges>
#include <utility>

#include "gba.hpp"

namespace gba::gb {
namespace {

constexpr auto ROM_SIZE_MULT = 0x8000;

auto cart_type_str(const u8 type) -> const char*
{
    switch (type)
    {
        case 0x00:  return "ROM ONLY";                 case 0x19:  return "MBC5";
        case 0x01:  return "MBC1";                     case 0x1A:  return "MBC5+RAM";
        case 0x02:  return "MBC1+RAM";                 case 0x1B:  return "MBC5+RAM+BATTERY";
        case 0x03:  return "MBC1+RAM+BATTERY";         case 0x1C:  return "MBC5+RUMBLE";
        case 0x05:  return "MBC2";                     case 0x1D:  return "MBC5+RUMBLE+RAM";
        case 0x06:  return "MBC2+BATTERY";             case 0x1E:  return "MBC5+RUMBLE+RAM+BATTERY";
        case 0x08:  return "ROM+RAM";                  case 0x20:  return "MBC6";
        case 0x09:  return "ROM+RAM+BATTERY";          case 0x22:  return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
        case 0x0B:  return "MMM01";
        case 0x0C:  return "MMM01+RAM";
        case 0x0D:  return "MMM01+RAM+BATTERY";
        case 0x0F:  return "MBC3+TIMER+BATTERY";
        case 0x10:  return "MBC3+TIMER+RAM+BATTERY";   case 0xFC:  return "POCKET CAMERA";
        case 0x11:  return "MBC3";                     case 0xFD:  return "BANDAI TAMA5";
        case 0x12:  return "MBC3+RAM";                 case 0xFE:  return "HuC3";
        case 0x13:  return "MBC3+RAM+BATTERY";         case 0xFF:  return "HuC1+RAM+BATTERY";
        default: return "NULL";
    }
}

void cart_header_print(const struct CartHeader* header)
{
    (void)cart_type_str;

    printf("\nROM HEADER INFO\n");

    struct CartName cart_name;
    get_rom_name_from_header(header, &cart_name);

    printf("\tTITLE: %s\n", cart_name.name);
    // printf("\tNEW LICENSEE CODE: 0x%02X\n", header->new_licensee_code);
    printf("\tSGB FLAG: 0x%02X\n", header->sgb_flag);
    printf("\tCART TYPE: %s\n", cart_type_str(header->cart_type));
    printf("\tCART TYPE VALUE: 0x%02X\n", header->cart_type);
    printf("\tROM SIZE: 0x%02X\n", header->rom_size);
    printf("\tRAM SIZE: 0x%02X\n", header->ram_size);
    printf("\tHEADER CHECKSUM: 0x%02X\n", header->header_checksum);
    // printf("\tGLOBAL CHECKSUM: 0x%04X\n", header->global_checksum);

    u8 hash = 0;
    u8 forth = 0;
    get_rom_palette_hash_from_header(header, &hash, &forth);
    printf("\tHASH: 0x%02X, 0x%02X\n", hash, forth);
    printf("\n");
}

auto get_rom_header_ptr_from_data(const u8* data) -> const struct CartHeader*
{
    return (const struct CartHeader*)&data[BOOTROM_SIZE];
}

void set_system_type(Gba& gba, const enum SystemType type)
{
    gb_log("[INFO] setting system type to %s\n", get_system_type_string(type));
    gba.gameboy.system_type = type;
}

void on_set_builtin_palette(Gba& gba, struct PaletteEntry* p)
{
    if (!gba.colour_callback)
    {
        gba.gameboy.palette = *p;
        return;
    }

    for (auto i = 0; i < 4; i++)
    {
        gba.gameboy.palette.BG[i] = gba.colour_callback(gba.userdata, Colour(p->BG[i]));
    }

    for (auto i = 0; i < 4; i++)
    {
        gba.gameboy.palette.OBJ0[i] = gba.colour_callback(gba.userdata, Colour(p->OBJ0[i]));
    }

    for (auto i = 0; i < 4; i++)
    {
        gba.gameboy.palette.OBJ1[i] = gba.colour_callback(gba.userdata, Colour(p->OBJ1[i]));
    }
}

void setup_palette(Gba& gba, const struct CartHeader* header)
{
    // this should only ever be called in NONE GBC system.
    assert(is_system_gbc(gba) == false);

    struct PaletteEntry builtin_palette{};

    if (gba.gameboy.config.palette_config == PALETTE_CONFIG_USE_CUSTOM)
    {
        set_palette_from_palette(gba, &gba.gameboy.config.custom_palette);
    }
    else if (gba.gameboy.config.palette_config == PALETTE_CONFIG_NONE || (gba.gameboy.config.palette_config & PALETTE_CONFIG_USE_BUILTIN) == PALETTE_CONFIG_USE_BUILTIN)
    {
        // attempt to fill set the palatte from the builtins...
        u8 hash = 0;
        u8 forth = 0;
        // this will never fail...
        get_rom_palette_hash_from_header(header, &hash, &forth);

        if (!palette_fill_from_hash(hash, forth, true, &builtin_palette))
        {
            // try and fallback to custom palette if the user has set it
            if ((gba.gameboy.config.palette_config & PALETTE_CONFIG_USE_CUSTOM) == PALETTE_CONFIG_USE_CUSTOM)
            {
                set_palette_from_palette(gba, &gba.gameboy.config.custom_palette);
            }
            // otherwise use default palette...
            else
            {
                palette_fill_from_custom(CUSTOM_PALETTE_DEFAULT, &builtin_palette);
                on_set_builtin_palette(gba, &builtin_palette);
            }
        }
        else
        {
            on_set_builtin_palette(gba, &builtin_palette);
        }
    }
}

} // namespace

auto init(Gba& gba) -> bool
{
    std::memset(&gba.gameboy, 0, sizeof(struct Core));

    gba.gameboy.vram[0] = gba.mem.vram + 0x0000;
    gba.gameboy.vram[1] = gba.mem.vram + 0x2000;
    gba.gameboy.oam = gba.mem.oam;

    gba.gameboy.wram[0] = gba.mem.iwram + 0x0000;
    gba.gameboy.wram[1] = gba.mem.iwram + 0x1000;
    gba.gameboy.wram[2] = gba.mem.iwram + 0x2000;
    gba.gameboy.wram[3] = gba.mem.iwram + 0x3000;
    gba.gameboy.wram[4] = gba.mem.iwram + 0x4000;
    gba.gameboy.wram[5] = gba.mem.iwram + 0x5000;
    gba.gameboy.wram[6] = gba.mem.iwram + 0x6000;
    gba.gameboy.wram[7] = gba.mem.iwram + 0x7000;

    gba.gameboy.hram = gba.mem.pram + 0x00;
    gba.gameboy.io = gba.mem.pram + 0x80;

    gba.gameboy.ram = gba.mem.ewram;
    gba.gameboy.ram_size = sizeof(gba.mem.ewram);

    return true;
}

void reset(Gba& gba)
{
    gba.system = System::GB;

    std::ranges::fill(gba.mem.vram, 0xFF);
    std::ranges::fill(gba.mem.iwram, 0xFF);
    std::ranges::fill(gba.mem.oam, 0xFF);
    std::ranges::fill(gba.mem.pram, 0xFF);

    gba.gameboy.vram[0] = gba.mem.vram + 0x0000;
    gba.gameboy.vram[1] = gba.mem.vram + 0x2000;
    gba.gameboy.oam = gba.mem.oam;

    gba.gameboy.wram[0] = gba.mem.iwram + 0x0000;
    gba.gameboy.wram[1] = gba.mem.iwram + 0x1000;
    gba.gameboy.wram[2] = gba.mem.iwram + 0x2000;
    gba.gameboy.wram[3] = gba.mem.iwram + 0x3000;
    gba.gameboy.wram[4] = gba.mem.iwram + 0x4000;
    gba.gameboy.wram[5] = gba.mem.iwram + 0x5000;
    gba.gameboy.wram[6] = gba.mem.iwram + 0x6000;
    gba.gameboy.wram[7] = gba.mem.iwram + 0x7000;

    gba.gameboy.hram = gba.mem.pram + 0x00;
    gba.gameboy.io = gba.mem.pram + 0x80;

    gba.gameboy.ram = gba.mem.ewram;
    gba.gameboy.ram_size = sizeof(gba.mem.ewram);

    std::memset(&gba.gameboy.mem, 0, sizeof(gba.gameboy.mem));
    std::memset(&gba.gameboy.cpu, 0, sizeof(gba.gameboy.cpu));
    std::memset(&gba.gameboy.ppu, 0, sizeof(gba.gameboy.ppu));
    std::memset(&gba.gameboy.timer, 0, sizeof(gba.gameboy.timer));
    std::memset(&gba.gameboy.joypad, 0, sizeof(gba.gameboy.joypad));
    std::memset(IO, 0xFF, 0xA0);

    scheduler::reset(gba);
    apu::reset(gba, true);

    update_all_colours_gb(gba);

    gba.gameboy.joypad.var = 0xFF;
    gba.gameboy.ppu.next_cycles = 0;
    gba.gameboy.cycles = 0;

    gba.gameboy.mem.vbk = 0;
    gba.gameboy.mem.svbk = 1;

    // CPU
    cpu_set_register_pair(gba, CPU_REGISTER_PAIR_SP, 0xFFFE);
    cpu_set_register_pair(gba, CPU_REGISTER_PAIR_PC, 0x0100);
    cpu_set_register_pair(gba, CPU_REGISTER_PAIR_AF, 0x1180);
    cpu_set_register_pair(gba, CPU_REGISTER_PAIR_BC, 0x0000);
    cpu_set_register_pair(gba, CPU_REGISTER_PAIR_DE, 0xFF56);
    cpu_set_register_pair(gba, CPU_REGISTER_PAIR_HL, 0x000D);
    cpu_set_register(gba, CPU_REGISTER_B, 0x1);

    // IO
    IO_TIMA = 0x00;
    IO_TMA = 0x00;
    IO_TAC = 0x00;
    IO_LCDC = 0x91;
    IO_STAT = 0x00;
    IO_SCY = 0x00;
    IO_SCX = 0x00;
    IO_LY = 0x00;
    IO_LYC = 0x00;
    IO_BGP = 0xFC;
    IO_WY = 0x00;
    IO_WX = 0x00;
    GB_IO_IF = 0x00;
    GB_IO_IE = 0x00;
    IO_SC = 0x00;
    IO_SB = 0x00;
    IO_DIV_LOWER = 0x00;
    IO_DIV = 0x00;
    IO_SVBK = 0x01;
    IO_VBK = 0x00;
    IO_BCPS = 0x00;
    IO_OCPS = 0x00;
    IO_OPRI = 0xFE;
    IO_KEY1 = 0x7E;
    IO_72 = 0x00;
    IO_73 = 0x00;
    IO_74 = 0x00;
    IO_75 = 0x8F;
    IO_76 = 0x00;
    IO_77 = 0x00;

    REG_SOUNDCNT_X = 0xF1;
    apu::write_NR52(gba, 0xF1); // do this first as to enable the apu

    // triggering the channels causes a high pitch sound effect to be played
    // at start of most games so disabled for now.
    // TODO: run the bios and check the state of the core after 0x50 write
    // and set the internal values to match that!
    #if 1
    apu::write_NR52(gba, 0xF1); // do this first as to enable the apu
    apu::write_NR10(gba, 0x80);
    apu::write_NR11(gba, 0xBF);
    apu::write_NR12(gba, 0xF3);
    // apu::write_NR14(gba, 0xBF);
    apu::write_NR21(gba, 0x3F);
    apu::write_NR22(gba, 0x00);
    // apu::write_NR24(gba, 0xBF);
    apu::write_NR30(gba, 0x7F);
    apu::write_NR31(gba, 0xFF);
    apu::write_NR32(gba, 0x9F);
    apu::write_NR33(gba, 0xBF);
    apu::write_NR41(gba, 0xFF);
    apu::write_NR42(gba, 0x00);
    // apu::write_NR44(gba, 0xBF);
    apu::write_NR50(gba, 0x77);
    apu::write_NR51(gba, 0xF3);
    apu::write_NR52(gba, 0xF1);
    #endif

    #if USE_SCHED
    scheduler::add(gba, scheduler::Event::PPU, on_ppu_event, gba.gameboy.ppu.next_cycles);
    scheduler::add(gba, scheduler::Event::TIMER1, on_div_event, 256);
    #endif
}

auto get_rom_header_from_data(const u8* data, struct CartHeader* header) -> bool
{
    std::memcpy(header, data + BOOTROM_SIZE, sizeof(struct CartHeader));
    return true;
}

auto get_rom_header(const Gba& gba, struct CartHeader* header) -> bool
{
    if (!gba.is_gb() || (gba.gameboy.cart.ram_size < (sizeof(struct CartHeader) + BOOTROM_SIZE)))
    {
        return false;
    }

    return get_rom_header_from_data(gba.rom, header);
}

auto get_rom_header_ptr(const Gba& gba) -> const struct CartHeader*
{
    return get_rom_header_ptr_from_data(gba.rom);
}

auto get_rom_palette_hash_from_header(const struct CartHeader* header, u8* hash, u8* forth) -> bool
{
    if (!header || !hash || !forth)
    {
        return false;
    }

    u8 temp_hash = 0;
    for (u8 c : header->title)
    {
        temp_hash += c;
    }

    *hash = temp_hash;
    *forth = header->title[0x3];

    return true;
}

auto get_rom_palette_hash(const Gba& gba, u8* hash, u8* forth) -> bool
{
    if (!hash)
    {
        return false;
    }

    return get_rom_palette_hash_from_header(
        get_rom_header_ptr(gba),
        hash, forth
    );
}

auto set_palette_from_palette(Gba& gba, const struct PaletteEntry* palette) -> bool
{
    if (!palette)
    {
        return false;
    }

    gba.gameboy.palette = *palette;

    return true;
}

auto set_rtc(Gba& gba, const struct Rtc rtc) -> bool
{
    if (!has_mbc_flags(gba, MBC_FLAGS_RTC))
    {
        return false;
    }

    gba.gameboy.cart.rtc.S = rtc.S > 59 ? 59 : rtc.S;
    gba.gameboy.cart.rtc.M = rtc.M > 59 ? 59 : rtc.M;
    gba.gameboy.cart.rtc.H = rtc.H > 23 ? 23 : rtc.H;
    gba.gameboy.cart.rtc.DL = rtc.DL;
    gba.gameboy.cart.rtc.DH = rtc.DH & 0xC1; // only bit 0,6,7

    return true;
}

auto has_mbc_flags(const Gba& gba, const u8 flags) -> bool
{
    return (gba.gameboy.cart.flags & flags) == flags;
}

auto get_system_type(const Gba& gba) -> enum SystemType
{
    return gba.gameboy.system_type;
}

auto is_system_gbc(const Gba& gba) -> bool
{
    return get_system_type(gba) == SYSTEM_TYPE_GBC;
}

void set_sram(Gba& gba, u8* ram, std::size_t size)
{
    gba.gameboy.ram = ram;
    gba.gameboy.ram_size = size;

    // if we have a rom loaded, re-map the ram banks.

    // TODO: this should be a func instead of checking the ptr here!
    if (gba.is_gb()) // why is this if here?
    {
        update_ram_banks(gba);
    }
}

auto get_rom_info(const u8* data, std::size_t size, struct RomInfo* info_out) -> bool
{
    // todo: should ensure the romsize is okay!
    (void)size;

    const auto header = get_rom_header_ptr_from_data(data);

    info_out->rom_size = ROM_SIZE_MULT << header->rom_size;

    if (!get_cart_ram_size(header, &info_out->ram_size))
    {
        return false;
    }

    if (!get_mbc_flags(header->cart_type, &info_out->flags))
    {
        return false;
    }

    return true;
}

auto loadrom(Gba& gba, std::span<const u8> rom) -> bool
{
    if (rom.empty())
    {
        return false;
    }

    if (rom.size() < BOOTROM_SIZE + sizeof(struct CartHeader))
    {
        return false;
    }

    if (rom.size() > UINT32_MAX)
    {
        return false;
    }

    const auto header = get_rom_header_ptr_from_data(rom.data());
    if (!header)
    {
        return false;
    }

    cart_header_print(header);

    gba.gameboy.cart.rom_size = ROM_SIZE_MULT << header->rom_size;

    if (gba.gameboy.cart.rom_size > rom.size())
    {
        return false;
    }

    enum
    {
        GBC_ONLY = 0xC0,
        GBC_AND_DMG = 0x80,
        // not much is known about these types
        // these are not checked atm, but soon will be...
        P1 = 0x84,
        P2 = 0x88,

        SFLAG = 0x03,

        NEW_LICENSEE_USED = 0x33,
    };

    // todo: clean up the below spaghetti code
    const auto gbc_flag = header->title[sizeof(header->title) - 1];

    if ((gbc_flag & GBC_ONLY) == GBC_ONLY)
    {
        set_system_type(gba, SYSTEM_TYPE_GBC);
    }
    else if ((gbc_flag & GBC_AND_DMG) == GBC_AND_DMG)
    {
        set_system_type(gba, SYSTEM_TYPE_GBC);
    }
    else
    {
        set_system_type(gba, SYSTEM_TYPE_DMG);
    }

    // try and setup the mbc, this also implicitly sets up
    // gbc mode
    if (!setup_mbc(gba, header))
    {
        gb_log("failed to setup mbc!\n");
        return false;
    }

    // todo: should add more checks before we get to this point!
    std::ranges::copy(rom, gba.rom);

    reset(gba);
    setup_mmap(gba);

    // set the palette!
    if (!is_system_gbc(gba))
    {
        setup_palette(gba, header);
    }

    return true;
}

auto has_save(const Gba& gba) -> bool
{
    return (gba.gameboy.cart.flags & (MBC_FLAGS_RAM | MBC_FLAGS_BATTERY)) == (MBC_FLAGS_RAM | MBC_FLAGS_BATTERY);
}

auto has_rtc(const Gba& gba) -> bool
{
    return (gba.gameboy.cart.flags & MBC_FLAGS_RTC) == MBC_FLAGS_RTC;
}

auto calculate_savedata_size(const Gba& gba) -> std::size_t
{
    return gba.gameboy.cart.ram_size;
}


enum { STATE_MAGIC = 0x6BCE };
enum { STATE_VER = 1 };

auto savestate(const Gba& gba, struct State* state) -> bool
{
    if (!state || !gba.is_gb())
    {
        return false;
    }

    state->magic = STATE_MAGIC;
    state->version = STATE_VER;
    state->size = sizeof(struct State);

    std::memcpy(&state->mem, &gba.gameboy.mem, sizeof(state->mem));
    std::memcpy(&state->cpu, &gba.gameboy.cpu, sizeof(state->cpu));
    std::memcpy(&state->ppu, &gba.gameboy.ppu, sizeof(state->ppu));
    std::memcpy(&state->cart, &gba.gameboy.cart, sizeof(state->cart));
    std::memcpy(&state->timer, &gba.gameboy.timer, sizeof(state->timer));

    return true;
}

auto loadstate(Gba& gba, const struct State* state) -> bool
{
    if (!state || !gba.is_gb())
    {
        return false;
    }

    if (state->magic != STATE_MAGIC || state->version != STATE_VER || state->size != sizeof(struct State))
    {
        return false;
    }

    std::memcpy(&gba.gameboy.mem, &state->mem, sizeof(gba.gameboy.mem));
    std::memcpy(&gba.gameboy.cpu, &state->cpu, sizeof(gba.gameboy.cpu));
    std::memcpy(&gba.gameboy.ppu, &state->ppu, sizeof(gba.gameboy.ppu));
    std::memcpy(&gba.gameboy.cart, &state->cart, sizeof(gba.gameboy.cart));
    std::memcpy(&gba.gameboy.timer, &state->timer, sizeof(gba.gameboy.timer));

    // we need to reload mmaps
    setup_mmap(gba);
    // reload colours!
    update_all_colours_gb(gba);

    return true;
}

void enable_interrupt(Gba& gba, const enum Interrupts interrupt)
{
    GB_IO_IF |= interrupt;
    schedule_interrupt(gba);
}

void disable_interrupt(Gba& gba, const enum Interrupts interrupt)
{
    GB_IO_IF &= ~(interrupt);
    schedule_interrupt(gba);
}

auto render_layer(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> u8
{
    assert(gba.is_gb());

    if (is_system_gbc(gba))
    {
        return GBC_render_layer(gba, pixels, layer);
    }
    else
    {
        return DMG_render_layer(gba, pixels, layer);
    }
}

auto get_name_of_region(u16 addr) -> enum RegionName
{
    if (addr <= 0x3FFF)
    {
        return RegionName_ROM_BANK_0;
    }
    else if (addr >= 0x4000 && addr <= 0x7FFF)
    {
        return RegionName_ROM_BANK_X;
    }
    else if (addr >= 0x8000 && addr <= 0x9FFF)
    {
        return RegionName_VRAM;
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF)
    {
        return RegionName_EXTERNAL_RAM;
    }
    else if (addr >= 0xC000 && addr <= 0xCFFF)
    {
        return RegionName_WRAM_BANK_0;
    }
    else if (addr >= 0xD000 && addr <= 0xDFFF)
    {
        return RegionName_WRAM_BANK_1;
    }
    else if (addr >= 0xE000 && addr <= 0xEFFF)
    {
        return RegionName_WRAM_BANK_0_ECHO;
    }
    else if (addr >= 0xF000 && addr <= 0xFDFF)
    {
        return RegionName_WRAM_BANK_1_ECHO;
    }
    else if (addr >= 0xFE00 && addr <= 0xFE9F)
    {
        return RegionName_OAM;
    }
    else if (addr >= 0xFEA0 && addr <= 0xFEFF)
    {
        return RegionName_UNUSED;
    }
    else if (addr >= 0xFF00 && addr <= 0xFF7F)
    {
        return RegionName_IO;
    }
    else if (addr >= 0xFF80 && addr <= 0xFFFE)
    {
        return RegionName_HRAM;
    }
    else if (addr == 0xFFFF)
    {
        return RegionName_IE;
    }

    std::unreachable();
}

auto get_name_of_region_str(u16 addr) -> const char*
{
    static const char* names[] =
    {
        /*[RegionName_ROM_BANK_0] =*/"16KB ROM Bank 0",
        /*[RegionName_ROM_BANK_X] =*/"16KB ROM Bank X",
        /*[RegionName_VRAM] =*/"8KB VRAM",
        /*[RegionName_EXTERNAL_RAM] =*/"8KB SRAM",
        /*[RegionName_WRAM_BANK_0] =*/"WRAM Bank 0",
        /*[RegionName_WRAM_BANK_1] =*/"WRAM Bank 1",
        /*[RegionName_WRAM_BANK_0_ECHO] =*/"WRAM Bank 0 ECHO",
        /*[RegionName_WRAM_BANK_1_ECHO] =*/"WRAM Bank 1 ECHO",
        /*[RegionName_OAM] =*/"OAM",
        /*[RegionName_UNUSED] =*/"Not Usable",
        /*[RegionName_IO] =*/"I/O Ports",
        /*[RegionName_HRAM] =*/"HRAM",
        /*[RegionName_IE] =*/"IE",
    };

    return names[get_name_of_region(addr)];
}

static auto on_frame_event(Gba& gba)
{
    gba.scheduler.frame_end = true;
}

void run(Gba& gba, u32 tcycles)
{
    #if USE_SCHED
    gba.scheduler.frame_end = false;
    scheduler::add(gba, scheduler::Event::FRAME, on_frame_event, tcycles);

    if (gba.gameboy.cpu.halt)
    {
        on_halt_event(gba);

        if (gba.scheduler.frame_end)
        {
            return;
        }
    }

    for (;;)
    {
        cpu_run(gba);
        const auto cycles = gba.gameboy.cycles;

        gba.scheduler.cycles += cycles >> gba.gameboy.cpu.double_speed;
        if (gba.scheduler.next_event_cycles <= gba.scheduler.cycles)
        {
            scheduler::fire(gba);

            if (gba.scheduler.frame_end) [[unlikely]]
            {
                break;
            }
        }
    }
    #else
    for (u32 i = 0; i < tcycles; i += gba.gameboy.cycles >> gba.gameboy.cpu.double_speed)
    {
        cpu_run(gba);
        timer_run(gba, gba.gameboy.cycles);
        ppu_run(gba, gba.gameboy.cycles >> gba.gameboy.cpu.double_speed);
    }
    #endif
}

} // namespace gba::gb
