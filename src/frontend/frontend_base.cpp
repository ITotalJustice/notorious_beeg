// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "frontend_base.hpp"
#include "gba.hpp"
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <vector>
#include <zlib.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>

namespace frontend {
namespace {

auto is_valid_rom_ext(std::string str)
{
    for (auto& c : str)
    {
        c = std::tolower(c);
    }

    const auto view = std::string_view{str};

    return view.ends_with(".gba") || view.ends_with(".gb") || view.ends_with(".gbc") || view.ends_with(".dmg");
}

struct MzMem
{
    union
    {
        const std::uint8_t* const_buf;
        std::uint8_t* buf;
    };
    std::size_t size;
    std::size_t offset;
    bool read_only;
};

auto minizip_tell64_file_func(void* opaque, [[maybe_unused]] void* stream) -> ZPOS64_T
{
    auto mem = static_cast<MzMem*>(opaque);
    return mem->offset;
}

auto minizip_seek64_file_func(void* opaque, [[maybe_unused]] void* stream, ZPOS64_T offset, int origin) -> long
{
    auto mem = static_cast<MzMem*>(opaque);
    std::size_t new_offset = 0;

    switch (origin)
    {
        case ZLIB_FILEFUNC_SEEK_SET:
            new_offset = offset;
            break;

        case ZLIB_FILEFUNC_SEEK_CUR:
            new_offset = mem->offset + offset;
            break;

        case ZLIB_FILEFUNC_SEEK_END:
            new_offset = (mem->size - 1) + offset;
            break;

        default:
            return -1;
    }

    if (new_offset > mem->size)
    {
        return -1;
    }

    mem->offset = new_offset;

    return 0;
}

auto minizip_open64_file_func(void* opaque, [[maybe_unused]] const void* filename, [[maybe_unused]] int mode) -> void*
{
    return opaque;
}

auto minizip_read_file_func(void* opaque, [[maybe_unused]] void* stream, void* buf, unsigned long size) -> unsigned long
{
    auto mem = static_cast<MzMem*>(opaque);

    if (mem->size <= mem->offset + size)
    {
        size = mem->size - mem->offset;
    }

    std::memcpy(buf, mem->const_buf + mem->offset, size);
    mem->offset += size;

    return size;
}

auto minizip_write_file_func([[maybe_unused]] void* opaque, [[maybe_unused]] void* stream, [[maybe_unused]] const void* buf, [[maybe_unused]] unsigned long size) -> unsigned long
{
    auto mem = static_cast<MzMem*>(opaque);

    if (mem->read_only)
    {
        return 0;
    }

    if (mem->size <= mem->offset + size)
    {
        mem->buf = static_cast<std::uint8_t*>(std::realloc(mem->buf, mem->offset + size));
        if (mem->buf == nullptr)
        {
            return 0;
        }

        mem->size = mem->offset + size;
    }

    std::memcpy(mem->buf + mem->offset, buf, size);
    mem->offset += size;

    return size;
}

auto minizip_close_file_func([[maybe_unused]] void* opaque, [[maybe_unused]] void* stream) -> int
{
    return 0;
}

auto minizip_testerror_file_func([[maybe_unused]] void* opaque, [[maybe_unused]] void* stream) -> int
{
    return 0;
}

// returns the number of files zipped
[[nodiscard]]
auto zipall_internal(zipFile zf, const std::string& folder) -> std::size_t
{
    std::size_t count{};

    // for (auto& folder : folders)
    {
        for (auto& entry : std::filesystem::recursive_directory_iterator{folder})
        {
            if (entry.is_regular_file())
            {
                // get the fullpath
                const auto path = entry.path().string();
                std::ifstream fs{path, std::ios::binary};

                if (fs.good())
                {
                    // read file into buffer
                    const auto file_size = entry.file_size();
                    std::vector<char> buffer(file_size);
                    fs.read(buffer.data(), buffer.size());

                    // open the file inside the zip
                    if (ZIP_OK != zipOpenNewFileInZip(zf,
                        path.c_str(),           // filepath
                        nullptr,                   // info, optional
                        nullptr, 0,                // extrafield and size, optional
                        nullptr, 0,                // extrafield-global and size, optional
                        "TotalGBA",                   // comment, optional
                        Z_DEFLATED,             // mode
                        Z_DEFAULT_COMPRESSION   // level
                    )) {
                        std::printf("failed to open file in zip: %s\n", path.c_str());
                        continue;
                    }

                    // write out the entire file
                    if (Z_OK != zipWriteInFileInZip(zf, buffer.data(), buffer.size()))
                    {
                        std::printf("failed to write file in zip: %s\n", path.c_str());
                    }
                    else
                    {
                        count++;
                    }

                    // don't forget to close when done!
                    if (Z_OK != zipCloseFileInZip(zf))
                    {
                        std::printf("failed to close file in zip: %s\n", path.c_str());
                    }
                }
                else
                {
                    std::printf("failed to open file %s\n", path.c_str());
                }
            }
        }
    }

    return count;
}

// basic rom loading from zip, will flesh this out more soon
auto loadzip_internal(unzFile zf) -> std::vector<std::uint8_t>
{
    if (zf != nullptr)
    {
        unz_global_info64 global_info;
        if (UNZ_OK == unzGetGlobalInfo64(zf, &global_info))
        {
            bool found = false;

            for (std::uint64_t i = 0; !found && i < global_info.number_entry; i++)
            {
                if (UNZ_OK == unzOpenCurrentFile(zf))
                {
                    unz_file_info64 file_info{};
                    char name[256]{};

                    if (UNZ_OK == unzGetCurrentFileInfo64(zf, &file_info, name, sizeof(name), nullptr, 0, nullptr, 0))
                    {
                        if (is_valid_rom_ext(name))
                        {
                            std::vector<std::uint8_t> data;
                            data.resize(file_info.uncompressed_size);
                            const auto result = unzReadCurrentFile(zf, data.data(), data.size());

                            if (result > 0 && file_info.uncompressed_size == static_cast<std::size_t>(result))
                            {
                                return data;
                            }
                        }
                    }

                    unzCloseCurrentFile(zf);
                }

                // advance to the next file (if there is one)
                if (i + 1 < global_info.number_entry)
                {
                    unzGoToNextFile(zf); // todo: error handling
                }
            }
        }
    }

    return {};
}

const zlib_filefunc64_def zlib_filefunc64{
    .zopen64_file = minizip_open64_file_func,
    .zread_file = minizip_read_file_func,
    .zwrite_file = minizip_write_file_func,
    .ztell64_file = minizip_tell64_file_func,
    .zseek64_file = minizip_seek64_file_func,
    .zclose_file = minizip_close_file_func,
    .zerror_file = minizip_testerror_file_func,
    .opaque = nullptr,
};

} // namespace

Base::Base(int argc, char** argv)
{
    if (argc < 2)
    {
        return;
    }

    if (!loadrom(argv[1]))
    {
        std::printf("loading rom from argv[1]: %s\n", argv[1]);
        return;
    }

    if (argc == 3)
    {
        std::printf("loading bios from argv[2]: %s\n", argv[2]);
        const auto bios = loadfile(argv[2]);
        if (bios.empty())
        {
            return;
        }

        if (!gameboy_advance.loadbios(bios))
        {
            return;
        }
    }
}

Base::~Base()
{
    closerom();
}

auto Base::dumpfile(const std::string& path, std::span<const std::uint8_t> data) -> bool
{
    std::ofstream fs{path.c_str(), std::ios_base::binary};

    if (fs.good())
    {
        fs.write(reinterpret_cast<const char*>(data.data()), data.size());

        if (fs.good())
        {
            return true;
        }
    }

    return false;
}

auto Base::zipall(const std::string& folder, const std::string& output) -> std::size_t
{
    if (auto zfile = zipOpen64(output.c_str(), APPEND_STATUS_CREATE))
    {
        const auto count = zipall_internal(zfile, folder);
        zipClose(zfile, "TotalGBA");
        // todo: error handling!
        return count;
    }

    return 0;
}

auto Base::loadzip(const std::string& path) -> std::vector<std::uint8_t>
{
    if (auto zf = unzOpen64(path.c_str()); zf != nullptr)
    {
        auto data = loadzip_internal(zf);
        unzClose(zf);
        return data;
    }

    return {};
}

auto Base::loadfile(const std::string& path) -> std::vector<std::uint8_t>
{
    if (path.ends_with(".zip"))
    {
        std::printf("attempting to load via zip\n");
        if (auto zf = unzOpen64(path.c_str()); zf != nullptr)
        {
            // don't const as it prevents move
            auto data = loadzip_internal(zf);
            unzClose(zf);
            return data;
        }
    }
    else
    {
        std::ifstream fs{ path.c_str(), std::ios_base::binary };

        if (fs.good())
        {
            fs.seekg(0, std::ios_base::end);
            const auto size = fs.tellg();
            fs.seekg(0, std::ios_base::beg);

            std::vector<std::uint8_t> data;
            data.resize(size);

            fs.read(reinterpret_cast<char*>(data.data()), data.size());

            if (fs.good())
            {
                return data;
            }
        }
    }

    return {};
}

auto Base::loadfile_mem(const std::string& path, std::span<const std::uint8_t> data) -> std::vector<std::uint8_t>
{
    if (path.ends_with(".zip"))
    {
        MzMem mzmem{
            .const_buf = data.data(),
            .size = data.size(),
            .offset = 0,
            .read_only = true
        };

        auto def = zlib_filefunc64;
        def.opaque = &mzmem;

        std::printf("attempting to load via zip\n");
        if (auto zf = unzOpen2_64(path.c_str(), &def); zf != nullptr)
        {
            // don't const as it prevents move
            auto unziped_data = loadzip_internal(zf);
            unzClose(zf);
            return unziped_data;
        }
    }
    else
    {
        std::vector<std::uint8_t> output;
        output.assign(data.begin(), data.end());
        return output;
    }

    return {};
}

auto Base::replace_extension(std::filesystem::path path, const std::string& new_ext) -> std::string
{
    return path.replace_extension(new_ext).string();
}

auto Base::create_save_path(const std::string& path) -> std::string
{
    return replace_extension(path, ".sav");
}

auto Base::create_state_path(const std::string& path, int slot) -> std::string
{
    return replace_extension(path, ".state" + std::to_string(slot));
}

auto Base::closerom() -> void
{
    if (has_rom)
    {
        savegame(rom_path);
        has_rom = false;
    }

    emu_run = false;
}

auto Base::loadrom(const std::string& path) -> bool
{
    // close any previous loaded rom
    closerom();

    rom_path = path;
    const auto rom_data = loadfile(rom_path);
    if (rom_data.empty())
    {
        return false;
    }

    if (!gameboy_advance.loadrom(rom_data))
    {
        return false;
    }

    emu_run = true;
    has_rom = true;
    loadsave(rom_path);

    return true;
}

auto Base::loadrom_mem(const std::string& path, std::span<const std::uint8_t> data) -> bool
{
    closerom();

    rom_path = path;
    const auto rom_data = loadfile_mem(path, data);
    if (rom_data.empty())
    {
        return false;
    }

    if (!gameboy_advance.loadrom(rom_data))
    {
        return false;
    }

    emu_run = true;
    has_rom = true;
    loadsave(rom_path);

    return true;
}

auto Base::loadsave(const std::string& path) -> bool
{
    const auto save_path = create_save_path(path);
    const auto save_data = loadfile(save_path);
    if (!save_data.empty())
    {
        std::printf("loading save from: %s\n", save_path.c_str());
        return gameboy_advance.loadsave(save_data);
    }

    return false;
}

auto Base::savegame(const std::string& path) -> bool
{
    // is save isn't dirty, then return early
    if (!gameboy_advance.is_save_dirty(true))
    {
        return false;
    }

    const auto save_path = create_save_path(path);
    const auto save_data = gameboy_advance.getsave();
    if (!save_data.empty())
    {
        std::printf("dumping save to: %s\n", save_path.c_str());
        return dumpfile(save_path, save_data);
    }

    return false;
}

auto Base::loadstate(const std::string& path) -> bool
{
    const auto state_path = create_state_path(path, state_slot);
    const auto state_data = loadfile(state_path);

    if (!state_data.empty())
    {
        auto state = std::make_unique<gba::State>();
        uLongf dst_size = gba::StateMeta::SIZE;

        if (Z_OK == uncompress(reinterpret_cast<std::uint8_t*>(state.get()), &dst_size, state_data.data(), state_data.size()))
        {
            std::printf("loadstate from: %s\n", state_path.c_str());
            return gameboy_advance.loadstate(*state);
        }
    }
    return false;
}

auto Base::savestate(const std::string& path) -> bool
{
    auto state = std::make_unique<gba::State>();
    if (gameboy_advance.savestate(*state))
    {
        const auto state_path = create_state_path(path, state_slot);

        std::vector<std::uint8_t> buf;
        buf.resize(compressBound(gba::StateMeta::SIZE));
        uLongf dst_size = gba::StateMeta::SIZE;

        if (Z_OK == compress(buf.data(), &dst_size, reinterpret_cast<std::uint8_t*>(state.get()), gba::StateMeta::SIZE))
        {
            buf.resize(dst_size);
            std::printf("savestate to: %s\n", state_path.c_str());
            return dumpfile(state_path, buf);
        }
    }
    return false;
}

auto Base::set_button(gba::Button button, bool down) -> void
{
    if (has_rom && emu_run && !emu_rewind)
    {
        gameboy_advance.setkeys(button, down);
    }
}

auto Base::update_scale(int screen_width, int screen_height) -> void
{
    const auto scale_w = screen_width / width;
    const auto scale_h = screen_height / height;

    scale = std::min(scale_w, scale_h);
}

auto Base::scale_with_aspect_ratio(int screen_width, int screen_height) -> std::tuple<int, int, int, int>
{
    const auto w = width * scale;
    const auto h = height * scale;
    const auto x = (screen_width - w) / 2;
    const auto y = (screen_height - h) / 2;

    return std::tuple{x, y, w, h};
}

} // namespace frontend
