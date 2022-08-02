// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include "frontend_base.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <minizip/unzip.h>

namespace frontend {

Base::Base(int argc, char** argv)
{
    if (argc < 2)
    {
        return;
    }

    if (!loadrom(argv[1]))
    {
        return;
    }

    if (argc == 3)
    {
        std::printf("loading bios\n");
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

// basic rom loading from zip, will flesh this out more soon
auto Base::loadzip(const std::string& path) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> data;
    auto zf = unzOpen64(path.c_str());

    if (zf != nullptr)
    {
        unz_global_info64 global_info;
        if (UNZ_OK == unzGetGlobalInfo64(zf, &global_info))
        {
            bool found = false;

            for (std::uint32_t i = 0; !found && i < global_info.number_entry; i++)
            {
                if (UNZ_OK == unzOpenCurrentFile(zf))
                {
                    unz_file_info64 file_info{};
                    char name[256]{};

                    if (UNZ_OK == unzGetCurrentFileInfo64(zf, &file_info, name, sizeof(name), nullptr, 0, nullptr, 0))
                    {
                        if (std::string_view{ name }.ends_with(".gba"))
                        {
                            data.resize(file_info.uncompressed_size);
                            unzReadCurrentFile(zf, data.data(), data.size());
                            found = true;
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

        unzClose(zf);
    }

    return data;
}

auto Base::loadfile(const std::string& path) -> std::vector<std::uint8_t>
{
    if (path.ends_with(".zip"))
    {
        printf("attempting to load via zip\n");
        // load zip
        return loadzip(path);
    }
    else
    {
        std::ifstream fs{ path.c_str(), std::ios_base::binary };

        if (fs.good()) {
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
    if (!state_data.empty() && state_data.size() == sizeof(gba::State))
    {
        std::printf("loadstate from: %s\n", state_path.c_str());
        auto state = std::make_unique<gba::State>();
        std::memcpy(state.get(), state_data.data(), state_data.size());
        return gameboy_advance.loadstate(*state);
    }
    return false;
}

auto Base::savestate(const std::string& path) -> bool
{
    auto state = std::make_unique<gba::State>();
    if (gameboy_advance.savestate(*state))
    {
        const auto state_path = create_state_path(path, state_slot);
        std::printf("savestate to: %s\n", state_path.c_str());
        return dumpfile(state_path, {reinterpret_cast<std::uint8_t*>(state.get()), sizeof(gba::State)});
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

} // namespace frontend
