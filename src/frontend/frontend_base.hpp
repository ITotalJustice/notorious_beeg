// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <gba.hpp>
#include <cstdint>
#include <span>
#include <tuple>
#include <vector>
#include <string>
#include <filesystem>

namespace frontend {

struct Base
{
    Base(int argc, char** argv);
    virtual ~Base();

public:
    virtual auto loop() -> void = 0;

    static auto dumpfile(const std::string& path, std::span<const std::uint8_t> data) -> bool;
    static auto loadzip(const std::string& path) -> std::vector<std::uint8_t>;
    static auto loadfile(const std::string& path) -> std::vector<std::uint8_t>;
    static auto replace_extension(std::filesystem::path path, const std::string& new_ext = "") -> std::string;
    static auto create_save_path(const std::string& path) -> std::string;
    static auto create_state_path(const std::string& path, int slot = 0) -> std::string;


protected:
    virtual auto loadrom(const std::string& path) -> bool;
    virtual auto closerom() -> void;

    virtual auto loadsave(const std::string& path) -> bool;
    virtual auto savegame(const std::string& path) -> bool;

    virtual auto loadstate(const std::string& path) -> bool;
    virtual auto savestate(const std::string& path) -> bool;

    virtual auto set_button(gba::Button button, bool down) -> void;

    virtual auto update_scale(int screen_width, int screen_height) -> void;
    virtual auto scale_with_aspect_ratio(int screen_width, int screen_height) -> std::tuple<int, int, int, int>;

public:
    gba::Gba gameboy_advance{};

    static constexpr auto width{240};
    static constexpr auto height{160};
    int scale{4};

    int state_slot{};
    std::string rom_path{};

    // set to true when a rom is loaded
    bool has_rom{false};
    // when true, the app continues to run, else it exits
    bool running{false};
    // when true, the game is running
    bool emu_run{true};
    // when true, states are recorded for rewinding
    bool enabled_rewind{false};
    // when true, the emulator is rewinding
    bool emu_rewind{false};
    // keeps ascpect ratio when resizing the sreen
    bool maintain_aspect_ratio{true};
};

} // namespace frontend
