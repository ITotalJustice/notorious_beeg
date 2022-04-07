// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "render.hpp"
#include "gba.hpp"
#include "bit.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>

namespace gba::ppu {

constexpr auto CHARBLOCK_SIZE = 0x4000;
constexpr auto SCREENBLOCK_SIZE = 0x800;

constexpr auto BG_4BPP = 0;
constexpr auto BG_8BPP = 1;

[[maybe_unused]] constexpr auto BG0_NUM = 0;
[[maybe_unused]] constexpr auto BG1_NUM = 1;
[[maybe_unused]] constexpr auto BG2_NUM = 2;
[[maybe_unused]] constexpr auto BG3_NUM = 3;
constexpr auto BG_BACKDROP_NUM = 4;

[[maybe_unused]] constexpr auto PRIORITY_0 = 0;
[[maybe_unused]] constexpr auto PRIORITY_1 = 1;
[[maybe_unused]] constexpr auto PRIORITY_2 = 2;
[[maybe_unused]] constexpr auto PRIORITY_3 = 3;
constexpr auto PRIORITY_BACKDROP = 4;

enum class Window
{
    None, // no clipping
    Inside, // clip inside
    Outside, // clip outside
};

enum class Blend
{
    None, // no blending
    Alpha, // blend 2 layers
    White, // fade to white
    Black, // fade to black
};

enum class WinBlend
{
    None, // no clipped blend
    Inside, // only blend inside
    Outside, // only blend outside
};

struct BGxCNT
{
    constexpr BGxCNT(u16 cnt) :
        Pr{static_cast<u16>(bit::get_range<0, 1>(cnt))},
        CBB{static_cast<u16>(bit::get_range<2, 3>(cnt))},
        Mos{static_cast<u16>(bit::is_set<6>(cnt))},
        CM{static_cast<u16>(bit::is_set<7>(cnt))},
        SBB{static_cast<u16>(bit::get_range<8, 12>(cnt))},
        Wr{static_cast<u16>(bit::is_set<13>(cnt))},
        Sz{static_cast<u16>(bit::get_range<14, 15>(cnt))} {}

    u16 Pr : 2; // Priority. Determines drawing order of backgrounds.
    u16 CBB : 2; // Character Base Block. Sets the charblock that serves as the base for character/tile indexing. Values: 0-3.
    u16 Mos : 1; // Mosaic flag. Enables mosaic effect.
    u16 CM : 1; // Color Mode. 16 colors (4bpp) if cleared; 256 colors (8bpp) if set.
    u16 SBB : 5; // Screen Base Block. Sets the screenblock that serves as the base for screen-entry/map indexing. Values: 0-31.
    u16 Wr : 1; // Affine Wrapping flag. If set, affine background wrap around at their edges. Has no effect on regular backgrounds as they wrap around by default.
    u16 Sz : 2; // Background Size. Regular and affine backgrounds have different sizes available to them.
};

struct WININ
{
    constexpr WININ(auto v) :
        bg0{static_cast<u8>(bit::is_set<0>(v))},
        bg1{static_cast<u8>(bit::is_set<1>(v))},
        bg2{static_cast<u8>(bit::is_set<2>(v))},
        bg3{static_cast<u8>(bit::is_set<3>(v))},
        obj{static_cast<u8>(bit::is_set<4>(v))},
        blend{static_cast<u8>(bit::is_set<5>(v))}
    {
        in[0] = bg0;
        in[1] = bg1;
        in[2] = bg2;
        in[3] = bg3;
        in[4] = obj;
        in[5] = blend;
    }

private:
    const u8 bg0 : 1; // BG0 in winX
    const u8 bg1 : 1; // BG1 in winX
    const u8 bg2 : 1; // BG2 in winX
    const u8 bg3 : 1; // BG3 in winX
    const u8 obj : 1; // Sprites in winX
    const u8 blend : 1; // Blends in winX

public:
    bool in[6];
};

struct WINOUT
{
    constexpr WINOUT(u16 v) :
        bg0_out{static_cast<u8>(bit::is_set<0>(v))},
        bg1_out{static_cast<u8>(bit::is_set<1>(v))},
        bg2_out{static_cast<u8>(bit::is_set<2>(v))},
        bg3_out{static_cast<u8>(bit::is_set<3>(v))},
        obj_out{static_cast<u8>(bit::is_set<4>(v))},
        blend_out{static_cast<u8>(bit::is_set<5>(v))},
        bg0_in_obj_win{static_cast<u8>(bit::is_set<8>(v))},
        bg1_in_obj_win{static_cast<u8>(bit::is_set<9>(v))},
        bg2_in_obj_win{static_cast<u8>(bit::is_set<10>(v))},
        bg3_in_obj_win{static_cast<u8>(bit::is_set<11>(v))},
        obj_in_obj_win{static_cast<u8>(bit::is_set<12>(v))},
        blend_in_obj_win{static_cast<u8>(bit::is_set<13>(v))}
    {
        out[0] = bg0_out;
        out[1] = bg1_out;
        out[2] = bg2_out;
        out[3] = bg3_out;
        out[4] = obj_out;
        out[5] = blend_out;
        obj[0] = bg0_in_obj_win;
        obj[1] = bg1_in_obj_win;
        obj[2] = bg2_in_obj_win;
        obj[3] = bg3_in_obj_win;
        obj[4] = obj_in_obj_win;
        obj[5] = blend_in_obj_win;
    }

private:
    const u8 bg0_out : 1;
    const u8 bg1_out : 1;
    const u8 bg2_out : 1;
    const u8 bg3_out : 1;
    const u8 obj_out : 1;
    const u8 blend_out : 1;

