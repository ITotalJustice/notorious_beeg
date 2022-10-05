// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "fat.hpp"
#include "gba.hpp"

#include <cassert>
#include <cstring>
#include <span>

namespace gba::fat {
namespace {

// BIOS Parameter Block offsets
enum BPB
{
    BS_JmpBoot = 0x00,
    BS_OEMName = 0x03,
    // BIOS Parameter Block
    BPB_BytesPerSec = 0x0B,
    BPB_SecPerClus = 0x0D,
    BPB_RsvdSecCnt = 0x0E,
    BPB_NumFATs = 0x10,
    BPB_RootEntCnt = 0x11,
    BPB_TotSec16 = 0x13,
    BPB_Media = 0x15,
    BPB_FatSz16 = 0x16,
    BPB_SecPerTrk = 0x18,
    BPB_NumHeads = 0x1A,
    BPB_HiddSec = 0x1C,
    BPB_TotSec32 = 0x20,
    // FAT32 extended block
    BPB_FATSz32 = 0x24,
    BPB_ExtFlags = 0x28,
    BPB_FSVer = 0x2A,
    BPB_RootClus = 0x2C,
    BPB_FSInfo = 0x30,
    BPB_BkBootSec = 0x32,
    BPB_Reserved = 0x34,
    // Ext BIOS Parameter Block for FAT32
    BS_DrvNum = 0x40,
    BS_Reserved = 0x41,
    BS_BootSig = 0x42,
    BS_VolID = 0x43,
    BS_VolLab = 0x47,
    BS_FilSysType = 0x52,
    // Bootcode
    BS_BootCode32 = 0x5A,
    BS_BootSign = 0x1FE,
};

enum FSI
{
    FSI_LeadSig = 0,
    FSI_Reserved1 = 4,
    FSI_StrucSig = 484,
    FSI_Free_Count = 488,
    FSI_Nxt_Free = 492,
    FSI_Reserved2 = 496,
    FSI_TrailSig = 508,
};

enum FAT
{
    FAT_ReserveC1 = 0x0,
    FAT_ReserveC2 = 0x4,
    FAT_End = 0x8,
};

inline auto read16(const u8* data) -> u16
{
    u16 result{};

    result |= data[0] << 0;
    result |= data[1] << 8;

    return result;
}

inline auto write8(u8* data, u8 value) -> void
{
    data[0] = value;
}

inline auto write16(u8* data, u16 value) -> void
{
    data[0] = value >> 0;
    data[1] = value >> 8;
}

inline auto write32(u8* data, u32 value) -> void
{
    data[0] = value >> 0;
    data[1] = value >> 8;
    data[2] = value >> 16;
    data[3] = value >> 24;
}

} // namespace

auto create_image(std::span<u8> data) -> bool
{
    if (data.size() != 512 * 1024 * 1024)
    {
        return false;
    }

    // http://elm-chan.org/docs/fat_e.html#bpb
    u8 mbr[512]{};
    u8 fsinfo[512]{};
    u8 fat[512]{};

    // some of the params are based on mkfs.fat
    write8(&mbr[BS_JmpBoot + 0], 0xEB); // short jump
    write8(&mbr[BS_JmpBoot + 1], 0x58); // ??
    write8(&mbr[BS_JmpBoot + 2], 0x90); // nop

    write8(&mbr[BS_OEMName + 0], 'M');
    write8(&mbr[BS_OEMName + 1], 'S');
    write8(&mbr[BS_OEMName + 2], 'W');
    write8(&mbr[BS_OEMName + 3], 'I');
    write8(&mbr[BS_OEMName + 4], 'N');
    write8(&mbr[BS_OEMName + 5], '4');
    write8(&mbr[BS_OEMName + 6], '.');
    write8(&mbr[BS_OEMName + 7], '1');

    constexpr auto BytesPerSec = 512;
    constexpr auto SecPerClus = 8; // 4k cluster
    constexpr auto NumFATs = 2;
    constexpr auto FATSz32 = 0x00000400;

    write16(&mbr[BPB_BytesPerSec], BytesPerSec); // 0x0200
    write8(&mbr[BPB_SecPerClus], SecPerClus); // todo: 0x08
    write16(&mbr[BPB_RsvdSecCnt], 32); // should be 32 on fat32
    write8(&mbr[BPB_NumFATs], NumFATs); // should always be 2
    write16(&mbr[BPB_RootEntCnt], 0); // must be 0 for fat32
    write16(&mbr[BPB_TotSec16], 0); // must be 0 for fat32
    write8(&mbr[BPB_Media], 0xF8); // value must be placed in FAT[0]
    write16(&mbr[BPB_FatSz16], 0); // must be 0 for fat32
    write16(&mbr[BPB_SecPerTrk], 0x003F); // todo: 0x003F
    write16(&mbr[BPB_NumHeads], 0x0020); // todo: 0x0020 (size * 4)
    write32(&mbr[BPB_HiddSec], 0); // should be 0
    write32(&mbr[BPB_TotSec32], 0x000FFFFC); // todo: 0x000FFFFC (size * 4)

    write32(&mbr[BPB_FATSz32], FATSz32); // todo: 0x00000400
    write16(&mbr[BPB_ExtFlags], 0); // no flags
    write16(&mbr[BPB_FSVer], 0); // version 0
    write32(&mbr[BPB_RootClus], 2); // usually 2?
    write16(&mbr[BPB_FSInfo], 1); // usually 1?
    write16(&mbr[BPB_BkBootSec], 6); // usually 6?
    std::memset(&mbr[BPB_Reserved], 0, 12); // set to 0

    write8(&mbr[BS_DrvNum], 0x80); // fixed disk
    write8(&mbr[BS_Reserved], 0x00); // set to 0
    write8(&mbr[BS_BootSig], 0x29); //
    write32(&mbr[BS_VolID], 0x85319E61); // 0x4A8DD912 serial number

    write8(&mbr[BS_VolLab + 0], 'N');
    write8(&mbr[BS_VolLab + 1], 'O');
    write8(&mbr[BS_VolLab + 2], ' ');
    write8(&mbr[BS_VolLab + 3], 'N');
    write8(&mbr[BS_VolLab + 4], 'A');
    write8(&mbr[BS_VolLab + 5], 'M');
    write8(&mbr[BS_VolLab + 6], 'E');
    write8(&mbr[BS_VolLab + 7], ' ');
    write8(&mbr[BS_VolLab + 8], ' ');
    write8(&mbr[BS_VolLab + 9], ' ');
    write8(&mbr[BS_VolLab + 10], ' ');

    write8(&mbr[BS_FilSysType + 0], 'F');
    write8(&mbr[BS_FilSysType + 1], 'A');
    write8(&mbr[BS_FilSysType + 2], 'T');
    write8(&mbr[BS_FilSysType + 3], '3');
    write8(&mbr[BS_FilSysType + 4], '2');
    write8(&mbr[BS_FilSysType + 5], ' ');
    write8(&mbr[BS_FilSysType + 6], ' ');
    write8(&mbr[BS_FilSysType + 7], ' ');
    std::memset(&mbr[BS_BootCode32], 0, 420); // set to 0
    write16(&mbr[BS_BootSign], 0xAA55);

    // write fsinfo
    write32(&fsinfo[FSI_LeadSig], 0x41615252); // magic num
    std::memset(&fsinfo[FSI_Reserved1], 0, 480); // set to 0
    write32(&fsinfo[FSI_StrucSig], 0x61417272); // magic num
    write32(&fsinfo[FSI_Free_Count], 0x0001FEFA); // todo: 0x0001FEFA (size * 4ish)
    write32(&fsinfo[FSI_Nxt_Free], 0x2); // todo: verify this
    std::memset(&fsinfo[FSI_Reserved2], 0, 12); // set to 0
    write32(&fsinfo[FSI_TrailSig], 0xAA550000); // magic num

    // write fat
    write32(&fat[FAT_ReserveC1], 0x0FFFFFF8);
    write32(&fat[FAT_ReserveC2], 0x0FFFFFFF);
    write32(&fat[FAT_End], 0x0FFFFFF8); // typically 0x0FFFFFFF, mkfat shows 0x0FFFFFF8

    std::memset(data.data(), 0, data.size());
    std::memcpy(data.data() + 0, mbr, sizeof(mbr));
    std::memcpy(data.data() + 512, fsinfo, sizeof(fsinfo));
    std::memcpy(data.data() + 32 * 512, fat, sizeof(fat));
    std::memcpy(data.data() + (FATSz32 + 32) * 512, fat, sizeof(fat));

    return true;
}

auto flush(Gba& gba, u64 offset, u64 size) -> bool
{
    if (gba.fat_flush_callback)
    {
        gba.fat_flush_callback(gba.userdata, offset, size);
    }

    return true;
}

auto read16(Gba& gba, u64 addr) -> u16
{
    return read16(gba.fat32_data + addr);
}

void write16(Gba& gba, u64 addr, u16 value)
{
    write16(gba.fat32_data + addr, value);
}

} // namespace gba::fat
