# gba emu

v0.0.3-dev

gba emulator witten in c++23.

---

## table, switch, goto

the arm7tdmi interptreter supports different "backends" for executing instructions.

to select one of the backends, simple build with `-DINTERPRETER=INTERPRETER_X` where `X` is one of the following

- INTERPRETER_TABLE
- INTERPRETER_SWITCH
- INTERPRETER_GOTO

for conveince, you can build with a cmake preset like so:

```sh
cmake --preset sdl2-table
cmake --build --preset sdl2-table
```

---

## web builds

web builds are the easiest way to quickly test a game. builds are automatically built from master. please report any bugs you find, giving as much info as possible such as browser, os, game etc.

[gh-pages](https://itotaljustice.github.io/notorious_beeg) version doesn't support threads / mutexs. may crash.

[netlify](https://notorious-beeg.netlify.app) version supports threads / mutexes, won't crash.

---

## changelog
0.0.3
- fully implemented flash. fixes [#19](https://github.com/ITotalJustice/notorious_beeg/issues/19).
- fixed flash, eeprom, sram not being initialised to 0xFF.
- fixed sram region not being mirrored.
- have addr alignment be handled by the memory r/w handlers. this sram 16-32bit stores at unaligned addresses.
- fix regression in mode3 rendering.
- more accurately mix audio channels.
- fix frontend audio_stream not being thread-safe.
- fix sprite_y wrapping behaviour. fixes [#27](https://github.com/ITotalJustice/notorious_beeg/issues/27).
- fix 8-bit io writes. fixes [#18](https://github.com/ITotalJustice/notorious_beeg/issues/18).
- add io viewer.
- fix `get_bg_offset()` being used incorrectly by always using X index.
- added background blending [#28](https://github.com/ITotalJustice/notorious_beeg/issues/28)
- added sprite priority [#29](https://github.com/ITotalJustice/notorious_beeg/issues/29)
- added sprite blending [#30](https://github.com/ITotalJustice/notorious_beeg/issues/30)
- added win0/win1 [#31](https://github.com/ITotalJustice/notorious_beeg/issues/31)
- added object windowing
- render obj in bitmap mode [#42](https://github.com/ITotalJustice/notorious_beeg/issues/42)
- enable blending/windowing in bitmap modes
- implement correct OOB rom reads, removing wrong rom mirroring that was added to "fix" minish cap. see [#12](https://github.com/ITotalJustice/notorious_beeg/issues/12)
- implement timer cascade. fixes [#2](https://github.com/ITotalJustice/notorious_beeg/issues/2)
- fix dma dst not being incremented if increment type == 3. fixes [#48](https://github.com/ITotalJustice/notorious_beeg/issues/48)
- mask dma addresses so they cannot go OOB.
- impl basic bios open bus. fixes [#41](https://github.com/ITotalJustice/notorious_beeg/issues/41)
- fix invalid io reads. fixes [#50](https://github.com/ITotalJustice/notorious_beeg/issues/50)
- fix LR buttons being reversed.
- add 2D layout obj support. fixes [#37](https://github.com/ITotalJustice/notorious_beeg/issues/37)
- fix vrally 3 by not disabling interrupts when skipping bios. fixes [#53](https://github.com/ITotalJustice/notorious_beeg/issues/53)
- better impl backup save type searching (less strict with the string found, may cause issues!). fixes [#52](https://github.com/ITotalJustice/notorious_beeg/issues/52)
- do obj alpha blending if the alpha bit is set in oam.
- fixed loading alt bios.
- add 1 cycle penalty if game accesses pram/vram/oam whilst in hdraw.
- more accurately set registers for skipping bios.
- fix strb to hltcnt_l writing to both hltcnt_l hltcnt_h, causing invalid halt in official bios.
- move bios array to gba struct so that bios isn't included in savestate.
- clamp light/dark blend coeff to 16.
- init `REG_RCNT` when skipping bios. fixes [#67](https://github.com/ITotalJustice/notorious_beeg/issues/67)
- no loger mirror io r/w.
- force align dma r/w.
- add controller support to frontend.
- correctly restore r8-12 when leaving fiq. fixes [#72](https://github.com/ITotalJustice/notorious_beeg/issues/72)
- force bit4 of psr to be set. fixes [#44](https://github.com/ITotalJustice/notorious_beeg/issues/44)
- constify most of the code.
- start cpu in supervisor mode.
- fix interrupts not being enabled in scheduler build on cpsr=spsr and cpsr=u32. see [#54](https://github.com/ITotalJustice/notorious_beeg/issues/54), [#60](https://github.com/ITotalJustice/notorious_beeg/issues/60), [#58](https://github.com/ITotalJustice/notorious_beeg/issues/58) and [#47](https://github.com/ITotalJustice/notorious_beeg/issues/47).
- add rtc support [#36](https://github.com/ITotalJustice/notorious_beeg/issues/36).
- add rewind support.

0.0.2
- fixed neg flags being treated as logical flags, rather instead of sub. fixes [#1](https://github.com/ITotalJustice/notorious_beeg/issues/1)
- fixed thumb cmp imm doing an add instead with the oprands complemented. this would give the correct result but the flags would be wrong.
- sample at 65536hz. this is the max that *most* games want to sample at.
- correctly init soundbias on reset.
- change sample format to s16. this change is needed for 9bit depth samples (not yet impl).
- bundle normmatt's bios as default fallback if a bios isn't provided. addresses [#9](https://github.com/ITotalJustice/notorious_beeg/issues/9)
- fixed arm templatated builds by removing printf from constexpr function.
- heavily reduced number of templated functions generated for thumb [#11](https://github.com/ITotalJustice/notorious_beeg/issues/11). removed non-templated thumb option as compile times are still instant.
- heavily reduced number of templated functions generated for arm [#11](https://github.com/ITotalJustice/notorious_beeg/issues/11). removed non-templated arm option as compile times are still instant.
- fixed vram dma overflow bug, thanks to [mgba](https://github.com/mgba-emu/mgba/issues/743).
- hle CpuSet bios (untested, should work though).
- fixed 16/32bit sram region reads by mirroring the single. (ie, 16bit read of 0xFF will return 0xFFFF).
- fix `is_bitmap_mode()` to also include mode5
- refactor io_reads to operate on 16bit reads by default.
- minor refactor of mem.cpp, it's more readable now. io r/w is still a mess atm.
- remove `assert(gba.cpu.halted);` from scheduler. this was left over from debugging scheduler issues in the past.
- blank ppu rendering (memset the screen).
- add std::execution to mode3/mode4 rendering. seems to slightly improve fps (~10fps) but that's within margin of error, so unsure if it has any benefit. though it doesn't harm performance so i've kept it. (this forced my to remove -fno-exceptions as well).
- added save support. on startup, the save for a game will be loaded, on exit, the save will be dumped.
- per game savestates.
- fix sbc when `b` was u32max and flag was not set, causing an overflow to 0, so carry flag wouldnt be set [#13](https://github.com/ITotalJustice/notorious_beeg/issues/13).
- fix r/w handlers to mask the address down from full 32bit to 28bit.
- hacky mirroring for roms that are <= 16MiB, see [#12](https://github.com/ITotalJustice/notorious_beeg/issues/12).
- disable broken cpuset hle (breaks emerald intro).
- fix cpu being reset before mem. this meant that when the cpu flushed the pipeline in reset, it would try to read from memtable that hasnt been setup yet, so the opcode would be 0.
- ignored more reads to write-only IOregs.
- basic impl of mode0-mode1 tile based rendering.
- add memory access time struct for slightly more accurate timings. this means in most cases, games now run slower (in game), meaning the performance of the emulator itself greatly increased.
- fix edge case in scheduler where halt would be next event when firing all events, causing halt loop to run recursively until crash.
- add `frame` event which fires at the end of a frame. this way, only 1 timer needs to be ticked and only 1 variable needs to be tracked if the frame has ended.
- add imgui frontend.
- add sprite rendering.
- add bg layer viewer to frontend.

0.0.1
- added readme
- fixed scheduler so that if an event adds itself whilst its already the current next event, check the new cycles to see if they increased, if so, then find_next_event(). this is most common in apu channels when theyre triggered.
- kept delta of events in scheduler, fixed small popping in minish cap and emerald
- build option to toggle thumb|arm templating
- fixed emerald bad sound fifo (assuming it was timer delta mess up)
- only enable halt if bios writes to hltcnt (or hle swi halt)
- fixed savestates so that the memtables are re-setup and all events in scheduler have their function pointers set
- use sdl audio stream to allow for wanted changes to format/freq when init audio.
- fixed renaming of decode_template function in arm.cpp for templated builds, this would cause build errors for templated arm builds.

## broken
-

## passed
- armwrestler
- thumb.gba
- arm.gba
- memory.gba
- nes.gba

## works
- emerald
- minish cap
- metroid fusion
- metroid zero mission
- doom
- openlara

## done
- arm
- thumb
- ppu mode 0
- ppu mode 1
- ppu mode 3
- ppu mode 4
- apu (mostly, write only regs, no apu disabled, wrong fifo)
- dmas (mostly, no dma3 special)
- timers (mostly, no cascade)
- eeprom
- sram
- rtc

## not impl
- dma3 special
- unused bits
- unused region read
- bios read (when pc is not in bios)
- proper r/w timing access
- proper fifo <https://github.com/mgba-emu/mgba/issues/1847>
- obj in obj window
- obj not in obj window
- bg affine
- obj affine
- obj mosaic
- window wrapping
- obj 4bpp

## misc
- doom runs at 13k fps (title) 800 fps (in-game)
- emerald runs at 600 fps (intro) 600-700 fps (in-game)
- minish runs at 1.7k fps (title) 1.1k fps (in-game)
- openlara runs at 1.3k fps (title) 800 fps (in-game)
- metroid fusion 1.5k fps (title) 700-800 fps (in-game)
- metroid zero 1.5k fps (title) 700-800 fps (in-game)

## todo
- better optimise vram r/w (somehow adjust the masking so it always works).
- refactor code. make functions into methods where possible (and if it makes sense).

## optimisations (todo)
- further reduce template bloat for arm/thumb instructions. this will help for platforms that have a small-ish icache.
- faster path for apu sampling, ie, if channel is disabled, don't sample it. if all dmg channels will output zero, stop sampling further.
- measure dmg apu channels on scheduler VS ticking them on sample(), likely it's faster to tick on sample().
- batch samples. basically, on sample callback, save everything needed (about 8 bytes) to an array. after X amount of entries added, process all samples at once. this can be processed on the audio thread (frontend) or still in emu core.
- impl computed goto. along side this (or first), impl switch() version as well. measure all 3 versions!
- prioritise ewram access first as thats the most common path.
- check for fast paths in dma's. check if they can be memcpy (doesnt cross boundries) / memset (if the src addr doesnt change). cache this check if R (repeat) is set, so on next dma fire, we know its fast path already.
- measure different impl of mem r/w. current impl uses fastmem, with a fallback to a function table. should be very fast, but maybe the indirection of all r/w access is too slow.

## optimisations that worked
- fastmem for reads. ie, array of pointers to arrays.
- adding [[likely]] to fastmen reads, this gave ~200fps in openlara.
- scheduler. HUGE fps increase.
- halt skipping. when in halt, fast forward to the next event in a loop.

## optimisations that didn't work
- tick arm/thumb in a loop. break out of this loop only on new_frame_event or state changed. this avoids the switch(state) on each ittr.
- switch instead of table, switch was slower (see table_switch_goto branch).
- computed goto instead of table, computed goto was slower (see table_switch_goto branch).
- array of functions pointers for reads. this was much slower than having 2 arrays, 1 of pointers, one of functions.

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