    const u8 bg0_in_obj_win : 1;
    const u8 bg1_in_obj_win : 1;
    const u8 bg2_in_obj_win : 1;
    const u8 bg3_in_obj_win : 1;
    const u8 obj_in_obj_win : 1;
    const u8 blend_in_obj_win : 1;

public:
    bool out[6];
    bool obj[6];
};

struct BLDMOD
{
    constexpr BLDMOD(u16 v) :
        bg0_src{static_cast<u16>(bit::is_set<0>(v))},
        bg1_src{static_cast<u16>(bit::is_set<1>(v))},
        bg2_src{static_cast<u16>(bit::is_set<2>(v))},
        bg3_src{static_cast<u16>(bit::is_set<3>(v))},
        obj_src{static_cast<u16>(bit::is_set<4>(v))},
        backdrop_src{static_cast<u16>(bit::is_set<5>(v))},
        mode{static_cast<u16>(bit::get_range<6, 7>(v))},
        bg0_dst{static_cast<u16>(bit::is_set<8>(v))},
        bg1_dst{static_cast<u16>(bit::is_set<9>(v))},
        bg2_dst{static_cast<u16>(bit::is_set<10>(v))},
        bg3_dst{static_cast<u16>(bit::is_set<11>(v))},
        obj_dst{static_cast<u16>(bit::is_set<12>(v))},
        backdrop_dst{static_cast<u16>(bit::is_set<13>(v))}
    {
        src[0] = bg0_src;
        src[1] = bg1_src;
        src[2] = bg2_src;
        src[3] = bg3_src;
        src[4] = obj_src;
        src[5] = backdrop_src;
        dst[0] = bg0_dst;
        dst[1] = bg1_dst;
        dst[2] = bg2_dst;
        dst[3] = bg3_dst;
        dst[4] = obj_dst;
        dst[5] = backdrop_dst;
    }

    constexpr auto get_mode() const
    {
        return static_cast<Blend>(mode);
    }

private:
    const u16 bg0_src : 1; // Blend BG0 (source)
    const u16 bg1_src : 1; // Blend Bg1 (source)
    const u16 bg2_src : 1; // Blend BG2 (source)
    const u16 bg3_src : 1; // Blend BG3 (source)
    const u16 obj_src : 1; // Blend sprites (source)
    const u16 backdrop_src : 1; // Blend backdrop (source)
    const u16 mode : 2; // Blend Mode
    const u16 bg0_dst : 1; // Blend BG0 (target)
    const u16 bg1_dst : 1; // Blend BG1 (target)
    const u16 bg2_dst : 1; // Blend BG2 (target)
    const u16 bg3_dst : 1; // Blend BG3 (target)
    const u16 obj_dst : 1; // Blend sprites (target)
    const u16 backdrop_dst : 1; // Blend backdrop (target)

public:
    bool src[6];
    bool dst[6];
};

// NOTE: The affine screen entries are only 8 bits wide and just contain an 8bit tile index.
struct ScreenEntry
{
    constexpr ScreenEntry(u16 screen_entry) :
        tile_index{static_cast<u16>(bit::get_range<0, 9>(screen_entry))},
        hflip{static_cast<u16>(bit::is_set<10>(screen_entry))},
        vflip{static_cast<u16>(bit::is_set<11>(screen_entry))},
        palette_bank{static_cast<u16>(bit::get_range<12, 15>(screen_entry))} {}

    const u16 tile_index : 10; // max 512
    const u16 hflip : 1;
    const u16 vflip : 1;
    const u16 palette_bank : 4;
};

struct Attr0
{
    constexpr Attr0(u16 v) :
        Y{static_cast<u16>(bit::get_range<0, 7>(v))},
        OM{static_cast<u16>(bit::get_range<8, 9>(v))},
        GM{static_cast<u16>(bit::get_range<10, 11>(v))},
        Mos{static_cast<u16>(bit::is_set<12>(v))},
        CM{static_cast<u16>(bit::is_set<13>(v))},
        Sh{static_cast<u16>(bit::get_range<14, 15>(v))} {}

