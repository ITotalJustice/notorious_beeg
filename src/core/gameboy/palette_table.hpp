#pragma once

#include "fwd.hpp"

namespace gba::gb {

/* SOURCES: */
/* Button:  https://tcrf.net/Notes:Game_Boy_Color_Bootstrap_ROM#Manual_Select_Palette_Configurations */
/* Hash:    https://tcrf.net/Notes:Game_Boy_Color_Bootstrap_ROM#Assigned_Palette_Configurations */
/* Unused:  https://tcrf.net/Game_Boy_Color_Bootstrap_ROM#Unused_Palette_Configurations */

enum { PALETTE_TABLE_MIN = 0x0 };
enum { PALETTE_TABLE_MAX = 0x5 };

enum { PALETTE_ENTRY_MIN = 0x00 };
enum { PALETTE_ENTRY_MAX = 0x1C };

enum CustomPalette {
    CUSTOM_PALETTE_GREY,
    CUSTOM_PALETTE_GREEN,
    CUSTOM_PALETTE_CREAM,
    CUSTOM_PALETTE_KGREEN,
    CUSTOM_PALETTE_DEFAULT = CUSTOM_PALETTE_CREAM,

    CUSTOM_PALETTE_MAX = 0xFF
};

struct PaletteEntry {
    u32 BG[4];
    u32 OBJ0[4];
    u32 OBJ1[4];
};

struct PalettePreviewShades {
    u32 shade1;
    u32 shade2;
};

auto palette_fill_from_table_entry(
    u8 table, u8 entry, /* keys */
    struct PaletteEntry* palette
) -> bool;

/* hash over the header title */
/* add each title entry % 100 */
/* forth byte is the 4th byte in the title, used for hash collisions. */
auto palette_fill_from_hash(
    u8 hash, /* key */
    u8 forth_byte, /* key */
    bool use_default, /* set to true to use the default palette if not found */
    struct PaletteEntry* palette
) -> bool;

/* fill palette using buttons as the key */
auto palette_fill_from_buttons(
    u8 buttons, /* key */
    struct PaletteEntry* palette,
    struct PalettePreviewShades* preview /* optional (can be NULL) */
) -> bool;

auto palette_fill_from_custom(
    enum CustomPalette custom, /* key */
    struct PaletteEntry* palette
) -> bool;

} // namespace gba::gb
