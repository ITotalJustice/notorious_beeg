# gba emu

gba emulator witten in c++23.

---

## table, switch, goto

the arm7tdmi interptreter supports different "backends" for executing instructions.

to select one of the backends, simple build with `-DINTERPRETER=INTERPRETER_X` where `X` is one of the following

- INTERPRETER_TABLE
- INTERPRETER_SWITCH `(default)`
- INTERPRETER_GOTO

for conveince, you can build with a cmake preset like so:

```sh
cmake --preset sdl2-table
cmake --build --preset sdl2-table
```

Below are a few tests i tried using the various benchmark presets in `CMakePresets.json`. The fps is an average after running for 20s, accumulated after 3 runs.

### results (OpenLara.gba, gcc 12.2.1, LTO=OFF):

| backend | speed | size | preset |
|---|:---:|:---:|:---:|
| `table` | 1080 fps | 442.6 KiB (453,184) | benchmark-table |
| `switch` | 1120 fps | 414.5 KiB (424,432) | benchmark-switch |
| `goto` | 1020 fps | 1.3 MiB (1,380,776) | benchmark-goto |

*NOTE: `Ofast` was slower than `O3` in both `table` and `switch` builds, but faster in `goto` build (Ofast=1146 fps, O3=1115 fps).*

### results (OpenLara.gba, gcc 12.2.1, LTO=ON):

| backend | speed | size | preset |
|---|:---:|:---:|:---:|
| `table` | 1060 fps | 442.0 KiB (452,600) | benchmark-table-lto |
| `switch` | 1095 fps | 409.0 KiB (418,840) | benchmark-switch-lto |
| `goto` | 985 fps | 1.3 MiB (1,368,784) | benchmark-goto-lto |

*NOTE: unlike the non-lto builds, `O3` was always faster than `Ofast`.*

### results (OpenLara.gba, clang 14.0.5, LTO=OFF):

| backend | speed | size | preset |
|---|:---:|:---:|:---:|
| `table` | 1022 fps | 503.7 KiB (515,816) | benchmark-clang-table |
| `switch` | 1065 fps | 489.6 KiB (501,320) | benchmark-clang-switch |
| `goto` | 1010 fps | 860.9 KiB (881,568) | benchmark-clang-goto |

*NOTE: lto wouldn't work for clang build for some reason, this is in an issue on my platform, not with my emulator / cmake.*

---

## web builds

web builds are the easiest way to quickly test a game. builds are automatically built from master. please report any bugs you find, giving as much info as possible such as browser, os, game etc.

[gh-pages](https://itotaljustice.github.io/notorious_beeg) version doesn't support threads / mutexs. may crash.

[netlify](https://notorious-beeg.netlify.app) version supports threads / mutexes, won't crash.

---

## yet to implement

list of stuff that i haven't yet implemented. for the most part, everything is done!

### ppu
- mosaic bg
- mosaic obj

### apu
- correct fifo <https://github.com/mgba-emu/mgba/issues/1847>

### dma
- dma3 special

### misc
- correct openbus behaviour
- correct rom access timings

---

## thanks
- cowbite spec <https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm>
- gbatek <https://problemkaputt.de/gbatek.htm>
- tonc <https://www.coranac.com/tonc/text/toc.htm>
- belogic for apu <http://belogic.com/gba/>
- dillion for flashmem <https://dillonbeliveau.com/2020/06/05/GBA-FLASH.html>
- dennis for eeprom <https://densinh.github.io/DenSinH/emulation/2021/02/01/gba-eeprom.html>
- emudev discord
- jsmolka for their very helpful tests <https://github.com/jsmolka/gba-tests>
- normmatt (and vba) for the builtin bios <https://github.com/Nebuleon/ReGBA/tree/master/bios>
- endrift for mgba <https://github.com/mgba-emu/mgba>
- ocornut for imgui <https://github.com/ocornut/imgui>
- ocornut for imgui_club <https://github.com/ocornut/imgui_club>
- everyone that has contributed to the bios decomp <https://github.com/Gericom/gba_bios>
- xproger for openlara (fixed several bugs in my emu) <https://github.com/XProger/OpenLara>
- zayd for info on rtc <https://beanmachine.alt.icu/post/rtc/>
- pokeemerald for a being a good reference <https://github.com/pret/pokeemerald>
- kenney for the onscreen control buttons <https://www.kenney.nl/assets/onscreen-controls>
- ifeelfine for yoshi gif <https://tenor.com/view/yoshi-bloated-cute-gif-12835998>