    const u16 Y : 8; // Y coordinate. Marks the top of the sprite.
    const u16 OM : 2; // (Affine) object mode. Use to hide the sprite or govern affine mode.
    const u16 GM : 2; // Gfx mode. Flags for special effects.
    const u16 Mos : 1; // Enables mosaic effect. Covered here.
    const u16 CM : 1; // Color mode. 16 colors (4bpp) if cleared; 256 colors (8bpp) if set.
    const u16 Sh : 2; // Sprite shape. This and the sprite's size (attr1{E-F}) determines the sprite's real size

    [[nodiscard]] constexpr auto is_disabled() const { return OM == 0b10; }
    [[nodiscard]] constexpr auto is_4bpp() const { return CM == 0; }
    [[nodiscard]] constexpr auto is_8bpp() const { return CM == 1; }
};

struct Attr1
{
    constexpr Attr1(u16 v) :
        X{static_cast<s16>(bit::get_range<0, 8>(v))},
        AID{static_cast<u16>(bit::get_range<9, 13>(v))},
        HF{static_cast<u16>(bit::is_set<12>(v))},
        VF{static_cast<u16>(bit::is_set<13>(v))},
        Sz{static_cast<u16>(bit::get_range<14, 15>(v))} {}

    const s16 X : 9; // X coordinate. Marks left of the sprite.
    const u16 AID : 5; // Affine index. Specifies the OAM_AFF_ENTY this sprite uses. Valid only if the affine flag (attr0{8}) is set.
    const u16 HF : 1; // Horizontal flipping flag. Used only if the affine flag (attr0) is clear; otherwise they're part of the affine index.
    const u16 VF : 1; // vertical flipping flag. Used only if the affine flag (attr0) is clear; otherwise they're part of the affine index.
    const u16 Sz : 2; // Sprite size. Kinda. Together with the shape bits (attr0{E-F}) these determine the sprite's real size.
};

struct Attr2
{
    constexpr Attr2(u16 v) :
        TID{static_cast<u16>(bit::get_range<0, 9>(v))},
        Pr{static_cast<u16>(bit::get_range<10, 11>(v))},
        PB{static_cast<u16>(bit::get_range<12, 15>(v))}{}

    const u16 TID : 10; // Base tile-index of sprite. Note that in bitmap modes this must be 512 or higher.
    const u16 Pr : 2; // Priority. Higher priorities are drawn first (and therefore can be covered by later sprites and backgrounds). Sprites cover backgrounds of the same priority, and for sprites of the same priority, the higher OBJ_ATTRs are drawn first.
    const u16 PB : 4; // Palette-bank to use when in 16-color mode. Has no effect if the color mode flag (attr0{C}) is set.
};

struct OBJ_Attr
{
    constexpr OBJ_Attr(u64 v) :
        attr0{static_cast<u16>(v >> 0)},
        attr1{static_cast<u16>(v >> 16)},
        attr2{static_cast<u16>(v >> 32)},
        fill{static_cast<s16>(v >> 48)} {}

    const Attr0 attr0; // The first attribute controls a great deal, but the most important parts are for the y coordinate, and the shape of the sprite. Also important are whether or not the sprite is transformable (an affine sprite), and whether the tiles are considered to have a bitdepth of 4 (16 colors, 16 sub-palettes) or 8 (256 colors / 1 palette).
    const Attr1 attr1; // The primary parts of this attribute are the x coordinate and the size of the sprite. The role of bits 8 to 14 depend on whether or not this is a affine sprite (determined by attr0{8}). If it is, these bits specify which of the 32 OBJ_AFFINEs should be used. If not, they hold flipping flags.
    const Attr2 attr2; // This attribute tells the GBA which tiles to display and its background priority. If it's a 4bpp sprite, this is also the place to say what sub-palette should be used.
    const s16 fill; // used for affine stuff

    [[nodiscard]] constexpr auto is_yflip() const
    {
        // flipping is only used in normal mode
        if (attr0.OM == 0b00)
        {
            return attr1.VF == true;
        }
        return false;
    }

    [[nodiscard]] constexpr auto is_xflip() const
    {
        // flipping is only used in normal mode
        if (attr0.OM == 0b00)
        {
            return attr1.HF == true;
        }
        return false;
    }

    struct [[nodiscard]] SizePair { u8 x; u8 y; };

