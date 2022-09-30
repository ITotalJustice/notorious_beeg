// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// things left
// - obj affine
// - obj mosaic
// - window wrapping

// todo:
// - cache oam entries
// - cache screen entries
#include "render.hpp"
#include "gba.hpp"
#include "bit.hpp"
#include "mem.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <utility>
#include <bit>

namespace gba::ppu {
namespace {

// this is the same as mem::read_array but without the mask
// this saves at least 1 (and) instruction on x86 and ppc
// this function is called in very tight loops so it's vital that
// it's optimised as possible!
template <typename T> [[nodiscard]]
auto read_array_no_mask(std::span<const u8> array, u32 addr) -> T
{
    addr = mem::align<T>(addr);

    T data;
    std::memcpy(&data, array.data() + addr, sizeof(T));

    if constexpr(std::endian::native == std::endian::big)
    {
        return std::byteswap(data);
    }

    return data;
}

constexpr u8 OBJ_SIZE_X[4][4] =
{
    { 8,  16, 32, 64 },
    { 16, 32, 32, 64 },
    { 8,  8,  16, 32 },
    { /* invalid */ }
};

constexpr u8 OBJ_SIZE_Y[4][4] =
{
    { 8,  16, 32, 64 },
    { 8,  8,  16, 32 },
    { 16, 32, 32, 64 },
    { /* invalid */ }
};

enum BlockSize
{
    CHARBLOCK_SIZE = 0x4000,
    SCREENBLOCK_SIZE = 0x800,
};

enum ObjBpp
{
    BG_4BPP = 0,
    BG_8BPP = 1,
};

enum LayerNumber
{
    BG0_NUM = 0,
    BG1_NUM = 1,
    BG2_NUM = 2,
    BG3_NUM = 3,
    OBJ_NUM = 4,
    BACKDROP_NUM = 5,
};

enum LayerPriority
{
    PRIORITY_0 = 0,
    PRIORITY_1 = 1,
    PRIORITY_2 = 2,
    PRIORITY_3 = 3,
    PRIORITY_BACKDROP = 4,
};

enum WindowPriority
{
    WIN0_PRIORITY = 0,
    WIN1_PRIORITY = 1,
    WIN_OBJ_PRIORITY = 2,
};

enum ObjMode
{
    Normal, // normal object
    Affine, // affine object
    Hide, // object is hidden (not rendered)
    Affine2X, // affine using double rendering area
};

enum class Blend
{
    None, // no blending
    Alpha, // blend 2 layers
    White, // fade to white
    Black, // fade to black
};

enum Layer : u8
{
    BG0 = 1 << 0,
    BG1 = 1 << 1,
    BG2 = 1 << 2,
    BG3 = 1 << 3,
    OBJ = 1 << 4,

    ALL = BG0 | BG1 | BG2 | BG3 | OBJ,
};

struct ObjLine
{
    ObjLine()
    {
        std::memset(priority, 0xFF, sizeof(priority));
    }

    u16 pixels[240];
    u8 priority[240];
    bool is_alpha[240]{false};
    bool is_win[240]{false};
    bool is_opaque[240]{false}; // pixel != 0
};

enum class RenderType
{
    Reg,
    Affine,
    Bitmap3,
    Bitmap4,
    Bitmap5,
};

struct BgLine
{
    BgLine(const u8 _num, const RenderType type) :
        num{_num},
        render_type{type} {}

    const u8 num; // background num (0-3)
    const RenderType render_type;

    u16 pixels[240];
    bool is_opaque[240]{false}; // pixel != 0
    u8 priority; // priority (0-3)
};

struct BGxCNT
{
    constexpr BGxCNT(const u16 cnt) :
        Pr{bit::get_range<0, 1>(cnt)},
        CBB{bit::get_range<2, 3>(cnt)},
        Mos{bit::is_set<6>(cnt)},
        CM{bit::is_set<7>(cnt)},
        SBB{bit::get_range<8, 12>(cnt)},
        Wr{bit::is_set<13>(cnt)},
        Sz{bit::get_range<14, 15>(cnt)} {}

    u16 Pr : 2; // Priority. Determines drawing order of backgrounds.
    u16 CBB : 2; // Character Base Block. Sets the charblock that serves as the base for character/tile indexing. Values: 0-3.
    u16 Mos : 1; // Mosaic flag. Enables mosaic effect.
    u16 CM : 1; // Color Mode. 16 colors (4bpp) if cleared; 256 colors (8bpp) if set.
    u16 SBB : 5; // Screen Base Block. Sets the screenblock that serves as the base for screen-entry/map indexing. Values: 0-31.
    u16 Wr : 1; // Affine Wrapping flag. If set, affine background wrap around at their edges. Has no effect on regular backgrounds as they wrap around by default.
    u16 Sz : 2; // Background Size. Regular and affine backgrounds have different sizes available to them.
};

// contains control and scroll values
struct BgMeta
{
    u16 xscroll;
    u16 yscroll;
};

struct BgAffineMeta
{
    s16 pa, pb, pc, pd;
    s32 dx, dy;
};

// todo: optimise the WIN IN/OUT arrays into bitfield
// this will remove the loops in the window build function
struct WININ
{
    constexpr WININ(const auto v) :
        bg0{bit::is_set<0>(v)},
        bg1{bit::is_set<1>(v)},
        bg2{bit::is_set<2>(v)},
        bg3{bit::is_set<3>(v)},
        obj{bit::is_set<4>(v)},
        blend{bit::is_set<5>(v)},
        in{bg0, bg1, bg2, bg3, obj, blend} {}

private:
    const bool bg0 : 1; // BG0 in winX
    const bool bg1 : 1; // BG1 in winX
    const bool bg2 : 1; // BG2 in winX
    const bool bg3 : 1; // BG3 in winX
    const bool obj : 1; // Sprites in winX
    const bool blend : 1; // Blends in winX

public:
    bool in[6];
};

struct WINOUT
{
    constexpr WINOUT(const u16 v) :
        bg0_out{bit::is_set<0>(v)},
        bg1_out{bit::is_set<1>(v)},
        bg2_out{bit::is_set<2>(v)},
        bg3_out{bit::is_set<3>(v)},
        obj_out{bit::is_set<4>(v)},
        blend_out{bit::is_set<5>(v)},
        bg0_obj_win{bit::is_set<8>(v)},
        bg1_obj_win{bit::is_set<9>(v)},
        bg2_obj_win{bit::is_set<10>(v)},
        bg3_obj_win{bit::is_set<11>(v)},
        obj_obj_win{bit::is_set<12>(v)},
        blend_obj_win{bit::is_set<13>(v)},
        out{bg0_out, bg1_out, bg2_out, bg3_out, obj_out, blend_out},
        obj{bg0_obj_win, bg1_obj_win, bg2_obj_win, bg3_obj_win, obj_obj_win, blend_obj_win} {}

private:
    const bool bg0_out : 1;
    const bool bg1_out : 1;
    const bool bg2_out : 1;
    const bool bg3_out : 1;
    const bool obj_out : 1;
    const bool blend_out : 1;

