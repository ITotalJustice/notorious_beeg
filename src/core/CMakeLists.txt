cmake_minimum_required(VERSION 3.18.0)

set(SINGLE_FILE OFF)
set(ENABLE_SCHEDULER ON)

if (SINGLE_FILE)
    add_library(GBA single.cpp)
else ()
    add_library(GBA
        gba.cpp
        ppu.cpp
        mem.cpp
        dma.cpp
        timer.cpp
        apu/apu.cpp
        bios_hle.cpp
        scheduler.cpp

        backup/backup.cpp
        backup/eeprom.cpp
        backup/flash.cpp
        backup/sram.cpp

        arm7tdmi/arm7tdmi.cpp
        arm7tdmi/arm/arm.cpp
        arm7tdmi/thumb/thumb.cpp

        arm7tdmi/arm/branch.cpp
        arm7tdmi/arm/data_processing.cpp
        arm7tdmi/arm/halfword_data_transfer.cpp
        arm7tdmi/arm/single_data_transfer.cpp
        arm7tdmi/arm/block_data_transfer.cpp
        arm7tdmi/arm/multiply.cpp
        arm7tdmi/arm/software_interrupt.cpp
        arm7tdmi/arm/branch_and_exchange.cpp
        arm7tdmi/arm/multiply_long.cpp
        arm7tdmi/arm/single_data_swap.cpp

        arm7tdmi/thumb/move_compare_add_subtract_immediate.cpp
        arm7tdmi/thumb/hi_register_operations.cpp
        arm7tdmi/thumb/load_address.cpp
        arm7tdmi/thumb/conditional_branch.cpp
        arm7tdmi/thumb/unconditional_branch.cpp
        arm7tdmi/thumb/move_shifted_register.cpp
        arm7tdmi/thumb/add_subtract.cpp
        arm7tdmi/thumb/long_branch_with_link.cpp
        arm7tdmi/thumb/alu_operations.cpp
        arm7tdmi/thumb/add_offset_to_stack_pointer.cpp
        arm7tdmi/thumb/pc_relative_load.cpp
        arm7tdmi/thumb/load_store_with_register_offset.cpp
        arm7tdmi/thumb/load_store_sign_extended_byte_halfword.cpp
        arm7tdmi/thumb/load_store_with_immediate_offset.cpp
        arm7tdmi/thumb/load_store_halfword.cpp
        arm7tdmi/thumb/sp_relative_load_store.cpp
        arm7tdmi/thumb/push_pop_registers.cpp
        arm7tdmi/thumb/multiple_load_store.cpp
        arm7tdmi/thumb/software_interrupt.cpp
    )
endif()

target_include_directories(GBA PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_add_common_cflags(GBA PRIVATE)

# enable sanitizer_flags
if (GBA_DEV)
    list(APPEND sanitizer_flags
        -fsanitize=address
        -fsanitize=leak
        -fsanitize=undefined
        -fsanitize=pointer-compare
        -fsanitize=pointer-subtract
        -fsanitize=thread
        -fstack-protector-all
    )

    target_add_compiler_flags(GBA sanitizer_flags PUBLIC)
    target_add_linker_flags(GBA sanitizer_flags PUBLIC)
endif()

target_compile_definitions(GBA PRIVATE
    GBA_DEBUG=$<BOOL:${GBA_DEBUG}>
    SINGLE_FILE=$<BOOL:${SINGLE_FILE}>
    ENABLE_SCHEDULER=$<BOOL:${ENABLE_SCHEDULER}>
)

# enable LTO (only in release builds)
# this greatly reduces template bloat as well
# if ($<CONFIG:Release>)
    # include(CheckIPOSupported)
    # check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

    # if (ipo_supported)
    #     message(STATUS "IPO / LTO enabled")
    #     set_property(TARGET GBA PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    # else()
    #     message(STATUS "IPO / LTO not supported: <${ipo_error}>")
    # endif()
# else()
    # message(STATUS "no ipo")
# endif()