    auto get_size() const -> SizePair
    {
        static constexpr SizePair sizes[4][4] =
        {
            { { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 64 } },
            { { 16, 8 }, { 32, 8 }, { 32, 16 }, { 64, 32 } },
            { { 8, 16 }, { 8, 32 }, { 16, 32 }, { 32, 64 } },
            { /* invalid */ }
        };

        return sizes[attr0.Sh][attr1.Sz];
    }
};

template<bool Y> [[nodiscard]] // 0=Y, 1=X
static auto get_bg_offset(BGxCNT cnt, auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;

    constexpr u16 mod[2][4] = // [0] = Y, [1] = X
    {
        { 256, 256, 512, 512 },
        { 256, 512, 256, 512 },
    };

    static constexpr u16 result[2][4] = // [0] = Y, [1] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[Y][cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[Y][cnt.Sz] * next_block;
}

template<bool Y> [[nodiscard]] // 0=Y, 1=X
static auto get_bg_offset_affine(BGxCNT cnt, auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;
    constexpr u16 mod[4] = { 128, 256, 512, 1024 };
    static constexpr u16 result[2][4] = // [0] = Y, [1] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[Y][cnt.Sz] * next_block;
}

struct Line
{
    Line(std::span<u16> _pixels) : pixels{_pixels}
    {
        std::ranges::fill(priority, PRIORITY_BACKDROP);
        std::ranges::fill(num, BG_BACKDROP_NUM);
    }

    std::span<u16> pixels;
    u8 priority[240];
    u8 num[240]; // bg number
};

struct Bgr
{
    constexpr Bgr(u16 col) :
        r{(u8)bit::get_range<0, 4>(col)},
        g{(u8)bit::get_range<5, 9>(col)},
        b{(u8)bit::get_range<10, 14>(col)} {}

    [[nodiscard]] constexpr auto pack() const -> u16
    {
        return (std::min<u8>(31, b) << 10) | (std::min<u8>(31, g) << 5) | (std::min<u8>(31, r));
    }

