cmake_minimum_required(VERSION 3.20.0)

project(testing LANGUAGES CXX)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        af1a5bc
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(stb)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

add_executable(testing test_framework.cpp)

target_link_libraries(testing PRIVATE frontend_base)
target_link_libraries(testing PRIVATE stb)

set_target_properties(testing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)


set(TEST_ROM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/roms_and_output)

################
## gba-tests/ ##
################
add_test(
    NAME "gba-tests/arm/arm"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/arm/arm.zip" "240" "0"
)
# add_test(
#     NAME "gba-tests/bios/bios"
#     COMMAND testing "${TEST_ROM_DIR}/gba-tests/bios/bios.zip" "240" "0"
# )
add_test(
    NAME "gba-tests/memory/memory"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/memory/memory.zip" "240" "0"
)
add_test(
    NAME "gba-tests/nes/nes"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/nes/nes.zip" "240" "0"
)
add_test(
    NAME "gba-tests/ppu/hello"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/ppu/hello.zip" "240" "0"
)
add_test(
    NAME "gba-tests/ppu/shades"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/ppu/shades.zip" "240" "0"
)
add_test(
    NAME "gba-tests/ppu/stripes"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/ppu/stripes.zip" "240" "0"
)
add_test(
    NAME "gba-tests/save/flash128"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/save/flash128.zip" "240" "0"
)
add_test(
    NAME "gba-tests/save/none"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/save/none.zip" "240" "0"
)
add_test(
    NAME "gba-tests/save/sram"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/save/sram.zip" "240" "0"
)
add_test(
    NAME "gba-tests/thumb/thumb"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/thumb/thumb.zip" "240" "0"
)
add_test(
    NAME "gba-tests/unsafe/unsafe"
    COMMAND testing "${TEST_ROM_DIR}/gba-tests/unsafe/unsafe.zip" "240" "0"
)


#################################################
## armwrestler-gba-fixed/armwrestler-gba-fixed ##
#################################################
add_test(
    NAME "armwrestler-gba-fixed/armwrestler-gba-fixed-00-menu"
    COMMAND testing "${TEST_ROM_DIR}/armwrestler-gba-fixed/armwrestler-gba-fixed.zip" "15" "0" "00-menu"
)
add_test(
    NAME "armwrestler-gba-fixed/armwrestler-gba-fixed-01-alu_tests_part_1"
    COMMAND testing "${TEST_ROM_DIR}/armwrestler-gba-fixed/armwrestler-gba-fixed.zip" "76" "0" "01-alu_tests_part_1" "START;61"
)
add_test(
    NAME "armwrestler-gba-fixed/armwrestler-gba-fixed-02-alu_pt_2_misc"
    COMMAND testing "${TEST_ROM_DIR}/armwrestler-gba-fixed/armwrestler-gba-fixed.zip" "91" "0" "02-alu_pt_2_misc" "START;61;START;76"
)
add_test(
    NAME "armwrestler-gba-fixed/armwrestler-gba-fixed-03-load_tests_part_1"
    COMMAND testing "${TEST_ROM_DIR}/armwrestler-gba-fixed/armwrestler-gba-fixed.zip" "106" "0" "03-load_tests_part_1" "START;61;START;76;START;91"
)
add_test(
    NAME "armwrestler-gba-fixed/armwrestler-gba-fixed-04-load_tests_part_2"
    COMMAND testing "${TEST_ROM_DIR}/armwrestler-gba-fixed/armwrestler-gba-fixed.zip" "121" "0" "04-load_tests_part_2" "START;61;START;76;START;91;START;106"
)
add_test(
    NAME "armwrestler-gba-fixed/armwrestler-gba-fixed-05-ldm_stm_tests_1"
    COMMAND testing "${TEST_ROM_DIR}/armwrestler-gba-fixed/armwrestler-gba-fixed.zip" "136" "0" "05-ldm_stm_tests_1" "START;61;START;76;START;91;START;106;START;121"
)


############
## suite/ ##
############
add_test(
    NAME "00-menu"
    COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "60" "0" "00-menu"
)
# add_test(
#     NAME "01-memory-tests"
#     COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "120" "0" "01-memory-tests" "A;15"
# )
add_test(
    NAME "02-io-read-tests"
    COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "120" "0" "02-io-read-tests" "DOWN;15;A;30"
)
# add_test(
#     NAME "03-timing-tests"
#     COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "480" "0" "03-timing-tests" "DOWN;15;DOWN;30;A;90"
# )
# add_test(
#     NAME "04-timer-count-up-tests"
#     COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "240" "0" "04-timer-count-up-tests" "DOWN;15;DOWN;30;DOWN;45;A;60"
# )
# add_test(
#     NAME "05-timer-irq-tests"
#     COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "120" "0" "05-timer-irq-tests" "DOWN;15;DOWN;30;DOWN;45;DOWN;60;A;75"
# )
add_test(
    NAME "06-carry-tests"
    COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "120" "0" "06-carry-tests" "DOWN;15;DOWN;30;DOWN;45;DOWN;60;DOWN;75;A;90"
)
add_test(
    NAME "07-shifter-tests"
    COMMAND testing "${TEST_ROM_DIR}/suite/suite.zip" "120" "0" "07-shifter-tests" "DOWN;15;DOWN;30;DOWN;45;DOWN;60;DOWN;75;DOWN;90;A;105"
)


