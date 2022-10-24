// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

#include <frontend_base.hpp>
#include <gba.hpp>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace {

constexpr auto VERSION = 1;

enum Error : int
{
    // passed the test
    PASS = 0,
    // bad or missing arguments
    ARGS = 1,
    // bad rom, failed to load
    ROM = 2,
    // bad expected image, failed to load
    IMAGE = 3,
    // test fails
    TEST = 4,
};

enum RW : int
{
    // read image, run rom and compare output
    READ = 0,
    // run rom and write new image
    WRITE = 1,
};

constexpr auto width = 240;
constexpr auto height = 160;
constexpr auto bpp = sizeof(std::uint32_t);
std::uint32_t pixels[height][width];

auto colour_callback(void* user, gba::Colour c) -> std::uint32_t
{
    return (c.b8() << 16) | (c.g8() << 8) | (c.r8() << 0) | 0xFF000000;
}

} // namespace

struct Button
{
    gba::Button button; // the button the press
    int frame; // the frame to press it (released the frame after)
    bool used; // internally used to keep track of the button press
};

auto main(int argc, char** argv) -> int
{
    std::printf("Testing Version: %d\n", VERSION);

    if (argc < 4)
    {
        std::printf("missing args!\n");
        std::printf("\targv[1] = rom_path\n");
        std::printf("\targv[2] = loop_intr\n");
        std::printf("\targv[3] = read_or_write\n");
        return Error::ARGS;
    }

    auto gameboy_advance = std::make_unique<gba::Gba>();

    gameboy_advance->set_colour_callback(colour_callback);
    gameboy_advance->set_pixels(pixels, width, bpp);

    const auto rom_path = argv[1];
    std::string _image_path;

    if (argc > 4)
    {
        auto tmp = frontend::Base::replace_extension(rom_path, "");
        tmp += "-";
        tmp += argv[4];
        tmp += ".png";
        _image_path = tmp;
    }
    else
    {
        _image_path = frontend::Base::replace_extension(rom_path, ".png");
    }
    // const auto _image_path = frontend::Base::replace_extension(rom_path, ".png");
    const auto image_path = _image_path.c_str();
    const auto frames = std::stoi(argv[2]);
    const auto read_or_write = std::stoi(argv[3]);
    std::vector<Button> buttons;

    if (argc > 5)
    {
        std::string_view view{argv[5]};
        bool running = true;

        while (running)
        {
            Button button{};
            const auto button_str = view.substr(0, view.find_first_of(';'));
            auto frame_str = view.substr(button_str.size()+1);

            if (frame_str.find_first_of(';') != frame_str.npos)
            {
                frame_str = frame_str.substr(0, frame_str.find_first_of(';'));
                view = view.substr(button_str.size() + 1 + frame_str.size() + 1);
            }
            else
            {
                running = false;
            }

            if (button_str == "A") { button.button = gba::Button::A; }
            else if (button_str == "B") { button.button = gba::Button::B; }
            else if (button_str == "LEFT") { button.button = gba::Button::LEFT; }
            else if (button_str == "RIGHT") { button.button = gba::Button::RIGHT; }
            else if (button_str == "UP") { button.button = gba::Button::UP; }
            else if (button_str == "DOWN") { button.button = gba::Button::DOWN; }
            else if (button_str == "START") { button.button = gba::Button::START; }
            else if (button_str == "SELECT") { button.button = gba::Button::SELECT; }
            else if (button_str == "L") { button.button = gba::Button::L; }
            else if (button_str == "R") { button.button = gba::Button::R; }

            // performance doesn't matter
            button.frame = std::stoi(std::string{frame_str});
            buttons.emplace_back(button);
        }
    }

    const auto rom_data = frontend::Base::loadfile(rom_path);
    if (rom_data.empty())
    {
        std::printf("failed to load rom file\n");
        return Error::ROM;
    }

    if (!gameboy_advance->loadrom(rom_data))
    {
        std::printf("failed to load rom\n");
        return Error::ROM;
    }

    for (auto i = 0; i < frames; i++)
    {
        gameboy_advance->run();

        for (auto& [button, button_frame, used] : buttons)
        {
            if (i > button_frame)
            {
                gameboy_advance->setkeys(button, !used);
                used = true;
            }
        }

    }

    if (read_or_write == RW::WRITE)
    {
        if (!stbi_write_png(image_path, width, height, bpp, pixels, width * bpp))
        {
            std::printf("failed to write image to: %s\n", image_path);
            return Error::IMAGE;
        }

        printf("writing!\n");
    }
    else if (read_or_write == RW::READ)
    {
        int w;
        int h;
        int channels;
        auto buf = stbi_load(image_path, &w, &h, &channels, 0);
        if (!buf)
        {
            std::printf("failed to load image to: %s reason: %s\n", image_path, stbi_failure_reason());
            return Error::IMAGE;
        }

        if (w*h*channels != sizeof(pixels))
        {
            stbi_image_free(buf);
            std::printf("image size doesnt match! want: %zu got: %d\n", sizeof(pixels), w*h*channels);
            return Error::IMAGE;
        }

        if (std::memcmp(buf, pixels, w*h*channels))
        {
            stbi_image_free(buf);
            std::printf("test failed, image missmatch!\n");
            return Error::TEST;
        }

        stbi_image_free(buf);
        std::printf("test passed!\n");
    }
    else
    {
        std::printf("bad args for read_or_write\n");
        return Error::ARGS;
    }

    return Error::PASS;
}