    const bool bg0_obj_win : 1;
    const bool bg1_obj_win : 1;
    const bool bg2_obj_win : 1;
    const bool bg3_obj_win : 1;
    const bool obj_obj_win : 1;
    const bool blend_obj_win : 1;

public:
    bool out[6];
    bool obj[6];
};

struct WindowBounds
{
    WindowBounds()
    {
        std::memset(inside, true, sizeof(inside));
        std::memset(priority, 0xFF, sizeof(priority));
    }

    // builds the obj window, call this rendering obj (and bg)
    auto build(Gba& gba) -> void;

    // call this after rendering the obj
    // this will apply any obj windowing (if any)
    auto apply_obj_window(Gba& gba, const ObjLine& obj_line) -> void;

    // returns true if the pixel can be drawn
    [[nodiscard]] auto in_bounds(const auto bg_num, const auto x) const { return inside[bg_num][x]; }
    // returns true if this pixel can blend
    [[nodiscard]] auto can_blend(const auto x) const { return in_bounds(5, x); }

private:
    // bg0, bg1, bg2, bg3, obj, blend
    bool inside[6][240];
    bool in_range[240]{false};
    u8 priority[240];
};

struct BLDMOD
{
    constexpr BLDMOD(const u16 v) :
        bg0_src{bit::is_set<0>(v)},
        bg1_src{bit::is_set<1>(v)},
        bg2_src{bit::is_set<2>(v)},
        bg3_src{bit::is_set<3>(v)},
        obj_src{bit::is_set<4>(v)},
        backdrop_src{bit::is_set<5>(v)},
        mode{bit::get_range<6, 7>(v)},
        bg0_dst{bit::is_set<8>(v)},
        bg1_dst{bit::is_set<9>(v)},
        bg2_dst{bit::is_set<10>(v)},
        bg3_dst{bit::is_set<11>(v)},
        obj_dst{bit::is_set<12>(v)},
        backdrop_dst{bit::is_set<13>(v)},
        src{bg0_src, bg1_src, bg2_src, bg3_src, obj_src, backdrop_src},
        dst{bg0_dst, bg1_dst, bg2_dst, bg3_dst, obj_dst, backdrop_dst} {}

    // get blend mode (does not apply to top layer alpha obj)
    [[nodiscard]] constexpr auto get_mode() const
    {
        return static_cast<Blend>(mode);
    }

    // check if can blend (does not apply to top layer alpha obj)
    [[nodiscard]] constexpr auto is_alpha(const u8 top_num, const u8 bottom_num = 0) const
    {
        switch (this->get_mode())
        {
            case Blend::None: return false;
            case Blend::Alpha: return src[top_num] && dst[bottom_num];
            case Blend::White: return src[top_num];
            case Blend::Black: return src[top_num];
        }

        std::unreachable();
    }

    // returns true if the bottom layer enabled for bledning
    [[nodiscard]] constexpr auto is_alpha_bottom(const u8 bottom_num) const
    {
        return dst[bottom_num];
    }

private:
    const bool bg0_src : 1; // Blend BG0 (source)
    const bool bg1_src : 1; // Blend Bg1 (source)
    const bool bg2_src : 1; // Blend BG2 (source)
    const bool bg3_src : 1; // Blend BG3 (source)
    const bool obj_src : 1; // Blend sprites (source)
    const bool backdrop_src : 1; // Blend backdrop (source)

    const u16 mode : 2; // Blend Mode

    const bool bg0_dst : 1; // Blend BG0 (target)
    const bool bg1_dst : 1; // Blend BG1 (target)
    const bool bg2_dst : 1; // Blend BG2 (target)
    const bool bg3_dst : 1; // Blend BG3 (target)
    const bool obj_dst : 1; // Blend sprites (target)
    const bool backdrop_dst : 1; // Blend backdrop (target)

public:
    bool src[6];
    bool dst[6];
};

// NOTE: The affine screen entries are only 8 bits wide and just contain an 8bit tile index.
struct ScreenEntry
{
    constexpr ScreenEntry(const u16 screen_entry) :
        tile_index{bit::get_range<0, 9>(screen_entry)},
        hflip{bit::is_set<10>(screen_entry)},
        vflip{bit::is_set<11>(screen_entry)},
        palette_bank{bit::get_range<12, 15>(screen_entry)} {}

    const u16 tile_index : 10; // max 512
    const u16 hflip : 1;
    const u16 vflip : 1;
    const u16 palette_bank : 4;
};

struct Attr0
{
    constexpr Attr0(const u16 v) :
        Y{bit::get_range<0, 7>(v)},
        OM{bit::get_range<8, 9>(v)},
        GM{bit::get_range<10, 11>(v)},
        Mos{bit::is_set<12>(v)},
        CM{bit::is_set<13>(v)},
        Sh{bit::get_range<14, 15>(v)} {}

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
    constexpr Attr1(const u16 v) :
        X{bit::get_range<0, 8, s16>(v)},
        AID{bit::get_range<9, 13>(v)},
        HF{bit::is_set<12>(v)},
        VF{bit::is_set<13>(v)},
        Sz{bit::get_range<14, 15>(v)} {}

    const s16 X : 9; // X coordinate. Marks left of the sprite.
    const u16 AID : 5; // Affine index. Specifies the OAM_AFF_ENTY this sprite uses. Valid only if the affine flag (attr0{8}) is set.
    const u16 HF : 1; // Horizontal flipping flag. Used only if the affine flag (attr0) is clear; otherwise they're part of the affine index.
    const u16 VF : 1; // vertical flipping flag. Used only if the affine flag (attr0) is clear; otherwise they're part of the affine index.
    const u16 Sz : 2; // Sprite size. Kinda. Together with the shape bits (attr0{E-F}) these determine the sprite's real size.
};

struct Attr2
{
    constexpr Attr2(const u16 v) :
        TID{bit::get_range<0, 9, u16>(v)},
        Pr{bit::get_range<10, 11>(v)},
        PB{bit::get_range<12, 15>(v)}{}