##################
## hw-test/ppu/ ##
##################
add_test(
    NAME "hw-test/ppu/vram-mirror/vram-mirror"
    COMMAND testing "${TEST_ROM_DIR}/hw-test/ppu/vram-mirror/vram-mirror.zip" "120" "0"
)


##############
## other/ ##
##############
add_test(
    NAME "beeg/beeg"
    COMMAND testing "${TEST_ROM_DIR}/beeg/beeg.zip" "60" "0"
)
add_test(
    NAME "yoshi_dma/yoshi_dma"
    COMMAND testing "${TEST_ROM_DIR}/yoshi_dma/yoshi_dma.zip" "60" "0"
)


#################
## cpu_instrs/ ##
#################
add_test(
    NAME "cpu_instrs/01-special"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/01-special.zip" "240" "0"
)
add_test(
    NAME "cpu_instrs/02-interrupts"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/02-interrupts.zip" "60" "0"
)
add_test(
    NAME "cpu_instrs/03-op sp,hl"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/03-op sp,hl.zip" "240" "0"
)
add_test(
    NAME "cpu_instrs/04-op r,imm"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/04-op r,imm.zip" "240" "0"
)
add_test(
    NAME "cpu_instrs/05-op rp"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/05-op rp.zip" "240" "0"
)
add_test(
    NAME "cpu_instrs/06-ld r,r"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/06-ld r,r.zip" "60" "0"
)
add_test(
    NAME "cpu_instrs/07-jr,jp,ret,rst"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/07-jr,jp,call,ret,rst.zip" "60" "0"
)
add_test(
    NAME "cpu_instrs/08-misc instrs"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/08-misc instrs.zip" "60" "0"
)
add_test(
    NAME "cpu_instrs/09-op r,r"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/09-op r,r.zip" "480" "0"
)
add_test(
    NAME "cpu_instrs/10-bit ops"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/10-bit ops.zip" "480" "0"
)
add_test(
    NAME "cpu_instrs/11-op a,(hl)"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/11-op a,(hl).zip" "600" "0"
)
add_test(
    NAME "cpu_instrs/cpu_instrs"
    COMMAND testing "${TEST_ROM_DIR}/cpu_instrs/cpu_instrs.zip" "1800" "0"
)


##############
## oam_bug/ ##
##############
add_test(
    NAME "oam_bug/1-lcd_sync"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/1-lcd_sync.zip" "60" "0"
)
add_test( # fails on cgb/agb
    NAME "oam_bug/2-causes"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/2-causes.zip" "60" "0"
)
add_test(
    NAME "oam_bug/3-non_causes"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/3-non_causes.zip" "120" "1"
)
add_test( # fails on cgb/agb
    NAME "oam_bug/4-scanline_timing"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/4-scanline_timing.zip" "60" "0"
)
add_test( # fails on cgb/agb
    NAME "oam_bug/5-timing_bug"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/5-timing_bug.zip" "60" "0"
)
add_test(
    NAME "oam_bug/6-timing_no_bug"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/6-timing_no_bug.zip" "120" "1"
)
add_test( # fails on cgb/agb
    NAME "oam_bug/7-timing_effect"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/7-timing_effect.zip" "480" "1"
)
add_test( # fails on cgb/agb
    NAME "oam_bug/8-instr_effect"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/8-instr_effect.zip" "60" "0"
)
add_test(
    NAME "oam_bug/oam_bug"
    COMMAND testing "${TEST_ROM_DIR}/oam_bug/oam_bug.zip" "1000" "0"
)


