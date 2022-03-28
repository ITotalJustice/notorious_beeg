cmake_minimum_required(VERSION 3.20.0)

# (C)flags for each compiler
# Werror is disabled atm because it's very annoying early into a project
list(APPEND gcc_flags
    -Wall
    -Wextra
    # -Werror
    -Wfatal-errors
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
    -Wformat-overflow=2
    -Wformat-truncation=1
    -Wundef
    -Wmissing-include-dirs
    -fstrict-aliasing
    -Wstrict-overflow=5
    -Walloca
    -Wduplicated-branches
    -Wduplicated-cond
    -Wwrite-strings
    -Wdate-time
    -Wlogical-op
    -Wpacked
    # -Wredundant-decls
    -Wnested-externs
    -Wcast-qual
    -Wcast-align
    -Wunused-macros
    -Wbad-function-cast
    # -Wsuggest-attribute=const
    -Wimplicit-fallthrough=5
    -Wsuggest-final-types
    -Wuninitialized

    -Wno-unused-parameter

    -fimplicit-constexpr
    -Wmissing-requires
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND gcc_flags -Og) # always enable otherwise debug builds would be too slow
else()
    list(APPEND gcc_flags -fno-rtti -Ofast)
endif()

list(APPEND clang_flags
    -Wall
    -Wextra
    # -Werror
    -Wfatal-errors
    -Wpedantic
    -Wshadow
    -Wdouble-promotion
    -Wformat=2
    -Wundef
    -Wmissing-include-dirs
    -Wstrict-aliasing=3
    -Wstrict-overflow=5
    -Walloca
    -Wwrite-strings
    -Wdate-time
    -Wpacked
    # -Wredundant-decls
    -Wnested-externs
    -Wcast-qual
    -Wcast-align
    -Wunused-macros
    -Wreserved-id-macro
    -Wbad-function-cast
    -Wbitwise-op-parentheses
    -Wbitfield-enum-conversion
    -Wno-missing-braces # false positive

    -Wno-unused-parameter
    -Wno-unused-variable
    # -Wno-gnu-case-range
    -Wno-c99-designator

    # can try enabling this just to see what breaks (still compiles as of clang 11)
    # be sure to enable (well, disable) the below flags else a *lot* of "errors"
    # -Weverything
    # -Wno-conversion
    # -Wno-sign-conversion
    # -Wno-missing-prototypes
    # -Wno-padded
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND clang_flags -Og) # always enable otherwise debug builds would be too slow
else()
    list(APPEND clang_flags -fno-rtti -Ofast)
endif()

list(APPEND tcc_flags
    -Wall
    # -Werror
    -Wwrite-strings
    -Wunsupported
)

list(APPEND msvc_flags
    /W4
    # /WX
)

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

function(target_add_compiler_flag target flag scope)
    # check_cxx_compiler_flag() adds a macro to the test.c file such that
    # it'll look like # will be shown as "#define COMPILER_SUPPORTS_-Wdouble-promotion"
    # which is not a valid macro! because of this, we have to remove all invalid characters
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" check_flag ${flag})
    check_cxx_compiler_flag(${flag} CHECK_FLAG_${check_flag})

    if (CHECK_FLAG_${check_flag})
        target_compile_options(${target} ${scope} ${flag})
        message(STATUS "has compiler flag ${flag}")
    else()
    message(STATUS "does NOT have compiler flag ${flag}")
    endif()
endfunction()

function(target_add_linker_flag target flag scope)
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" check_flag ${flag})
    check_linker_flag("CXX" ${flag} CHECK_FLAG_${check_flag})

    if (CHECK_FLAG_${check_flag})
        target_link_options(${target} ${scope} ${flag})
        message(STATUS "has linker flag ${flag}")
    else()
        message(STATUS "does NOT have linker flag ${flag}")
    endif()
endfunction()

function(target_add_compiler_flags target flag_list scope)
    foreach (flag IN LISTS ${flag_list})
       target_add_compiler_flag(${target} ${flag} ${scope})
    endforeach()
endfunction()

function(target_add_linker_flags target flag_list scope)
    foreach (flag IN LISTS ${flag_list})
       target_add_linker_flag(${target} ${flag} ${scope})
    endforeach()
endfunction()

function(target_add_common_cflags target scope)
    # enable warning flags for each compiler
    # only gcc, clang, tcc and msvc are supported so far
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_add_compiler_flags(${target} clang_flags ${scope})
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_add_compiler_flags(${target} gcc_flags ${scope})
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "TinyCC")
        target_add_compiler_flags(${target} tcc_flags ${scope})
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_add_compiler_flags(${target} msvc_flags ${scope})
    endif()
endfunction()

macro(add_static_lib package subdir)
    message(STATUS "using static ${package} from third-party/${subdir}")
    set(${package}_CHECKED ON PARENT_SCOPE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/third-party/${subdir} ${CMAKE_BINARY_DIR}/src/third-party/${subdir})
endmacro()

# helper macro that works exactly like find_package()
# except that it'll fallback to the static lib if the shared
# lib is not found!
macro(find_shared_lib package subdir)
    find_package(${package} QUIET)
    if (${package}_FOUND)
        message(STATUS "using shared ${package}")
    elseif (NOT ${package}_CHECKED)
        add_static_lib(${package} ${subdir})
    endif()
endmacro()

add_subdirectory(core)

add_executable(main main.cpp)
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(main GBA)
target_link_libraries(main SDL2::SDL2)
target_link_libraries(main SDL2::SDL2main)

set(DUMP_AUDIO OFF)
set(SPEED_TEST OFF)

target_compile_definitions(main PRIVATE
    SPEED_TEST=$<BOOL:${SPEED_TEST}>
    DUMP_AUDIO=$<BOOL:${DUMP_AUDIO}>
)