    const u16 TID : 10; // Base tile-index of sprite. Note that in bitmap modes this must be 512 or higher.
    const u16 Pr : 2; // Priority. Higher priorities are drawn first (and therefore can be covered by later sprites and backgrounds). Sprites cover backgrounds of the same priority, and for sprites of the same priority, the higher OBJ_ATTRs are drawn first.
    const u16 PB : 4; // Palette-bank to use when in 16-color mode. Has no effect if the color mode flag (attr0{C}) is set.
};

struct OBJ_Attr
{
    const Attr0 attr0; // The first attribute controls a great deal, but the most important parts are for the y coordinate, and the shape of the sprite. Also important are whether or not the sprite is transformable (an affine sprite), and whether the tiles are considered to have a bitdepth of 4 (16 colors, 16 sub-palettes) or 8 (256 colors / 1 palette).
    const Attr1 attr1; // The primary parts of this attribute are the x coordinate and the size of the sprite. The role of bits 8 to 14 depend on whether or not this is a affine sprite (determined by attr0{8}). If it is, these bits specify which of the 32 OBJ_AFFINEs should be used. If not, they hold flipping flags.
    const Attr2 attr2; // This attribute tells the GBA which tiles to display and its background priority. If it's a 4bpp sprite, this is also the place to say what sub-palette should be used.

    [[nodiscard]] constexpr auto is_yflip() const
    {
        // flipping is only used in normal mode
        if (attr0.OM == 0b00)
        {
            return static_cast<bool>(attr1.VF);
        }
        return false;
    }

    [[nodiscard]] constexpr auto is_xflip() const
    {
        // flipping is only used in normal mode
        if (attr0.OM == 0b00)
        {
            return static_cast<bool>(attr1.HF);
        }
        return false;
    }
};

struct OBJ_Affine
{
    s16 pa; // dx
    s16 pb; // dmx
    s16 pc; // dy
    s16 pd; // dmy
};

constexpr auto get_bg_affine_xy(const BGxCNT& cnt, const BgAffineMeta& meta, u32 x, u32 width, u32 height) -> std::pair<u32, u32>
{
    u32 tx = (meta.dx + meta.pa * x) >> 8;
    u32 ty = (meta.dy + meta.pc * x) >> 8;

    // apply wrapping if enabled
    if (cnt.Wr)
    {
        tx %= width;
        ty %= height;
    }

    return { tx, ty };
}

// psudo-ish code for affine
auto render_obj_affine(Gba& gba, const OBJ_Attr& obj, const WindowBounds& bounds, ObjLine& line) -> void
{
    // if affine2X, then scale size by 2
    const auto scale_size = obj.attr0.OM == Affine2X ? 2 : 1;

    // affine data is interleaved in oam along with attr data.
    // eg, [a0,a1,a2,pa], [a0,a1,a2,pb], [a0,a1,a2,pc], [a0,a1,a2,pd]
    // indexing starts from oam[0x6]
    // every affine block occupies 32 bytes of space
    // to to index, affine = oam[0x6 + (AID * 32)]
    const auto affine_index = 0x6 + (obj.attr1.AID * 32);

    const OBJ_Affine affine{
        .pa = read_array_no_mask<s16>(gba.mem.oam, affine_index + 0),
        .pb = read_array_no_mask<s16>(gba.mem.oam, affine_index + 8),
        .pc = read_array_no_mask<s16>(gba.mem.oam, affine_index + 16),
        .pd = read_array_no_mask<s16>(gba.mem.oam, affine_index + 24),
    };

    const auto xSize = scale_size * OBJ_SIZE_X[obj.attr0.Sh][obj.attr1.Sz];
    const auto ySize = scale_size * OBJ_SIZE_Y[obj.attr0.Sh][obj.attr1.Sz];

    // NOTE: not sure what to do from here...
}

enum Index : bool
{
    X = false,
    Y = true
};

template<Index index> [[nodiscard]] // 1=Y, 0=X
auto get_bg_offset(const BGxCNT cnt, const auto x) -> u16
{
    constexpr auto PIXEL_MAP_SIZE = 255;

    static constexpr u16 mod[2][4] = // [1] = Y, [1] = X
    {
        { 256, 512, 256, 512 },
        { 256, 256, 512, 512 },
    };

    static constexpr u16 result[2][4] = // [1] = Y, [0] = X
    {
        { 0x800, 0x800, 0x800, 0x800 },
        { 0x800, 0x800, 0x800, 0x800*2 }, // Y=1, Sz=3: this is a 2d array, so if we are on the next slot, then we need to move down 2*SCREENBLOCK_SIZE
    };

    // move to the next block if out of range
    const auto next_block = (x % mod[index][cnt.Sz]) > PIXEL_MAP_SIZE;

    return result[index][cnt.Sz] * next_block;
}

struct Bgr
{
    constexpr Bgr() = default;

    constexpr Bgr(const u16 col) :
        r{static_cast<u8>(bit::get_range<0, 4>(col))},
        g{static_cast<u8>(bit::get_range<5, 9>(col))},
        b{static_cast<u8>(bit::get_range<10, 14>(col))} {}

    [[nodiscard]] constexpr auto pack() const -> u16
    {
        return (std::min<u8>(31, b) << 10) | (std::min<u8>(31, g) << 5) | (std::min<u8>(31, r));
    }

