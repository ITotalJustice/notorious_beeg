// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only
#include "bit.hpp"
#include "mem.hpp"
#include </opt/libdragon/mips64-elf/include/libdragon.h>
// #include <libdragon.h>
#include <cstdlib>
#include <gba.hpp>
#include <cstring>
#include <cstdio>
#include <span>

extern "C" {

// See: https://github.com/DragonMinded/libdragon/blob/e8051c77b34b0cafda2bb2e81bb44848b962d5f8/src/display.c#L176
// See: https://github.com/DragonMinded/libdragon/blob/92feeeb9b7d2c03d434a5bee00e82c52159a9a0b/src/rdp.c#L99
extern void *__safe_buffer[3];
extern void display_show_force(display_context_t disp);

} // extern "C"

namespace {

gba::Gba gameboy_advance{};
display_context_t disp = 0;

#define FS_ENTRIES_MAX 256

char fs_dir[512];
dir_t fs_entries[FS_ENTRIES_MAX];
size_t fs_entries_count;

std::span rom_data{gameboy_advance.rom};
std::size_t rom_size{};

#define FPS_SKIP_MAX (4)
#define SKIP_VSYNC (1)

enum class Menu
{
    MAIN,
    ROM,
};

Menu menu = Menu::MAIN;
bool loadrom_once = false;
int fps_skip = 0;

enum class ExtensionType
{
    ROM,
    UNK
};

[[noreturn]] auto display_message_error(const char* msg) -> void
{
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    for (;;)
    {
        console_clear();
            printf("%s", msg);
        console_render();
    }
}

auto get_extension_type(const char* file_name) -> ExtensionType
{
    auto ext = std::strrchr(file_name, '.');

    if (!ext)
    {
        return ExtensionType::UNK;
    }

    static const struct
    {
        const char* const ext;
        enum ExtensionType type;
    } ext_pairs[] =
    {
        { ".gba", ExtensionType::ROM },
    };

    for (auto ext_pair : ext_pairs)
    {
        if (!strcasecmp(ext, ext_pair.ext))
        {
            return ext_pair.type;
        }
    }

    return ExtensionType::UNK;
}

auto loadfile(const char* path) -> bool
{
    auto f = std::fopen(path, "rb");

    if (f)
    {
        rom_size = std::fread(rom_data.data(), 1, rom_data.size(), f);
        std::fclose(f);
        return rom_size > 0;
    }

    return false;
}

auto aquire_and_swap_buffers() -> void
{
    while(!(disp = display_lock()));
    graphics_fill_screen(disp, 0);
}

// SOURCE: https://github.com/DragonMinded/libdragon/blob/49e6a7d2f2ef88f0be111286f1678ae560fddfa1/examples/dfsdemo/dfsdemo.c#L24
auto chdir( const char * const dirent ) -> void
{
    /* Ghetto implementation */
    if( std::strcmp( dirent, ".." ) == 0 )
    {
        /* Go up one */
        int len = strlen( fs_dir ) - 1;

        /* Stop going past the min */
        if( fs_dir[len] == '/' && fs_dir[len-1] == '/' && fs_dir[len-2] == ':' )
        {
            return;
        }

        if( fs_dir[len] == '/' )
        {
            fs_dir[len] = 0;
            len--;
        }

        while( fs_dir[len] != '/')
        {
            fs_dir[len] = 0;
            len--;
        }
    }
    else
    {
        /* Add to end */
        std::strcat( fs_dir, dirent );
        std::strcat( fs_dir, "/" );
    }
}

// SOURCE: https://github.com/DragonMinded/libdragon/blob/49e6a7d2f2ef88f0be111286f1678ae560fddfa1/examples/dfsdemo/dfsdemo.c#L58
auto compare(const void * a, const void * b) -> int
{
    const auto *first = static_cast<const dir_t*>(a);
    const auto *second = static_cast<const dir_t*>(b);

    if(first->d_type == DT_DIR && second->d_type != DT_DIR)
    {
        /* First should be first */
        return -1;
    }

    if(first->d_type != DT_DIR && second->d_type == DT_DIR)
    {
        /* First should be second */
        return 1;
    }

    return std::strcmp(first->d_name, second->d_name);
}

// SOURCE: https://github.com/DragonMinded/libdragon/blob/49e6a7d2f2ef88f0be111286f1678ae560fddfa1/examples/dfsdemo/dfsdemo.c#L78
auto scan_dfs() -> bool
{
    fs_entries_count = 0;

    /* Grab first */
    int ret = dir_findfirst(fs_dir, &fs_entries[fs_entries_count]);

    if( ret != 0 )
    {
        return false;
    }

    fs_entries_count++;

    /* Copy in loop */
    while (fs_entries_count < FS_ENTRIES_MAX)
    {
        /* Grab next */
        ret = dir_findnext(fs_dir,&fs_entries[fs_entries_count]);

        if (ret != 0)
        {
            break;
        }

        // check the extention is what we want
        switch (get_extension_type(fs_entries[fs_entries_count].d_name))
        {
            case ExtensionType::ROM:
                fs_entries_count++;
                break;

            case ExtensionType::UNK:
                printf("skipping file: %s\n", fs_entries[fs_entries_count].d_name);
                break;
        }
    }

    if (fs_entries_count > 1)
    {
        /* Should sort! */
        std::qsort(fs_entries, fs_entries_count, sizeof(dir_t), compare);
    }

    return true;
}

auto core_vblank_callback(void* user, std::uint16_t line) -> void
{
    static int fps_skip_counter = 0;

    if (fps_skip_counter > 0)
    {
        fps_skip_counter--;
        // SMS_skip_frame(&sms, true);
    }
    else
    {
        static const char* skip_str[] = {"Frameskip: 0", "Frameskip: 1", "Frameskip: 2", "Frameskip: 3", "Frameskip: 4"};

        auto a = (short*)__safe_buffer[disp-1]+(320*25)+30;

        for (auto i = 0; i < 160; i++)
        {
            for (auto j = 0; j < 240; j++)
            {
                const auto r = (gameboy_advance.ppu.pixels[i][j] >> 1) & 0xF;
                const auto g = (gameboy_advance.ppu.pixels[i][j] >> 6) & 0xF;
                const auto b = (gameboy_advance.ppu.pixels[i][j] >> 11) & 0xF;
                a[(i + 15) * 320 + j] = graphics_make_color(r << 3, g << 3, b << 3, 0xFF);
            }
        }

        graphics_draw_text(disp, 10, 10, "NotoriousBEEG v0.0.1");
        graphics_draw_text(disp, 200, 10, skip_str[fps_skip]);
        graphics_draw_text(disp, 10, 220, "[Z = Menu] [L/R = dec/inc FPS skip]");
        #if SKIP_VSYNC
            display_show_force(disp);
        #else
            display_show(disp);
        #endif
        aquire_and_swap_buffers();
        fps_skip_counter = fps_skip;
        // SMS_skip_frame(&sms, false);
    }
}

auto menu_update_cursor(int cursor, int max) -> int
{
    if (cursor < 0)
    {
        return max-1;
    }
    return (cursor % max);
}

auto display_menu(struct controller_data* kdown, struct controller_data* kheld) -> void
{
    (void)kheld;

    struct RomEntry
    {
        const char* name;
        const unsigned char* data;
        size_t size;
    };

    static int cursor = 0;
    const int max = fs_entries_count;

    graphics_fill_screen(disp, 0);

    if (kdown->c[0].up)
    {
        cursor = menu_update_cursor(cursor-1, max);
    }
    else if (kdown->c[0].down)
    {
        cursor = menu_update_cursor(cursor+1, max);
    }
    else if (kdown->c[0].A && fs_entries_count > 0)
    {
        if (fs_entries[cursor].d_type == DT_REG)
        {
            char path[512];
            std::strcpy( path, fs_dir );
            std::strcat( path, fs_entries[cursor].d_name );

            bool success = false;

            switch (get_extension_type(fs_entries[cursor].d_name))
            {
                case ExtensionType::ROM:
                    success = loadfile(path);
                    break;

                case ExtensionType::UNK:
                    break;
            }

            if (success)
            {
                if (!gameboy_advance.loadrom(rom_data.subspan(0, rom_size)))
                {
                    char msg[128] = {0};
                    std::sprintf(msg, "failed to loadrom: %s", fs_entries[cursor].d_name);
                    display_message_error(msg);
                }
                else
                {
                    loadrom_once = true;
                    menu = Menu::ROM;
                }
            }
        }
        else if (fs_entries[cursor].d_type == DT_DIR)
        {
            chdir(fs_entries[cursor].d_name);
            scan_dfs();
            cursor = 0;
        }

        return;
    }
    else if (kdown->c[0].B)
    {
        chdir("..");
        scan_dfs();
        cursor = 0;
    }
    else if (kdown->c[0].Z && loadrom_once)
    {
        for (int i = 0; i < 3; i++)
        {
            graphics_fill_screen(disp, 0);
            display_show(disp);
            aquire_and_swap_buffers();
        }
        menu = Menu::ROM;
        return;
    }

    graphics_draw_text(disp, 10, 10, "NotoriousBEEG v0.0.1");

    for (int i = 0; i < max; i++)
    {
        if (cursor == i)
        {
            graphics_draw_text(disp, 5, 25 + (i * 15), "->");
            graphics_draw_text(disp, 20, 25 + (i * 15), fs_entries[i].d_name);
        }
        else
        {
            graphics_draw_text(disp, 5, 25 + (i * 15), fs_entries[i].d_name);
        }
    }

    display_show(disp);
    aquire_and_swap_buffers();
}

auto display_rom(struct controller_data* kdown, struct controller_data* kheld) -> void
{
    if (kdown->c[0].Z)
    {
        graphics_fill_screen(disp, 0);
        menu = Menu::MAIN;
        return;
    }
    else if (kdown->c[0].L)
    {
        fps_skip = fps_skip > 0 ? fps_skip - 1 : 0;
    }
    else if (kdown->c[0].R)
    {
        fps_skip = fps_skip < FPS_SKIP_MAX ? fps_skip + 1 : FPS_SKIP_MAX;
    }

    gameboy_advance.setkeys(gba::Button::UP, kheld->c[0].up);
    gameboy_advance.setkeys(gba::Button::RIGHT, kheld->c[0].right);
    gameboy_advance.setkeys(gba::Button::DOWN, kheld->c[0].down);
    gameboy_advance.setkeys(gba::Button::LEFT, kheld->c[0].left);
    gameboy_advance.setkeys(gba::Button::A, kheld->c[0].A);
    gameboy_advance.setkeys(gba::Button::B, kheld->c[0].B);
    gameboy_advance.setkeys(gba::Button::START, kheld->c[0].start);
    gameboy_advance.setkeys(gba::Button::L, kheld->c[0].L);
    gameboy_advance.setkeys(gba::Button::R, kheld->c[0].R);

    gameboy_advance.run();
}

auto update_joystick_directions(struct controller_data* keys) -> void
{
    #define JOYSTICK_DEAD_ZONE 32

    if( (keys->c[0].x < -JOYSTICK_DEAD_ZONE) )
    {
        keys->c[0].left = true;
    }
    else if ( keys->c[0].x > +JOYSTICK_DEAD_ZONE )
    {
        keys->c[0].right = true;
    }

    if( keys->c[0].y > +JOYSTICK_DEAD_ZONE )
    {
        keys->c[0].up = true;
    }
    else if ( keys->c[0].y < -JOYSTICK_DEAD_ZONE )
    {
        keys->c[0].down = true;
    }
}

} // namespace

auto main() -> int
{
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    controller_init();
    aquire_and_swap_buffers();

    gameboy_advance.set_vblank_callback(core_vblank_callback);

    if (dfs_init(DFS_DEFAULT_LOCATION) != DFS_ESUCCESS)
    {
        display_message_error("Filesystem failed to start!\n");
    }

    // first try and mount the romfs
    std::strcpy(fs_dir, "rom://");
    if (!scan_dfs())
    {
        // if that fails, mount the sd card
        std::strcpy(fs_dir, "sd://");
        if (!scan_dfs())
        {
            // if that fails, fail early because we have no games :(
            display_message_error("No roms or folders found!\n");
        }
    }

    for (;;)
    {
        controller_scan();
        auto kheld = get_keys_pressed();
        auto kdown = get_keys_down();
        update_joystick_directions(&kheld);
        update_joystick_directions(&kdown);

        switch (menu)
        {
            case Menu::MAIN:
                display_menu(&kdown, &kheld);
                break;

            case Menu::ROM:
                display_rom(&kdown, &kheld);
                break;
        }
    }
}
