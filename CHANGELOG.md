## 0.0.3

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
- added web port.
- fixed emulator being little endian based, now works on big endian.
- add dmg/gbc support
- fix gb tima reload being delayed by 4 cycles
- fix gb tima not being clocked on div write when wanted bit is falling edge
- fix apu frame sequencer timing
- fix gba vram mirroring read value when reading whilst in bitmap mode
- fix gba sram not being mirrored to 0xF region
- fix gb stat_line being incorrectly reset on lcd_disable
- fix gb lyc incorrectly being compared whilst lcd is disabled
- fix gb mbc2 ram r/w when using raw pointer tables
- fix gb mbc1 missing call to update_ram_banks() after updating the modes
- fix gb building in non sched mode
- fix gbc wram bank1 not being memset to 0 (which is what the bios does)
- fix gb startup io reg values
- fix gb startup cpu reg values
- fix gb dma not returning the last written value
- fix gb ppu mode on lcd enable. stat reads mode as 0 until the mode is update (~80 cycles).
- fix gb ppu not skipping the first frame of the lcd being enabled.
- fix gbc palette data register being accessible during mode3, access is only allowed in mode0,1,2
- fix gba halt skipping to only set scheduler cycles to event cycles if event cycles is >= than scheduler cycles
- fix gba wave ram reads.
- add back halt bug in gb cpu (still need to test if this occurs on agb).
- add r/w function tables to gb to replace switch. this is how i handle it in gba code as well
- add ctest support along with automated testing via github
- remove std::ranges from core

---

## 0.0.2

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

---

## 0.0.1
- added readme
- fixed scheduler so that if an event adds itself whilst its already the current next event, check the new cycles to see if they increased, if so, then find_next_event(). this is most common in apu channels when theyre triggered.
- kept delta of events in scheduler, fixed small popping in minish cap and emerald
- build option to toggle thumb|arm templating
- fixed emerald bad sound fifo (assuming it was timer delta mess up)
- only enable halt if bios writes to hltcnt (or hle swi halt)
- fixed savestates so that the memtables are re-setup and all events in scheduler have their function pointers set
- use sdl audio stream to allow for wanted changes to format/freq when init audio.
- fixed renaming of decode_template function in arm.cpp for templated builds, this would cause build errors for templated arm builds.