    u8 r, g, b;
};

// the blending *formular* took way longer than it should've
// NOTE: don't try to apply the coeff directly to the bgr555 colour, it won't work.
// have to apply it to each b,g,r value.
// masking the b,g,r is handled by the bitfield.
constexpr auto blend_alpha(const u16 src, const u16 dst, const u8 coeff_src, const u8 coeff_dst) -> u16
{
    assert(coeff_src <= 16);
    assert(coeff_dst <= 16);

    const Bgr src_col{src};
    const Bgr dst_col{dst};
    Bgr fin_col;

    fin_col.r = ((src_col.r * coeff_src) + (dst_col.r * coeff_dst)) / 16;
    fin_col.g = ((src_col.g * coeff_src) + (dst_col.g * coeff_dst)) / 16;
    fin_col.b = ((src_col.b * coeff_src) + (dst_col.b * coeff_dst)) / 16;

    return fin_col.pack();
}

constexpr auto blend_white(const u16 col, const u8 coeff) -> u16
{
    assert(coeff <= 16);

    const Bgr src_col{col};
    Bgr fin_col;

    // eg (((31 - 0) * 16) / 16) = max white
    // eg (((31 - 0) * 32) / 16) = max white (masked by bitfield)
    fin_col.r = src_col.r + (((31 - src_col.r) * coeff) / 16);
    fin_col.g = src_col.g + (((31 - src_col.g) * coeff) / 16);
    fin_col.b = src_col.b + (((31 - src_col.b) * coeff) / 16);
    return fin_col.pack();
}

constexpr auto blend_black(const u16 col, const u8 coeff) -> u16
{
    assert(coeff <= 16);

    const Bgr src_col{col};
    Bgr fin_col;

    // eg (16 - ((16 * 0) / 16)) = max black
    fin_col.r = src_col.r - ((src_col.r * coeff) / 16);
    fin_col.g = src_col.g - ((src_col.g * coeff) / 16);
    fin_col.b = src_col.b - ((src_col.b * coeff) / 16);
    return fin_col.pack();
}

auto get_backdrop_colour(const Gba& gba) -> u16
{
    return read_array_no_mask<u16>(gba.mem.pram, 0);
}

auto parse_obj(Gba& gba, const WindowBounds& bounds, ObjLine& line) -> void
{
    // ovram is the last 2 entries of the charblock in vram.
    // in tile modes, this allows for 1024 tiles in total.
    // in bitmap modes, this allows for 512 tiles in total.
    const auto ovram = std::span{gba.mem.vram}.subspan(4 * CHARBLOCK_SIZE);
    const auto oam = std::span{gba.mem.oam};
    const auto vcount = REG_VCOUNT;
    const auto is_1D_layout = bit::is_set<6>(REG_DISPCNT);

    // 1024 entries in oam, each entry is 64bytes, 1024/64=128
    for (auto i = 0; i < 128; i++)
    {
        const OBJ_Attr obj{
            .attr0 = read_array_no_mask<u16>(oam, (i * 8) + 0),
            .attr1 = read_array_no_mask<u16>(oam, (i * 8) + 2),
            .attr2 = read_array_no_mask<u16>(oam, (i * 8) + 4),
        };

        // todo: split the rendering process here
        // in affine case, call render_obj_affine()
        // in normal case, call render_obj_normal()
        switch (obj.attr0.OM)
        {
            case ObjMode::Normal:
                break;

            case ObjMode::Affine:
            case ObjMode::Affine2X:
                #if 0
                render_obj_affine(gba, obj, bounds, line);
                continue;
                #else
                break;
                #endif

            case ObjMode::Hide:
                continue;
        }

        // fetch the x/y size of the sprite
        const auto width = OBJ_SIZE_X[obj.attr0.Sh][obj.attr1.Sz];
        const auto height = OBJ_SIZE_Y[obj.attr0.Sh][obj.attr1.Sz];
        // see here for wrapping: https://www.coranac.com/tonc/text/affobj.htm#ssec-wrap
        const auto sprite_y = obj.attr0.Y + height > 256 ? obj.attr0.Y - 256 : obj.attr0.Y;
        // calculate the y_index, handling flipping
        const auto mosY = obj.is_yflip() ? (height - 1) - (vcount - sprite_y) : vcount - sprite_y;
        // fine_y
        const auto yMod = mosY % 8;
        // for which row the obj will be on (based on layout)
        const auto row_mult = is_1D_layout ? width / 8 : 32;

        // check if the sprite is to be drawn on this ;ine
        if (vcount >= sprite_y && vcount < sprite_y + height)
        {
            // for each pixel of the sprite
            for (auto x = 0; x < width; x++)
            {
                // this is the index into the pixel array
                const auto pixel_x = obj.attr1.X + x;

                // check its in bounds
                if (pixel_x < 0 || pixel_x >= 240)
                {
                    continue;
                }

                // check if we are allowed inside
                // NOTE: cowbite states that win0/1 has higher prio than obj
                // window, i need to test this!
                if (!bounds.in_bounds(OBJ_NUM, pixel_x))
                {
                    continue;
                }

                // skip obj already rendered over higher or equal prio
                if (line.priority[pixel_x] <= obj.attr2.Pr)
                {
                    continue;
                }

                // x_index, handling flipping
                const auto mosX = obj.is_xflip() ? width - 1 - x : x;
                // fine_x
                const auto xMod = mosX % 8;

                auto pram_addr = 0;
                auto pixel = 0;

                if (obj.attr0.is_4bpp())
                {
                    // thank you Kellen for the below code, i wouldn't have firgured it out otherwise.
                    auto charblock_addr = obj.attr2.TID * 32;
                    charblock_addr += (((mosY / 8 * row_mult)) + mosX / 8) * 32;
                    charblock_addr += yMod * 4;
                    charblock_addr += xMod / 2;

                    // if the adddress is out of bounds, break early
                    if (charblock_addr >= CHARBLOCK_SIZE * 2) [[unlikely]]
                    {
                        break;
                    }

                    // in bitmap mode, only the last charblock can be used for sprites.
                    if (is_bitmap_mode(gba) && charblock_addr < CHARBLOCK_SIZE) [[unlikely]]
                    {
                        continue;
                    }

                    pixel = ovram[charblock_addr];

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
                    auto charblock_addr = obj.attr2.TID * 32;
                    charblock_addr += (((mosY / 8 * row_mult)) + mosX / 8) * 64;
                    charblock_addr += yMod * 8;
                    charblock_addr += xMod;

                    // if the adddress is out of bounds, break early
                    if (charblock_addr >= CHARBLOCK_SIZE * 2) [[unlikely]]
                    {
                        break;
                    }

                    // in bitmap mode, only the last charblock can be used for sprites.
                    if (is_bitmap_mode(gba) && charblock_addr < CHARBLOCK_SIZE) [[unlikely]]
                    {
                        continue;
                    }

                    pixel = ovram[charblock_addr];
                    pram_addr = pixel * 2;
                }

                // don't render transparent pixels
                if (pixel != 0)
                {
                    // this is object window
                    if (obj.attr0.GM == 0b10) [[unlikely]]
                    {
                        line.is_win[pixel_x] = obj.attr0.GM == 0b10;
                    }
                    else
                    {
                        line.is_opaque[pixel_x] = true;
                        line.priority[pixel_x] = obj.attr2.Pr;
                        line.is_alpha[pixel_x] = obj.attr0.GM == 0b01;
                        line.pixels[pixel_x] = pram_addr;
                    }
                }
            }
        }
    }
}

auto render_obj(Gba& gba, const WindowBounds& bounds, ObjLine& line) -> void
{
    const auto pram = std::span{gba.mem.pram}.subspan(512);

    // 1024 entries in oam, each entry is 64bytes, 1024/64=128
    for (auto x = 0; x < 240; x++)
    {
        // don't render transparent pixels
        if (line.is_opaque[x])
        {
            // checks if clipped by object window
            if (!bounds.in_bounds(OBJ_NUM, x))
            {
                line.is_opaque[x] = false;
            }
            else
            {
                const auto pram_addr = line.pixels[x];
                line.pixels[x] = read_array_no_mask<u16>(pram, pram_addr);
            }
        }
    }
}

auto render_tile_line_bg(Gba& gba, BgLine& line, const WindowBounds& bounds, const BGxCNT cnt, const BgMeta meta)
{
    const auto charblock_offset = cnt.CBB * CHARBLOCK_SIZE;
    const auto charblock_size = 4 * CHARBLOCK_SIZE - charblock_offset;

    const auto vcount = REG_VCOUNT;
    //
    const auto y = (meta.yscroll + vcount) % 256;
    // pal_mem
    const auto pram = std::span{gba.mem.pram};
    // tile_mem (where the tiles/tilesets are)
    const auto charblock = std::span{gba.mem.vram}.subspan(charblock_offset);
    // se_mem (where the tilemaps are)
    const auto screenblock = std::span{gba.mem.vram}.subspan((cnt.SBB * SCREENBLOCK_SIZE) + get_bg_offset<Index::Y>(cnt, meta.yscroll + vcount) + ((y / 8) * 64));

    for (auto x = 0; x < 240; x++)
    {
        // check if we are allowed inside
        if (!bounds.in_bounds(line.num, x))
        {
            continue;
        }

        const auto tx = (x + meta.xscroll) % 256;
        const auto se_number = (tx / 8) + (get_bg_offset<Index::X>(cnt, x + meta.xscroll) / 2); // SE-number n = tx+ty·tw,
        const ScreenEntry se = read_array_no_mask<u16>(screenblock, se_number * 2);

        const auto tile_x = se.hflip ? 7 - (tx & 7) : tx & 7;
        const auto tile_y = se.vflip ? 7 - (y & 7) : y & 7;

        const auto tile_offset = tile_x + (tile_y * 8);

        auto pram_addr = 0;
        auto pixel = 0;

        if (cnt.CM == BG_4BPP)
        {
            const auto charblock_addr = (se.tile_index * 32) + tile_offset / 2;
            // don't allow access to blocks 4,5
            if (charblock_addr >= charblock_size) [[unlikely]]
            {
                continue;
            }

            pixel = charblock[charblock_addr];

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
            const auto charblock_addr = (se.tile_index * 64) + tile_offset;
            // don't allow access to blocks 4,5
            if (charblock_addr >= charblock_size) [[unlikely]]
            {
                continue;
            }

            pixel = charblock[charblock_addr];
            pram_addr = 2 * pixel;
        }

        if (pixel != 0) // don't render transparent pixel
        {
            line.is_opaque[x] = true;
            line.pixels[x] = read_array_no_mask<u16>(pram, pram_addr);
        }
    }
}

// affine bg is 1D layout, aka, 8bpp
auto render_affine_line_bg(Gba& gba, BgLine& line, const WindowBounds& bounds, const BGxCNT cnt, const BgAffineMeta meta)
{
    // width/height sizes are the same
    constexpr u16 sizes[4] = { 128, 256, 512, 1024 };

    const auto width = sizes[cnt.Sz];
    const auto height = sizes[cnt.Sz];
    const auto tw = width / 8;

    const auto charblock_offset = cnt.CBB * CHARBLOCK_SIZE;
    const auto charblock_size = 4 * CHARBLOCK_SIZE - charblock_offset;

    // pal_mem
    const auto pram = std::span{gba.mem.pram};
    // tile_mem (where the tiles/tilesets are)
    const auto charblock = std::span{gba.mem.vram}.subspan(cnt.CBB * CHARBLOCK_SIZE);
    // se_mem (where the tilemaps are)
    const auto screenblock = std::span{gba.mem.vram}.subspan((cnt.SBB * SCREENBLOCK_SIZE));

    for (auto x = 0; x < 240; x++)
    {
        // check if we are allowed inside
        if (!bounds.in_bounds(line.num, x))
        {
            continue;
        }

        const auto [tx, ty] = get_bg_affine_xy(cnt, meta, x, width, height);

        if (tx >= width || ty >= height)
        {
            continue;
        }

        const auto se_number = (tx / 8) + (ty / 8) * tw; // SE-number n = tx+ty·tw,

        const auto tile_index = read_array_no_mask<u8>(screenblock, se_number);

        const auto tile_x = tx & 7;
        const auto tile_y = ty & 7;

        const auto tile_offset = tile_x + (tile_y * 8);
        const auto charblock_addr = (tile_index * 64) + tile_offset;

        // don't allow access to blocks 4,5
        if (charblock_addr >= charblock_size) [[unlikely]]
        {
            continue;
        }

        const auto pixel = charblock[(tile_index * 64) + tile_offset];
        const auto pram_addr = 2 * pixel;

        if (pixel != 0) // don't render transparent pixel
        {
            line.is_opaque[x] = true;
            line.pixels[x] = read_array_no_mask<u16>(pram, pram_addr);
        }
    }
}

auto render_bitmap3_line_bg(Gba& gba, BgLine& line, const WindowBounds& bounds, const BGxCNT cnt, const BgAffineMeta meta)
{
    constexpr auto width = 240;
    constexpr auto height = 160;
    const auto vram = std::span{gba.mem.vram};

    for (auto x = 0; x < width; x++)
    {
        // check if we are allowed inside
        if (!bounds.in_bounds(line.num, x))
        {
            continue;
        }

        const auto [tx, ty] = get_bg_affine_xy(cnt, meta, x, width, height);

        if (tx >= width || ty >= height)
        {
            continue;
        }

        const auto pixel = read_array_no_mask<u16>(vram, (ty * width * 2) + (tx * 2));
        line.is_opaque[x] = true;
        line.pixels[x] = pixel;
    }
}

auto render_bitmap4_line_bg(Gba& gba, BgLine& line, const WindowBounds& bounds, const BGxCNT cnt, const BgAffineMeta meta)
{
    constexpr auto width = 240;
    constexpr auto height = 160;
    const auto page = bit::is_set<4>(REG_DISPCNT) ? 0xA000 : 0;
    const auto pram = std::span{gba.mem.pram};
    const auto vram = std::span{gba.mem.vram}.subspan(page);

    for (auto x = 0; x < width; x++)
    {
        // check if we are allowed inside
        if (!bounds.in_bounds(line.num, x))
        {
            continue;
        }

        const auto [tx, ty] = get_bg_affine_xy(cnt, meta, x, width, height);

        if (tx >= width || ty >= height)
        {
            continue;
        }

        const auto pixel = vram[(width * ty) + tx];

        if (pixel != 0) // don't render transparent pixel
        {
            line.is_opaque[x] = true;
            line.pixels[x] = read_array_no_mask<u16>(pram, pixel * 2);
        }
    }
}

auto render_bitmap5_line_bg(Gba& gba, BgLine& line, const WindowBounds& bounds, const BGxCNT cnt, const BgAffineMeta meta)
{
    constexpr auto width = 160;
    constexpr auto height = 128;
    const auto page = bit::is_set<4>(REG_DISPCNT) ? 0xA000 : 0;
    const auto vram = std::span{gba.mem.vram}.subspan(page);

    for (auto x = 0; x < width; x++)
    {
        // check if we are allowed inside
        if (!bounds.in_bounds(line.num, x))
        {
            continue;
        }

        const auto [tx, ty] = get_bg_affine_xy(cnt, meta, x, width, height);

        if (tx >= width || ty >= height)
        {
            continue;
        }

        const auto pixel = read_array_no_mask<u16>(vram, (ty * width * 2) + (tx * 2));
        line.is_opaque[x] = true;
        line.pixels[x] = pixel;
    }
}

auto is_obj_enabled(Gba& gba)
{
    return bit::is_set<12>(REG_DISPCNT);
}

auto WindowBounds::build(Gba& gba) -> void
{
    const auto win0_enabled = bit::is_set<13>(REG_DISPCNT);
    const auto win1_enabled = bit::is_set<14>(REG_DISPCNT);

    // exit early if neither windows are enabled
    if (!win0_enabled && !win1_enabled)
    {
        return;
    }

    const WININ win0_in{ REG_WININ };
    const WININ win1_in{ REG_WININ >> 8 };
    const WINOUT win_out{REG_WINOUT};

    const auto win0_x_start = bit::get_range<8, 15>(REG_WIN0H);
    const auto win0_x_end = bit::get_range<0, 7>(REG_WIN0H);
    const auto win0_y_start = bit::get_range<8, 15>(REG_WIN0V);
    const auto win0_y_end = bit::get_range<0, 7>(REG_WIN0V);

    const auto win1_x_start = bit::get_range<8, 15>(REG_WIN1H);
    const auto win1_x_end = bit::get_range<0, 7>(REG_WIN1H);
    const auto win1_y_start = bit::get_range<8, 15>(REG_WIN1V);
    const auto win1_y_end = bit::get_range<0, 7>(REG_WIN1V);

    if (win0_enabled)
    {
        assert(win0_x_start <= win0_x_end && "wrapping not handled");
        assert(win1_x_start <= win1_x_end && "wrapping not handled");
    }
    if (win1_enabled)
    {
        assert(win0_y_start <= win0_y_end && "wrapping not handled");
        assert(win1_y_start <= win1_y_end && "wrapping not handled");
    }

    const auto vcount = REG_VCOUNT;

    const auto win0_in_range_y = vcount >= win0_y_start && vcount < win0_y_end;
    const auto win1_in_range_y = vcount >= win1_y_start && vcount < win1_y_end;

    // todo: optimise this into 2 loops...
    for (auto x = 0; x < 240; x++)
    {
        if (win0_enabled)
        {
            const auto win0_in_range = win0_in_range_y && x >= win0_x_start && x < win0_x_end;

            for (auto i = 0; i < 6; i++)
            {
                if (win0_in_range)
                {
                    this->inside[i][x] = win0_in.in[i];
                    this->in_range[x] = true;
                    this->priority[x] = WIN0_PRIORITY;
                }
                else
                {
                    this->inside[i][x] = !this->in_range[x] && win_out.out[i];
                }
            }
        }

        if (win1_enabled && this->priority[x] == 0xFF)
        {
            const auto win1_in_range = win1_in_range_y && x >= win1_x_start && x < win1_x_end;

            for (auto i = 0; i < 6; i++)
            {
                if (win1_in_range)
                {
                    this->inside[i][x] = win1_in.in[i];
                    this->in_range[x] = true;
                    this->priority[x] = WIN1_PRIORITY;
                }
                else
                {
                    this->inside[i][x] = !this->in_range[x] && win_out.out[i];
                }
            }
        }
    }
}

auto WindowBounds::apply_obj_window(Gba& gba, const ObjLine& obj_line) -> void
{
    const auto win_obj_enabled = bit::is_set<15>(REG_DISPCNT);
    const WINOUT win_out{REG_WINOUT};

    // exit early if obj window is not enabled
    if (!win_obj_enabled)
    {
        return;
    }

    for (auto x = 0; x < 240; x++)
    {
        if (win_obj_enabled && this->priority[x] == 0xFF)
        {
            for (auto i = 0; i < 6; i++)
            {
                if (obj_line.is_win[x])
                {
                    this->inside[i][x] = win_out.obj[i];
                    this->in_range[x] = true;
                    this->priority[x] = WIN_OBJ_PRIORITY;
                }
                else
                {
                    this->inside[i][x] = !this->in_range[x] && win_out.out[i];
                }
            }
        }
    }
}

auto merge(Gba& gba, const WindowBounds& bounds, std::span<u16> pixels, std::span<const BgLine> bg_lines, const ObjLine& obj_line) -> void
{
    struct Layers
    {
        Layers(u16 backdrop_colour)
        {
            for (auto& p : pixel)
            {
                p = backdrop_colour;
            }

            // std::memset(pixel, backdrop_colour, sizeof(pixel));
            std::memset(prio, PRIORITY_BACKDROP, sizeof(prio));
            std::memset(num, BACKDROP_NUM, sizeof(num));
        }

        constexpr auto add(u16 new_pixel, u8 new_prio, u8 new_num, bool new_is_alpha)
        {
            // if the new pixel has a lower priority than the current
            // top layer pixel, overwrite it.
            if (new_prio < prio[0])
            {
                // save previous top layer as bottom layer
                pixel[1] = pixel[0];
                prio[1] = prio[0];
                num[1] = num[0];

                pixel[0] = new_pixel;
                prio[0] = new_prio;
                num[0] = new_num;
                is_alpha = new_is_alpha;
            }
            // otherwise, check if new pixel has a lower prio
            // than the current bottom layer.
            else if (new_prio < prio[1])
            {
                pixel[1] = new_pixel;
                prio[1] = new_prio;
                num[1] = new_num;
            }
        }

        [[nodiscard]] auto get_pixel() const { return pixel[0]; }
        [[nodiscard]] auto get_prio() const { return prio[0]; }
        [[nodiscard]] auto get_num() const { return num[0]; }
        [[nodiscard]] auto is_obj_alpha() const { return is_alpha; }

        u16 pixel[2];
        u8 prio[2];
        u8 num[2];
        bool is_alpha{}; // only useful for obj
    };

    const auto backdrop_colour = get_backdrop_colour(gba);
    const auto obj_enabled = is_obj_enabled(gba);

    const auto bldmod = BLDMOD{REG_BLDMOD};
    const auto blend_mode = bldmod.get_mode();

    const auto coeff_src = std::min<u8>(16, bit::get_range<0, 4>(REG_COLEV));
    const auto coeff_dst = std::min<u8>(16, bit::get_range<8, 12>(REG_COLEV));
    const auto coeff_wb = std::min<u8>(16, bit::get_range<0, 4>(REG_COLEY));

    for (auto x = 0; x < 240; x++)
    {
        Layers layers{backdrop_colour};

        if (obj_enabled)
        {
            if (obj_line.is_opaque[x]) // is the pixel visible
            {
                layers.add(
                    obj_line.pixels[x],
                    obj_line.priority[x],
                    OBJ_NUM,
                    obj_line.is_alpha[x]
                );
            }
        }

        for (auto& bg_line : bg_lines)
        {
            if (bg_line.is_opaque[x])
            {
                layers.add(
                    bg_line.pixels[x], // pixel
                    bg_line.priority, // priority
                    bg_line.num, // bg_num
                    false // is_alpha (always false for bg)
                );
            }
        }

        // if obj has alpha bit set, it always does alpha blend as long
        // as the bottom layer (dst) is enabled in bldmod
        if (layers.is_obj_alpha())
        {
            if (bldmod.is_alpha_bottom(layers.num[1]))
            {
                layers.pixel[0] = blend_alpha(layers.pixel[0], layers.pixel[1], coeff_src, coeff_dst);
            }
        }
        // check if we can blend within or outside the window and if the top
        // layer is a src and bottom layer is a dst of bldmod.
        else if (bounds.can_blend(x))
        {
            if (bldmod.is_alpha(layers.num[0], layers.num[1]))
            {
                switch (blend_mode)
                {
                    case Blend::None:
                        break;

                    case Blend::Alpha:
                        layers.pixel[0] = blend_alpha(layers.pixel[0], layers.pixel[1], coeff_src, coeff_dst);
                        break;

                    case Blend::White:
                        layers.pixel[0] = blend_white(layers.pixel[0], coeff_wb);
                        break;

                    case Blend::Black:
                        layers.pixel[0] = blend_black(layers.pixel[0], coeff_wb);
                        break;
                }
            }
        }

        pixels[x] = layers.get_pixel();
    }
}

auto is_bg_enabled(Gba& gba, u8 bg_num)
{
    assert(bg_num <= 3);

    return bit::is_set(REG_DISPCNT, 8 + bg_num);
}

auto get_bg_cnt(Gba& gba, u8 bg_num) -> BGxCNT
{
    assert(bg_num <= 3);

    switch (bg_num)
    {
        case 0: return { REG_BG0CNT };
        case 1: return { REG_BG1CNT };
        case 2: return { REG_BG2CNT };
        case 3: return { REG_BG3CNT };
    }

    std::unreachable();
}

auto get_bg_meta(Gba& gba, u8 bg_num) -> BgMeta
{
    assert(bg_num <= 3);

    switch (bg_num)
    {
        case 0: return { .xscroll = REG_BG0HOFS, .yscroll = REG_BG0VOFS };
        case 1: return { .xscroll = REG_BG1HOFS, .yscroll = REG_BG1VOFS };
        case 2: return { .xscroll = REG_BG2HOFS, .yscroll = REG_BG2VOFS };
        case 3: return { .xscroll = REG_BG3HOFS, .yscroll = REG_BG3VOFS };
    }

    std::unreachable();
}

auto get_bg_affine_meta(Gba& gba, u8 bg_num) -> BgAffineMeta
{
    assert(bg_num >= 2 && bg_num <= 3);

    switch (bg_num)
    {
        case 2: return {
            .pa = static_cast<s16>(REG_BG2PA),
            .pb = static_cast<s16>(REG_BG2PB),
            .pc = static_cast<s16>(REG_BG2PC),
            .pd = static_cast<s16>(REG_BG2PD),
            .dx = gba.ppu.bg2x,
            .dy = gba.ppu.bg2y
        };

        case 3: return {
            .pa = static_cast<s16>(REG_BG3PA),
            .pb = static_cast<s16>(REG_BG3PB),
            .pc = static_cast<s16>(REG_BG3PC),
            .pd = static_cast<s16>(REG_BG3PD),
            .dx = gba.ppu.bg3x,
            .dy = gba.ppu.bg3y
        };
    }

    std::unreachable();
}

auto write_scanline_to_frame(Gba& gba, const u16 scanline[160])
{
    if (!gba.pixels)
    {
        assert(!"no pixels found!");
        return;
    }

    if (!gba.colour_callback)
    {
        if (gba.bpp == 2 || gba.bpp == 15 || gba.bpp == 16)
        {
            std::memcpy(&static_cast<u16*>(gba.pixels)[gba.stride * REG_VCOUNT], scanline, 240 * 2);
        }
        return;
    }

    const auto func = [&gba, &scanline](auto pixels) {
        assert(240 <= gba.stride);

        pixels = &pixels[gba.stride * REG_VCOUNT];

        for (auto i = 0; i < 240; i++)
        {
            pixels[i] = gba.colour_callback(gba.userdata, Colour{scanline[i]});
        }
    };

    switch (gba.bpp)
    {
        case 1:
        case 8:
            func(static_cast<u8*>(gba.pixels));
            break;

        case 2:
        case 15:
        case 16:
            func(static_cast<u16*>(gba.pixels));
            break;

        case 4:
        case 24:
        case 32:
            func(static_cast<u32*>(gba.pixels));
            break;

        default:
            assert(!"bpp invalid option!");
            break;
    }
}

auto tile_render(Gba& gba, std::span<BgLine> bg_lines, const Layer layers = Layer::ALL, const bool apply_window = true, const bool apply_merge = true) -> void
{
    // setup inital windowing (using win0 and win1)
    WindowBounds bounds{};

    if (apply_window)
    {
        bounds.build(gba);
    }

    ObjLine obj_line{};

    // only render obj if enabled
    if (is_obj_enabled(gba) && layers & Layer::OBJ)
    {
        parse_obj(gba, bounds, obj_line);

        // update bounds with any obj window
        if (apply_window)
        {
            bounds.apply_obj_window(gba, obj_line);
        }

        render_obj(gba, bounds, obj_line);
    }

    for (auto& line : bg_lines)
    {
        // only render bg if enabled
        if (is_bg_enabled(gba, line.num) && bit::is_set<u8>(layers, line.num))
        {
            const auto cnt = get_bg_cnt(gba, line.num);
            line.priority = cnt.Pr;

            switch (line.render_type)
            {
                case RenderType::Reg: {
                    const auto meta = get_bg_meta(gba, line.num);
                    render_tile_line_bg(gba, line, bounds, cnt, meta);
                }   break;

                case RenderType::Affine: {
                    const auto meta = get_bg_affine_meta(gba, line.num);
                    render_affine_line_bg(gba, line, bounds, cnt, meta);
                }   break;

                case RenderType::Bitmap3: {
                    const auto meta = get_bg_affine_meta(gba, line.num);
                    render_bitmap3_line_bg(gba, line, bounds, cnt, meta);
                }   break;

                case RenderType::Bitmap4: {
                    const auto meta = get_bg_affine_meta(gba, line.num);
                    render_bitmap4_line_bg(gba, line, bounds, cnt, meta);
                }   break;

                case RenderType::Bitmap5: {
                    const auto meta = get_bg_affine_meta(gba, line.num);
                    render_bitmap5_line_bg(gba, line, bounds, cnt, meta);
                }   break;
            }
        }
    }

    // merge all backgrounds and objects, applying blending if needed
    if (apply_merge)
    {
        u16 scanline[240];
        merge(gba, bounds, scanline, bg_lines, obj_line);
        write_scanline_to_frame(gba, scanline);
    }
}

// 4 regular
auto render_mode0(Gba& gba) -> void
{
    BgLine bg_lines[4]{ {0, RenderType::Reg}, {1, RenderType::Reg}, {2, RenderType::Reg}, {3, RenderType::Reg} };
    tile_render(gba, bg_lines);
}

// 2 regular, 1 affine
auto render_mode1(Gba& gba) -> void
{
    BgLine bg_lines[3]{ {0, RenderType::Reg}, {1, RenderType::Reg}, {2, RenderType::Affine} };
    tile_render(gba, bg_lines);
}

// 2 affine
auto render_mode2(Gba& gba) -> void
{
    BgLine bg_lines[2]{ {2, RenderType::Affine}, {3, RenderType::Affine} };
    tile_render(gba, bg_lines);
}

auto render_mode3(Gba& gba) -> void
{
    BgLine bg_lines[1]{ {2, RenderType::Bitmap3} };
    tile_render(gba, bg_lines);
}

auto render_mode4(Gba& gba) -> void
{
    BgLine bg_lines[1]{ {2, RenderType::Bitmap4} };
    tile_render(gba, bg_lines);
}

auto render_mode5(Gba& gba) -> void
{
    BgLine bg_lines[1]{ {2, RenderType::Bitmap5} };
    tile_render(gba, bg_lines);
}

constexpr auto bits_per_pixel_to_bytes_per_pixel(u8 bpp)
{
    switch (bpp)
    {
        case 1:
        case 8:
            return 1;

        case 2:
        case 15:
        case 16:
            return 2;

        case 4:
        case 24:
        case 32:
            return 4;

        default:
            assert(!"bpp invalid option!");
            return 0;
    }
}

} // namespace

auto render(Gba& gba) -> void
{
    // if forced blanking is enabled, the screen is black
    if (is_screen_blanked(gba)) [[unlikely]]
    {
        if (gba.pixels && gba.stride && gba.bpp)
        {
            const auto bpp = bits_per_pixel_to_bytes_per_pixel(gba.bpp);
            auto pixels = static_cast<u8*>(gba.pixels) + (gba.stride * REG_VCOUNT * bpp);
            std::memset(pixels, 0, 240 * bpp);
        }
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
        case 5: render_mode5(gba); break;

        default:
            #ifndef NDEBUG
            assert(!"unhandled ppu mode!");
            std::printf("unhandled ppu mode: %u\n", mode);
            #endif
            break;
    }
}

// todo: re-write this so that more internal info can be
// shown in the front end.
auto render_bg_mode(Gba& gba, u8 mode, u8 layer, std::span<u16> pixels) -> u8
{
    const auto cnt = get_bg_cnt(gba, layer);
    // for now, ignore the mode the frontend wants
    mode = get_mode(gba);

    const auto func = [&gba, &pixels, &layer](std::span<BgLine> lines, bool bitmap_mode)
    {
        if ((!bitmap_mode && layer < lines.size()) || (bitmap_mode && layer == 2))
        {
            for (auto& a : lines)
            {
                std::memset(a.pixels, 0, sizeof(a.pixels));
            }

            const auto index = bitmap_mode ? 0 : layer;

            tile_render(gba, lines, static_cast<Layer>(1 << layer), false, false);
            std::memcpy(pixels.data(), lines[index].pixels, sizeof(lines[index].pixels));
        }
    };

    switch (mode)
    {
        case 0: {
            BgLine bg_lines[4]{ {0, RenderType::Reg}, {1, RenderType::Reg}, {2, RenderType::Reg}, {3, RenderType::Reg} };
            func(bg_lines, false);
        } break;

        case 1: {
            BgLine bg_lines[3]{ {0, RenderType::Reg}, {1, RenderType::Reg}, {2, RenderType::Affine} };
            func(bg_lines, false);
        } break;

        case 2: {
            BgLine bg_lines[2]{ {2, RenderType::Affine}, {3, RenderType::Affine} };
            func(bg_lines, false);
        } break;

        case 3: {
            BgLine bg_lines[1]{ {2, RenderType::Bitmap3} };
            func(bg_lines, true);
        } break;

        case 4: {
            BgLine bg_lines[1]{ {2, RenderType::Bitmap4} };
            func(bg_lines, true);
        } break;
    }

    return cnt.Pr;
}

} // namespace gba::ppu
