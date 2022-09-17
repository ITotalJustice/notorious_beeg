#pragma once

#include "types.hpp"
#include "fwd.hpp"
#include <span>

namespace gba::gb {

auto init(Gba& gba) -> bool;
void reset(Gba& gba);

void skip_frame(Gba& gba, bool enable);

// todo: explain this function
void set_sram(Gba& gba, u8* ram, std::size_t size);

// todo: explain this function
auto get_rom_info(const u8* data, std::size_t size, struct RomInfo* info_out) -> bool;

// pass the fully loaded rom data.
// this memory is NOT owned.
// freeing the memory should still be handled by the caller!
auto loadrom(Gba& gba, std::span<const u8> rom) -> bool;

auto has_save(const Gba& gba) -> bool;
auto has_rtc(const Gba& gba) -> bool;

auto calculate_savedata_size(const Gba& gba) -> std::size_t;

// save/loadstate to struct.
auto savestate(const Gba& gba, struct State* state) -> bool;
auto loadstate(Gba& gba, const struct State* state) -> bool;

// pass in filled out rtc struct.
// NOTE: the s, m, h will be clamped to the max values
// so there won't be 255 seconds, it'll be clamped to 59.
// returns false if the game does not support rtc.
auto set_rtc(Gba& gba, struct Rtc rtc) -> bool;

auto has_mbc_flags(const Gba& gba, u8 flags) -> bool;

/* run for number of cycles */
void run(Gba& gba, u32 tcycles = gb::FRAME_CPU_CYCLES);

auto get_system_type(const Gba& gba) -> enum SystemType;

// calls get_system_type(gba and compares the result
auto is_system_gbc(const Gba& gba) -> bool;

/*  */
auto get_rom_name(const Gba& gba, struct CartName* name) -> int;
auto get_rom_name_from_header(const struct CartHeader* header, struct CartName* name) -> int;

// this can be used to set multiple buttons down or up
// at once, or can set just 1.
void set_buttons(Gba& gba, u8 buttons, bool is_down);
auto get_buttons(const Gba& gba) -> u8;
auto is_button_down(const Gba& gba, enum Button button) -> bool;

// make this a seperate header, gb_adv.h, add these there
auto get_rom_palette_hash_from_header(const struct CartHeader* header, u8* hash, u8* forth) -> bool;

auto get_rom_palette_hash(const Gba& gba, u8* hash, u8* forth) -> bool;

auto set_palette_from_table_entry(Gba& gba, u8 table, u8 entry) -> bool;

auto set_palette_from_hash(Gba& gba, u8 hash) -> bool;

// todo: show button table here or link to the table!
auto set_palette_from_buttons(Gba& gba, u8 buttons) -> bool;

/* set a custom palette */
auto set_palette_from_palette(Gba& gba, const struct PaletteEntry* palette) -> bool;

//
void update_all_colours_gb(Gba& gba);


/* -------------------- ADVANCED STUFF -------------------- */


// fills out the header struct using the loaded rom data
auto get_rom_header(const Gba& gba, struct CartHeader* header) -> bool;

/* returns a pointer to the loaded rom data as a CartHeader */
auto get_rom_header_ptr(const Gba& gba) -> const struct CartHeader*;

void cpu_set_flag(Gba& gba, enum CpuFlags flag, bool value);
auto cpu_get_flag(const Gba& gba, enum CpuFlags flag) -> bool;

void cpu_set_register(Gba& gba, enum CpuRegisters reg, u8 value);
auto cpu_get_register(const Gba& gba, enum CpuRegisters reg) -> u8;

void cpu_set_register_pair(Gba& gba, enum CpuRegisterPairs pair, u16 value);
auto cpu_get_register_pair(const Gba& gba, enum CpuRegisterPairs pair) -> u16;

auto render_layer(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> u8;

auto get_name_of_region(u16 addr) -> enum RegionName;
auto get_name_of_region_str(u16 addr) -> const char*;

} // namespace gba::gb