    u8 r;
    u8 g;
    u8 b;
};

// the blending *formular* took way longer than it should've
// NOTE: don't try to apply the coeff directly to the bgr555 colour, it won't work.
// have to apply it to each b,g,r value.
// masking the b,g,r is handled by the bitfield.
static constexpr auto blend_alpha(u16 src, u16 dst, u8 coeff_src, u8 coeff_dst) -> u16
{
    // disable this to have very bright colours in phoenix wright
    coeff_src = std::min<u8>(16, coeff_src);
    coeff_dst = std::min<u8>(16, coeff_dst);

    const Bgr src_col{src};
    const Bgr dst_col{dst};
    Bgr fin_col{0};

    fin_col.r = ((src_col.r * coeff_src) + (dst_col.r * coeff_dst)) / 16;
    fin_col.g = ((src_col.g * coeff_src) + (dst_col.g * coeff_dst)) / 16;
    fin_col.b = ((src_col.b * coeff_src) + (dst_col.b * coeff_dst)) / 16;

    return fin_col.pack();
}

static constexpr auto blend_white(u16 col, u8 coeff) -> u16
{
    const Bgr src_col{col};
    Bgr fin_col{0};

    // eg (((31 - 0) * 16) / 16) = max white
    // eg (((31 - 0) * 32) / 16) = max white (masked by bitfield)
    fin_col.r = src_col.r + (((31 - src_col.r) * coeff) / 16);
    fin_col.g = src_col.g + (((31 - src_col.g) * coeff) / 16);
    fin_col.b = src_col.b + (((31 - src_col.b) * coeff) / 16);
    return fin_col.pack();
}

static constexpr auto blend_black(u16 col, u8 coeff) -> u16
{
    const Bgr src_col{col};
    Bgr fin_col{0};

    // eg (16 - ((16 * 0) / 16)) = max black
    fin_col.r = src_col.r - ((src_col.r * coeff) / 16);
    fin_col.g = src_col.g - ((src_col.g * coeff) / 16);
    fin_col.b = src_col.b - ((src_col.b * coeff) / 16);
    return fin_col.pack();
}

static auto render_obj(Gba& gba, Line& line) -> void
{
    // keep track if a sprite has already been rendered.
    u8 drawn[240];
    std::ranges::fill(drawn, 0xFF);

    // ovram is the last 2 entries of the charblock in vram.
    // in tile modes, this allows for 1024 tiles in total.
    // in bitmap modes, this allows for 512 tiles in total.
    const auto ovram = reinterpret_cast<u8*>(gba.mem.vram + 4 * CHARBLOCK_SIZE);
    const auto pram = reinterpret_cast<u16*>(gba.mem.palette_ram + 512);
    const auto oam = reinterpret_cast<u64*>(gba.mem.oam);
    const auto vcount = REG_VCOUNT;

    // 1024 entries in oam, each entry is 64bytes, 1024/64=128
    for (auto i = 0; i < 128; i++)
    {
        const OBJ_Attr obj = oam[i];

        if (obj.attr0.is_disabled())
        {
            continue;
        }

        // skip all non-normal sprites for now
        if (obj.attr0.OM != 0b00)
        {
            // std::printf("skipping affine sprite\n");
            // continue;
        }

        // skip any non-normal sprites for now
        if (obj.attr0.GM != 0b00)
        {
            continue;
        }

        // fetch the x/y size of the sprite
        const auto [xSize, ySize] = obj.get_size();
        // see here for wrapping: https://www.coranac.com/tonc/text/affobj.htm#ssec-wrap
        const auto sprite_y = obj.attr0.Y + ySize > 256 ? obj.attr0.Y - 256 : obj.attr0.Y;
        // calculate the y_index, handling flipping
        const auto mosY = obj.is_yflip() ? (ySize - 1) - (vcount - sprite_y) : vcount - sprite_y;
        // fine_y
        const auto yMod = mosY % 8;

        // check if the sprite is to be drawn on this ;ine
        if (vcount >= sprite_y && vcount < sprite_y + ySize)
        {
            // for each pixel of the sprite
            for (auto x = 0; x < xSize; x++)
            {
                // this is the index into the pixel array
                const auto pixel_x = obj.attr1.X + x;

                // check its in bounds
                if (pixel_x < 0 || pixel_x >= 240)
                {
                    continue;
                }

                // skip obj already rendered over higher or equal prio
                if (drawn[pixel_x] <= obj.attr2.Pr)
                {
                    continue;
                }

                // bg over obj
                // NOTE: this breaks metroid zero as the obj is supposed
                // to blend with bg0. coeff for bg0 is 0, so obj is simply
                // rendered over.
                if (line.priority[pixel_x] < obj.attr2.Pr)
                {
                    continue;
                }

                // x_index, handling flipping
                const auto mosX = obj.is_xflip() ? xSize - 1 - x : x;
                // fine_x
                const auto xMod = mosX % 8;

                assert(obj.attr0.is_4bpp());

                // thank you Kellen for the below code, i wouldn't have firgured it out otherwise.
                auto tileRowAddress = obj.attr2.TID * 32;
                tileRowAddress += (((mosY / 8 * xSize / 8)) + mosX / 8) * 32;
                tileRowAddress += yMod * 4;

                // if the adddress is out of bounds, break early
                if (tileRowAddress >= CHARBLOCK_SIZE * 2) [[unlikely]]
                {
                    break;
                }

                // in bitmap mode, only the last charblock can be used for sprites.
                if (is_bitmap_mode(gba) && tileRowAddress < CHARBLOCK_SIZE) [[unlikely]]
                {
                    continue;
                }

                auto pram_addr = 0;
                auto pixel = 0;

                if (obj.attr0.is_4bpp())
                {
                    pixel = ovram[tileRowAddress + xMod/2];

                    if (xMod & 1) // odd/even (lo/hi nibble)
                    {
                        pixel >>= 4;
                    }
                    pixel &= 0xF;
                    pram_addr = pixel * 2;
                    pram_addr += obj.attr2.PB * 32;
                }
                else
                {
                    pixel = ovram[tileRowAddress + xMod];
                    pram_addr = pixel * 2;
                    pram_addr += obj.attr2.PB * 32;
                }

                // don't render transparent pixels
                if (pixel != 0)
                {
                    drawn[pixel_x] = obj.attr2.Pr;
                    line.pixels[pixel_x] = pram[pram_addr / 2];
                }
            }
        }
    }
}

template<Window window, Blend blend, WinBlend winblend, u8 bpp>
static auto render_line_bg(Gba& gba, Line& line, BGxCNT cnt, u16 xscroll, u16 yscroll, u16 winx_start, u16 winx_end)
{
    const auto coeff_src = bit::get_range<0, 4>(REG_COLEV);
    const auto coeff_dst = bit::get_range<8, 12>(REG_COLEV);
    const auto coeff_wb = bit::get_range<0, 4>(REG_COLEY);

    const auto vcount = REG_VCOUNT;
    //
    const auto y = (yscroll + vcount) % 256;
    // pal_mem
    const auto pram = reinterpret_cast<u16*>(gba.mem.palette_ram);
    // tile_mem (where the tiles/tilesets are)
    const auto charblock = reinterpret_cast<u8*>(gba.mem.vram + cnt.CBB * CHARBLOCK_SIZE);
    // se_mem (where the tilemaps are)
    const auto screenblock = reinterpret_cast<u16*>(gba.mem.vram + (cnt.SBB * SCREENBLOCK_SIZE) + get_bg_offset<0>(cnt, yscroll + vcount) + ((y / 8) * 64));

    for (auto x = 0; x < 240; x++)
    {
        const auto in_range = (x >= winx_start && x < winx_end);

        // only render if inside the window
        if constexpr(window == Window::Inside)
        {
            if (!in_range)
            {
                continue;
            }
        }
        // only render if outside the window
        else if constexpr(window == Window::Outside)
        {
            if (in_range)
            {
                continue;
            }
        }

        const auto tx = (x + xscroll) % 256;
        const auto se_number = (tx / 8) + (get_bg_offset<1>(cnt, x + xscroll) / 2); // SE-number n = tx+ty·tw,
        const ScreenEntry se = screenblock[se_number];

        const auto tile_x = se.hflip ? 7 - (tx & 7) : tx & 7;
        const auto tile_y = se.vflip ? 7 - (y & 7) : y & 7;

        const auto tile_offset = tile_x + (tile_y * 8);

        auto pram_addr = 0;
        auto pixel = 0;

        // todo: don't allow access to blocks 4,5
        if constexpr(bpp == BG_4BPP)
        {
            pixel = charblock[(se.tile_index * 32) + tile_offset/2];

            if (tile_x & 1) // hi or lo nibble
            {
                pixel >>= 4;
            }
            pixel &= 0xF; // 4bpp remember
            pram_addr = 2 * pixel;
            pram_addr += se.palette_bank * 32;
        }
        else // BG_8BPP
        {
            pixel = charblock[(se.tile_index * 64) + tile_offset];
            pram_addr = 2 * pixel;
        }

        if (pixel != 0) // don't render transparent pixel
        {
            // all branches involving constants will be optimised away
            bool should_blend = blend != Blend::None;

            if constexpr(winblend == WinBlend::Inside)
            {
                if (!in_range)
                {
                    should_blend = false;
                }
            }
            else if constexpr(winblend == WinBlend::Outside)
            {
                if (in_range)
                {
                    should_blend = false;
                }
            }

            const auto colour = pram[pram_addr / 2];
            line.priority[x] = cnt.Pr;

            // this is all optimised away if blending is disabled
            if (blend != Blend::None && should_blend)
            {
                if constexpr(blend == Blend::Alpha)
                {
                    const auto dst = line.pixels[x];
                    const auto src = colour;

                    line.pixels[x] = blend_alpha(src, dst, coeff_src, coeff_dst);
                }
                else if (blend == Blend::White)
                {
                    line.pixels[x] = blend_white(colour, coeff_wb);
                }
                else if (blend == Blend::Black)
                {
                    line.pixels[x] = blend_black(colour, coeff_wb);
                }
            }
            else
            {
                line.pixels[x] = colour;
            }
        }
    }
}

// todo: create better name
struct Set
{
    BGxCNT cnt;
    u16 xscroll;
    u16 yscroll;
    bool enable;
    int num;
};

template<Window window, Blend blend, WinBlend winblend>
static auto render_line_bg(Gba& gba, Line& line, BGxCNT cnt, u16 xscroll, u16 yscroll, u16 winx_start, u16 winx_end)
{
    if (cnt.CM == BG_4BPP)
    {
       render_line_bg<window, blend, winblend, BG_4BPP>(gba, line, cnt, xscroll, yscroll, winx_start, winx_end);
    }
    else if (cnt.CM == BG_8BPP)
    {
        render_line_bg<window, blend, winblend, BG_8BPP>(gba, line, cnt, xscroll, yscroll, winx_start, winx_end);
    }
}

template<Window window, Blend blend>
static auto render_line_bg(Gba& gba, WinBlend winblend, Line& line, BGxCNT cnt, u16 xscroll, u16 yscroll, u16 winx_start, u16 winx_end)
{
    // if blending is disabled, winblend is also disabled
    if constexpr(blend == Blend::None)
    {
        render_line_bg<window, blend, WinBlend::None>(gba, line, cnt, xscroll, yscroll, winx_start, winx_end);
    }
    else
    {
        switch (winblend)
        {
            case WinBlend::None:
                render_line_bg<window, blend, WinBlend::None>(gba, line, cnt, xscroll, yscroll, winx_start, winx_end);
                break;

            case WinBlend::Inside:
                render_line_bg<window, blend, WinBlend::Inside>(gba, line, cnt, xscroll, yscroll, winx_start, winx_end);
                break;

            case WinBlend::Outside:
                render_line_bg<window, blend, WinBlend::Outside>(gba, line, cnt, xscroll, yscroll, winx_start, winx_end);
                break;
        }
    }
}

template<Window window>
static auto render_line_bg(Gba& gba, Blend blend, WinBlend winblend, Line& line, BGxCNT cnt, u16 xscroll, u16 yscroll, u16 winx_start, u16 winx_end)
{
    switch (blend)
    {
        case Blend::None:
            render_line_bg<window, Blend::None>(gba, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;

        case Blend::Alpha:
            render_line_bg<window, Blend::Alpha>(gba, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;

        case Blend::White:
            render_line_bg<window, Blend::White>(gba, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;

        case Blend::Black:
            render_line_bg<window, Blend::Black>(gba, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;
    }
}

static auto render_line_bg(Gba& gba, Window window, Blend blend, WinBlend winblend, Line& line, BGxCNT cnt, u16 xscroll, u16 yscroll, u16 winx_start, u16 winx_end)
{
    switch (window)
    {
        case Window::None:
            render_line_bg<Window::None>(gba, blend, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;

        case Window::Inside:
            render_line_bg<Window::Inside>(gba, blend, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;

        case Window::Outside:
            render_line_bg<Window::Outside>(gba, blend, winblend, line, cnt, xscroll, yscroll, winx_start, winx_end);
            break;
    }
}

static auto render_backdrop(Gba& gba)
{
    const auto pram = reinterpret_cast<u16*>(gba.mem.palette_ram);
    std::ranges::fill(gba.ppu.pixels[REG_VCOUNT], pram[0]);
}

static constexpr auto sort_priority_set(auto& set)
{
    std::ranges::stable_sort(set, [](auto lhs, auto rhs){
        return lhs.cnt.Pr > rhs.cnt.Pr;
    });
}

static auto render_set(Gba& gba, std::span<Set> set, Line& line)
{
    // is the window enabled
    const auto win0 = bit::is_set<13>(REG_DISPCNT);
    const auto win1 = bit::is_set<14>(REG_DISPCNT);
    // todo: what is an obj window?????????
    // const auto win_obj = bit::is_set<15>(REG_DISPCNT);

    // parse the window bits
    WININ win_in[2] = { { REG_WININ }, { REG_WININ >> 8 } };
    const WINOUT win_out{REG_WINOUT};

    // window stuff, wrapping isn't handled yet
    u16 x_start = 0; // leftmost (inclusive)
    u16 x_end = 0; // rightmost (exclusive)
    u16 y_start = 0; // top (inclusive)
    u16 y_end = 0; // bottom (exclusive)

    const BLDMOD bldmod{REG_BLDMOD};
    const auto blend_mode = bldmod.get_mode();

    const WININ* winin = nullptr;
    bool window_enabled = false;

    if (win1)
    {
        x_start = bit::get_range<8, 15>(REG_WIN1H);
        x_end = bit::get_range<0, 7>(REG_WIN1H);
        y_start = bit::get_range<8, 15>(REG_WIN1V);
        y_end = bit::get_range<0, 7>(REG_WIN1V);

        window_enabled = true;
        winin = &win_in[1];
    }
    if (win0)
    {
        x_start = bit::get_range<8, 15>(REG_WIN0H);
        x_end = bit::get_range<0, 7>(REG_WIN0H);
        y_start = bit::get_range<8, 15>(REG_WIN0V);
        y_end = bit::get_range<0, 7>(REG_WIN0V);

        window_enabled = true;
        winin = &win_in[0];
    }

    const auto in_range = REG_VCOUNT >= y_start && REG_VCOUNT < y_end;

    for (std::size_t i = 0; i < set.size(); i++)
    {
        auto& p = set[i];

        // not enabled, skip
        if (p.enable == false)
        {
            continue;
        }

        // the below code is just a really bad truth table.
        // eventually, it will be an array of function pointers.
        Blend blend = Blend::None;
        Window window = Window::None;
        WinBlend winblend = WinBlend::None;

        if (window_enabled)
        {
            // check is this bg is allowed at all
            if (winin->in[p.num] == 0 && win_out.out[p.num] == 0)
            {
                continue;
            }

            // check if clipping is enabled
            // if bg is enabled in/out, then we dont check for clipping
            if (winin->in[p.num] == 0 || win_out.out[p.num] == 0)
            {
                // first check if we are in_range
                if (in_range)
                {
                    // have to make sure that we can render inside the window
                    if (winin->in[p.num])
                    {
                        window = Window::Inside; // clip x
                    }
                    // othewise, clip outside
                    else
                    {
                        window = Window::Outside; // clip x
                    }
                }
                else
                {
                    // check if we are allowed outisde the window
                    if (win_out.out[p.num])
                    {
                        // window = Window::Inside; // clip x
                    }
                    else
                    {
                        continue; // skip this bg
                    }
                }
            }
        }

        // handle blending
        if (blend_mode != Blend::None)
        {
            // check is this is a src target
            if (bldmod.src[p.num])
            {
                blend = blend_mode;
            }
        }

        // windowed blending!
        if (window_enabled && blend != Blend::None)
        {
            // blending may be allowed inside and outside
            // check that we are going to have clipped blending
            if (winin->in[5] == 0 || win_out.out[5] == 0)
            {
                if (in_range)
                {
                    // check if we are allowed to blend inside
                    if (winin->in[5])
                    {
                        winblend = WinBlend::Inside; // clip x blend
                    }
                    // otherwise, blend outside (clipped)
                    else
                    {
                        winblend = WinBlend::Outside; // clip x blend
                    }
                }
                else
                {
                    // check if we are allowed to blend outside
                    if (win_out.out[5])
                    {
                        // winblend = WinBlend::Outside; // clip x blend
                    }
                    // otherwise, disable blending
                    else
                    {
                        blend = Blend::None; // disable blending
                    }
                }
            }
        }

        render_line_bg(gba, window, blend, winblend, line, p.cnt, p.xscroll, p.yscroll, x_start, x_end);
    }
}

// 4 regular
static auto render_mode0(Gba& gba) -> void
{
    Line line{gba.ppu.pixels[REG_VCOUNT]};

    Set set[4] =
    {
        { REG_BG3CNT, REG_BG3HOFS, REG_BG3VOFS, bit::is_set<11>(REG_DISPCNT), 3 },
        { REG_BG2CNT, REG_BG2HOFS, REG_BG2VOFS, bit::is_set<10>(REG_DISPCNT), 2 },
        { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT), 1 },
        { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT), 0 }
    };

    render_backdrop(gba);
    sort_priority_set(set);
    render_set(gba, set, line);

    if (bit::is_set<12>(REG_DISPCNT))
    {
        assert(bit::is_set<6>(REG_DISPCNT));
        render_obj(gba, line);
    }
}

// 2 regular, 1 affine
static auto render_mode1(Gba& gba) -> void
{
    Line line{gba.ppu.pixels[REG_VCOUNT]};

    Set set[2] =
    {
        { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT), 1 },
        { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT), 0 }
    };

    render_backdrop(gba);
    sort_priority_set(set);
    render_set(gba, set, line);

    // todo: affine
    if (bit::is_set<12>(REG_DISPCNT))
    {
        assert(bit::is_set<6>(REG_DISPCNT));
        render_obj(gba, line);
    }
}

// 4 affine
static auto render_mode2(Gba& gba) -> void
{
    render_backdrop(gba);
    assert(0);

    // todo: affine*4
    // todo: obj
}

static auto render_mode3(Gba& gba) noexcept -> void
{
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];
    const auto vram = reinterpret_cast<u16*>(gba.mem.vram);
    std::memcpy(pixels, vram + 240 * REG_VCOUNT, sizeof(pixels));
}

static auto render_mode4(Gba& gba) noexcept -> void
{
    const auto page = bit::is_set<4>(REG_DISPCNT) ? 0xA000 : 0;
    auto addr = page + (240 * REG_VCOUNT);
    auto& pixels = gba.ppu.pixels[REG_VCOUNT];
    const auto pram = reinterpret_cast<u16*>(gba.mem.palette_ram);

    std::ranges::for_each(pixels, [&gba, &addr, pram](auto& pixel){
        pixel = pram[gba.mem.vram[addr++]];
    });
}

auto render(Gba& gba) -> void
{
    // if forced blanking is enabled, the screen is black
    if (bit::is_set<7>(REG_DISPCNT)) [[unlikely]]
    {
        std::ranges::fill(gba.ppu.pixels[REG_VCOUNT], 0);
        return;
    }

    const auto mode = get_mode(gba);

    switch (mode)
    {
        case 0: render_mode0(gba); break;
        case 1: render_mode1(gba); break;
        case 2: render_mode2(gba); break;
        case 3: render_mode3(gba); break;
        case 4: render_mode4(gba); break;
        // case 5: // only unhandled mode atm

        default:
            std::printf("unhandled ppu mode: %u\n", mode);
            break;
    }
}

auto render_bg_mode(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> u8
{
    Line line{pixels};

    if (mode == 0)
    {
        const Set set[4] =
        {
            { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT), 0 },
            { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT), 1 },
            { REG_BG2CNT, REG_BG2HOFS, REG_BG2VOFS, bit::is_set<10>(REG_DISPCNT), 2 },
            { REG_BG3CNT, REG_BG3HOFS, REG_BG3VOFS, bit::is_set<11>(REG_DISPCNT), 3 },
        };

        render_line_bg<Window::None, Blend::None, WinBlend::None>(gba, line, set[layer].cnt, set[layer].xscroll, set[layer].yscroll, 0, 0);
        return set[layer].cnt.Pr;
    }

    if (mode == 1)
    {
        const Set set[2] =
        {
            { REG_BG0CNT, REG_BG0HOFS, REG_BG0VOFS, bit::is_set<8>(REG_DISPCNT), 0 },
            { REG_BG1CNT, REG_BG1HOFS, REG_BG1VOFS, bit::is_set<9>(REG_DISPCNT), 1 },
        };

        render_line_bg<Window::None, Blend::None, WinBlend::None>(gba, line, set[layer].cnt, set[layer].xscroll, set[layer].yscroll, 0, 0);
        return set[layer].cnt.Pr;
    }

    return 0;
}

} // namespace gba::ppu