#####################
## mts/acceptance/ ##
#####################
add_test(
    NAME "mts/acceptance/boot_div2-S"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_div2-S.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_div-dmg0"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_div-dmg0.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_div-dmgABCmgb"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_div-dmgABCmgb.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_div-S"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_div-S.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_hwio-cgb-dmg"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_hwio-cgb-dmg.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_hwio-cgb"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_hwio-cgb.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_hwio-dmg0"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_hwio-dmg0.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_hwio-dmgABCmgb"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_hwio-dmgABCmgb.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_hwio-S"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_hwio-S.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_regs-dmg0"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_regs-dmg0.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_regs-dmgABC"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_regs-dmgABC.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_regs-mgb"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_regs-mgb.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_regs-sgb2"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_regs-sgb2.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/boot_regs-sgb"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/boot_regs-sgb.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/call_cc_timing2"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/call_cc_timing2.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/call_cc_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/call_cc_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/call_timing2"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/call_timing2.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/call_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/call_timing.zip" "240" "0"
# )
add_test(
    NAME "mts/acceptance/di_timing"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/di_timing-GS.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/div_timing"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/div_timing.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/ei_sequence"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ei_sequence.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/ei_timing"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ei_timing.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/halt_ime0_ei"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/halt_ime0_ei.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/halt_ime0_nointr_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/halt_ime0_nointr_timing.zip" "240" "0"
# )
add_test(
    NAME "mts/acceptance/halt_ime1_timing2"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/halt_ime1_timing2-GS.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/halt_ime1_timing"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/halt_ime1_timing.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/if_ie_registers"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/if_ie_registers.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/intr_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/intr_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/jp_cc_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/jp_cc_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/jp_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/jp_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ld_hl_sp_e_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ld_hl_sp_e_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/oam_dma_restart"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/oam_dma_restart.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/oam_dma_start"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/oam_dma_start.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/oam_dma_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/oam_dma_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/pop_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/pop_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/push_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/push_timing.zip" "240" "0"
# )
add_test(
    NAME "mts/acceptance/rapid_di_ei"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/rapid_di_ei.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/ret_cc_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ret_cc_timing.zip" "240" "0"
# )
add_test(
    NAME "mts/acceptance/reti_intr_timing"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/reti_intr_timing.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/reti_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/reti_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ret_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ret_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/rst_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/rst_timing.zip" "240" "0"
# )


##########################
## mts/acceptance/bits/ ##
##########################
add_test(
    NAME "mts/acceptance/bits/mem_oam"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/bits/mem_oam.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/bits/reg_f"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/bits/reg_f.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/bits/unused_hwio-GS"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/bits/unused_hwio-GS.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/instr/daa"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/instr/daa.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/interrupts/ie_push"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/interrupts/ie_push.zip" "240" "0"
)


#############################
## mts/acceptance/oam_dma/ ##
#############################
add_test(
    NAME "mts/acceptance/oam_dma/basic"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/oam_dma/basic.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/oam_dma/reg_read"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/oam_dma/reg_read.zip" "240" "0"
)
# shouldn't pass as i am emulating agb
# add_test(
#     NAME "mts/acceptance/oam_dma/sources-GS"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/oam_dma/sources-GS.zip" "240" "0"
# )


#########################
## mts/acceptance/ppu/ ##
#########################
# A=00, F=40, B=42, C=00, D=41, E=42, H=FF, L=44
# add_test(
#     NAME "mts/acceptance/ppu/hblank_ly_scx_timing-GS"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/hblank_ly_scx_timing-GS.zip" "240" "0"
# )
# A=00, F=00, B=14, C=00, D=14, E=14, H=FF, L=41
# add_test(
#     NAME "mts/acceptance/ppu/intr_1_2_timing-GS"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/intr_1_2_timing-GS.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ppu/intr_2_0_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/intr_2_0_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ppu/intr_2_mode0_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/intr_2_mode0_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ppu/intr_2_mode0_timing_sprites"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/intr_2_mode0_timing_sprites.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ppu/intr_2_mode3_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/intr_2_mode3_timing.zip" "240" "0"
# )
# add_test(
#     NAME "mts/acceptance/ppu/intr_2_oam_ok_timing"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/intr_2_oam_ok_timing.zip" "240" "0"
# )
# add_test( # CGB: STAT LYC=0, Cycle=$6F, Expected=$80, Actual=$64
#     NAME "mts/acceptance/ppu/lcdon_timing-GS"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/lcdon_timing-GS.zip" "240" "0"
# )
# add_test( # CGB: OAM Write, Cycle=$6F, Expected=$81, Actual=$00
#     NAME "mts/acceptance/ppu/lcdon_write_timing-GS"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/lcdon_write_timing-GS.zip" "240" "0"
# )
add_test(
    NAME "mts/acceptance/ppu/stat_irq_blocking"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/stat_irq_blocking.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/ppu/stat_lyc_onoff"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/stat_lyc_onoff.zip" "240" "0"
)
# A=01, F=80, B=01, C=00, D=00, E=00, H=FF, L=96
# add_test(
#     NAME "mts/acceptance/ppu/vblank_stat_intr-GS"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/ppu/vblank_stat_intr-GS.zip" "240" "0"
# )


############################
## mts/acceptance/serial/ ##
############################
# add_test(
#     NAME "mts/acceptance/serial/boot_sclk_align-dmgABCmgb"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/serial/boot_sclk_align-dmgABCmgb.zip" "240" "0"
# )


###########################
## mts/acceptance/timer/ ##
###########################
add_test(
    NAME "mts/acceptance/timer/div_write"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/div_write.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/timer/rapid_toggle"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/rapid_toggle.zip" "240" "0"
# )
add_test(
    NAME "mts/acceptance/timer/tim00_div_trigger"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim00_div_trigger.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim00"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim00.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim01_div_trigger"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim01_div_trigger.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim01"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim01.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim10_div_trigger"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim10_div_trigger.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim10"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim10.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim11_div_trigger"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim11_div_trigger.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tim11"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tim11.zip" "240" "0"
)
add_test(
    NAME "mts/acceptance/timer/tima_reload"
    COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tima_reload.zip" "240" "0"
)
# add_test(
#     NAME "mts/acceptance/timer/tima_write_reloading"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tima_write_reloading.zip" "240" "01"
# )
# add_test(
#     NAME "mts/acceptance/timer/tma_write_reloading"
#     COMMAND testing "${TEST_ROM_DIR}/mts/acceptance/timer/tma_write_reloading.zip" "240" "0"
# )


#############################
## mts/emulator-only/mbc1/ ##
#############################
add_test(
    NAME "mts/emulator-only/mbc1/bits_bank1"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/bits_bank1.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/bits_bank2"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/bits_bank2.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/bits_mode"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/bits_mode.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/bits_ramg"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/bits_ramg.zip" "400" "0"
)
# add_test(
#     NAME "mts/emulator-only/mbc1/multicart_rom_8Mb"
#     COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/multicart_rom_8Mb.zip" "240" "0"
# )
add_test(
    NAME "mts/emulator-only/mbc1/ram_256kb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/ram_256kb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/ram_64kb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/ram_64kb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/rom_16Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/rom_16Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/rom_1Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/rom_1Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/rom_2Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/rom_2Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/rom_4Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/rom_4Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/rom_512kb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/rom_512kb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc1/rom_8Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc1/rom_8Mb.zip" "240" "0"
)


#############################
## mts/emulator-only/mbc2/ ##
#############################
add_test(
    NAME "mts/emulator-only/mbc2/bits_ramg"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/bits_ramg.zip" "400" "0"
)
add_test(
    NAME "mts/emulator-only/mbc2/bits_romb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/bits_romb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc2/bits_unused"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/bits_unused.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc2/ram"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/ram.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc2/rom_1Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/rom_1Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc2/rom_2Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/rom_2Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc2/rom_512kb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc2/rom_512kb.zip" "240" "0"
)


#############################
## mts/emulator-only/mbc5/ ##
#############################
add_test(
    NAME "mts/emulator-only/mbc5/rom_16Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_16Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_1Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_1Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_2Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_2Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_32Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_32Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_4Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_4Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_512kb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_512kb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_64Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_64Mb.zip" "240" "0"
)
add_test(
    NAME "mts/emulator-only/mbc5/rom_8Mb"
    COMMAND testing "${TEST_ROM_DIR}/mts/emulator-only/mbc5/rom_8Mb.zip" "240" "0"
)


###############
## mts/misc/ ##
###############
# add_test(
#     NAME "mts/misc/boot_div-A"
#     COMMAND testing "${TEST_ROM_DIR}/mts/misc/boot_div-A.zip" "240" "0"
# )
# add_test(
#     NAME "mts/misc/boot_div-cgb0"
#     COMMAND testing "${TEST_ROM_DIR}/mts/misc/boot_div-cgb0.zip" "240" "0"
# )
# add_test(
#     NAME "mts/misc/boot_div-cgbABCDE"
#     COMMAND testing "${TEST_ROM_DIR}/mts/misc/boot_div-cgbABCDE.zip" "240" "0"
# )
add_test(
    NAME "mts/misc/boot_hwio-C"
    COMMAND testing "${TEST_ROM_DIR}/mts/misc/boot_hwio-C.zip" "240" "0"
)
add_test(
    NAME "mts/misc/boot_regs-A"
    COMMAND testing "${TEST_ROM_DIR}/mts/misc/boot_regs-A.zip" "240" "0"
)
add_test(
    NAME "mts/misc/boot_regs-cgb"
    COMMAND testing "${TEST_ROM_DIR}/mts/misc/boot_regs-cgb.zip" "240" "0"
)


####################
## mts/misc/bits/ ##
####################
add_test(
    NAME "mts/misc/bits/unused_hwio"
    COMMAND testing "${TEST_ROM_DIR}/mts/misc/bits/unused_hwio-C.zip" "240" "0"
)


###################
## mts/misc/ppu/ ##
###################
add_test(
    NAME "mts/misc/ppu/vblank_stat_intr"
    COMMAND testing "${TEST_ROM_DIR}/mts/misc/ppu/vblank_stat_intr-C.zip" "240" "0"
)
