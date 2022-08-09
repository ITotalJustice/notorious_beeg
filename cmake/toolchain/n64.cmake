# to set your own path, use -DLIBDRAGON=/path/to/libragon
if (NOT LIBDRAGON)
    set(LIBDRAGON "/opt/libdragon") # this is my default
endif()

set(LIBDRAGON_LIBS "-ldragon -lc -lm -ldragonsys") # order matters
set(LIBDRAGON_LINK_FILE "${LIBDRAGON}/mips64-elf/lib/n64.ld")

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_SYSTEM_PROCESSOR mips)

SET(CMAKE_C_COMPILER ${LIBDRAGON}/bin/mips64-elf-gcc)
SET(CMAKE_CXX_COMPILER ${LIBDRAGON}/bin/mips64-elf-g++)
set(CMAKE_LINKER ${LIBDRAGON}/bin/mips64-elf-ld)
set(CMAKE_AR ${LIBDRAGON}/bin/mips64-elf-gcc-ar)
set(CMAKE_OBJCOPY ${LIBDRAGON}/bin/mips64-elf-objcopy)
set(CMAKE_STRIP ${LIBDRAGON}/bin/mips64-elf-strip)
set(CMAKE_ADDR2LINE ${LIBDRAGON}/bin/mips64-elf-addr2line)
set(CMAKE_RANLIB ${LIBDRAGON}/bin/mips64-elf-gcc-ranlib)
set(CMAKE_NM ${LIBDRAGON}/bin/mips64-elf-gcc-nm)

SET(CMAKE_C_FLAGS_INIT "-I${LIBDRAGON}/mips64-elf/include -march=vr4300 -mtune=vr4300 -ffunction-sections -fdata-sections")
SET(CMAKE_CXX_FLAGS_INIT "-I${LIBDRAGON}/mips64-elf/include -march=vr4300 -mtune=vr4300 -ffunction-sections -fdata-sections")
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-L${LIBDRAGON}/lib -L${LIBDRAGON}/mips64-elf/lib")

# used in cmake 3.20+
set(CMAKE_C_BYTE_ORDER BIG_ENDIAN)
set(CMAKE_CXX_BYTE_ORDER BIG_ENDIAN)

SET(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
SET(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")

SET(CMAKE_FIND_ROOT_PATH ${LIBDRAGON} ${LIBDRAGON}/mips64-elf)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# for convinience sake
link_libraries(${LIBDRAGON_LIBS})

add_link_options(
    -T${LIBDRAGON_LINK_FILE}
    -Wl,--wrap=__do_global_ctors # asserts otherwise
    -Wl,--gc-sections
)

# find programs, will fatal if not found
find_program(N64TOOL NAMES n64tool HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(MKSPRITE NAMES mksprite HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(MKDFS NAMES mkdfs HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(E64ROMCONFIG NAMES ed64romconfig HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(DUMPDFS NAMES dumpdfs HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(CONVTOOL NAMES convtool HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(CHKSUM64 NAMES chksum64 HINTS ${LIBDRAGON}/bin REQUIRED)
find_program(AUDIOCONV64 NAMES audioconv64 HINTS ${LIBDRAGON}/bin REQUIRED)

add_definitions("-DN64 -D__N64__")
SET(PLATFORM_N64 TRUE)
SET(N64 TRUE)

# helper to create dfs archive from a folder
function(n64_create_dfs folder outfile)
    add_custom_command(
        OUTPUT ${outfile}
        COMMAND ${MKDFS} ${outfile} ${folder}
        DEPENDS ${folder}
        VERBATIM
    )

    add_custom_target(${outfile}_dfs ALL DEPENDS ${outfile})
endfunction()

# helper to create .z64 rom
function(n64_create_z64 target)
    cmake_parse_arguments(Z64 "" "NAME;HEADER;SIZE;DFS" "" ${ARGN})

    set(Z64_DEPS ${target})

    if (NOT DEFINED Z64_NAME)
        set(Z64_NAME ${target})
	endif()

    if (DEFINED Z64_HEADER)
        list(APPEND Z64_DEPS ${Z64_HEADER})
        set(Z64_HEADER ${Z64_HEADER})
    else()
		set(Z64_HEADER ${LIBDRAGON}/mips64-elf/lib/header)
	endif()

    if (DEFINED Z64_SIZE)
		set(Z64_SIZE "-l" "${Z64_SIZE}")
	endif()

    if (DEFINED Z64_DFS)
        list(APPEND Z64_DEPS ${Z64_DFS})
        set(Z64_DFS "-s" "1M" "${Z64_DFS}")
    endif()

    message(STATUS "COMMAND: ${N64TOOL} -t ${Z64_NAME} -h ${Z64_HEADER} ${Z64_SIZE} -o ${target}.z64 ${target}.bin ${Z64_DFS}")

    add_custom_command(OUTPUT ${Z64_NAME}.z64
        COMMAND ${CMAKE_OBJCOPY} ${target}.elf ${target}.bin -O binary
        COMMAND ${N64TOOL} -t ${Z64_NAME} -h ${Z64_HEADER} ${Z64_SIZE} -o ${target}.z64 ${target}.bin ${Z64_DFS}
        COMMAND ${CHKSUM64} ${target}.z64
        DEPENDS ${Z64_DEPS}
        VERBATIM
    )

    add_custom_target(${target}_z64 ALL DEPENDS ${target}.z64)
endfunction()
