# gba emu
v0.0.1-dev

gba emulator witten in c++23.

---

## changelog
0.0.2
- fixed neg flags being treated as logical flags, rather instead of sub. fixes <https://github.com/ITotalJustice/notorious_beeg/issues/1>

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

## done
- arm
- thumb
- ppu mode 3
- ppu mode 4
- apu (mostly, write only regs, no apu disabled, wrong fifo)
- dmas (mostly, no dma3 special)
- timers (mostly, no cascade)
- eeprom
- sram

## not impl
- flash
- dma3 special
- timer cascade
- dumping saves
- all non-bitmap modes
- apu writes ignored on apu disabled
- unused bits
- unused region read
- bios read (when pc is not in bios)
- delayed dma
- proper r/w timing access
- soundbias
- proper fifo <https://github.com/mgba-emu/mgba/issues/1847>

## misc
- doom runs at 17k fps (title) 750-800 fps (in game)
- emerald runs at 700-800 fps (intro)
- minish runs at 3k fps (title) 800 fps (in game)
- openlara runs at 800-900 fps (title)
- metroid fusion 2k fps (title)
- metroid zero 3k fps (title) 2-3k fps (in game)

## todo
- reduce template bloat for arm/thumb instructions by creating dedicated functions for every possible version, ie, data_proc_add_imm_s. this will GREATLY reduce bloat, which is nicer for icache and also compile times. to achive this, still have normal template function, such as data_proc, so data_proc_add_imm_s will call that template func. the template<> will have to be toggled by a macro so that debug builds can still be fast (instant) and not templated at all.
- better optimise vram r/w (somehow adjust the masking so it always works).
- refcator code. make functions into methods where possible (and if it makes sense).

## thanks
- cowbite spec <https://www.cs.rit.edu/~tjh8300/CowBite/CowBiteSpec.htm>
- gbatek <https://problemkaputt.de/gbatek.htm>
- tonc <https://www.coranac.com/tonc/text/toc.htm>
- belogic for apu <http://belogic.com/gba/>
- dillion for flashmem <https://dillonbeliveau.com/2020/06/05/GBA-FLASH.html>
- dennis for eeprom <https://densinh.github.io/DenSinH/emulation/2021/02/01/gba-eeprom.html>
- emudev discord