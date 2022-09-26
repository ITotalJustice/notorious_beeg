cmake_minimum_required(VERSION 3.20.0)

if (VCPKG)
    # checks the documented env variable for vcpkg
    if (DEFINED ENV{VCPKG_ROOT} AND EXISTS $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
        message(STATUS "found VCPKG_ROOT, using system installed vcpkg at: $ENV{VCPKG_ROOT}")
        set(vcpkg_toolchain_file $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

    # this checks for github actions installed vcpkg
    elseif (DEFINED ENV{VCPKG_INSTALLATION_ROOT} AND EXISTS $ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake)
        message(STATUS "found VCPKG_INSTALLATION_ROOT, using system installed vcpkg at: $ENV{VCPKG_INSTALLATION_ROOT}")
        set(vcpkg_toolchain_file $ENV{VCPKG_INSTALLATION_ROOT}/scripts/buildsystems/vcpkg.cmake)

    # fallback to fetching latest vcpkg from git
    else()
        message(STATUS "installing latest vcpkg from git root: $ENV{VCPKG_ROOT} insta: $ENV{VCPKG_INSTALLATION_ROOT}")

        include(FetchContent)
        FetchContent_Declare(vcpkg GIT_REPOSITORY https://github.com/microsoft/vcpkg.git)
        FetchContent_MakeAvailable(vcpkg)

        set(vcpkg_toolchain_file ${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake)
    endif()

    # if this was already set, then we need to chainload the toolchain file
    # vcpkg offers support for this, which basically does include(toolchain)
    # need to make sure that we don't chainload our own toolchain file!
    if (CMAKE_TOOLCHAIN_FILE AND NOT ${CMAKE_TOOLCHAIN_FILE} STREQUAL ${vcpkg_toolchain_file})
        set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE})
    endif()

    set(CMAKE_TOOLCHAIN_FILE ${vcpkg_toolchain_file})
endif()

project(notorious_beeg
    VERSION 0.0.3
    DESCRIPTION "GBA emulator written in c++23"
    HOMEPAGE_URL "https://github.com/ITotalJustice/notorious_beeg"
    LANGUAGES C CXX
)

find_program(CLANGD_INSTALLED clangd)

if (CLANGD_INSTALLED)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    message(STATUS "found clangd: generating compile_commands.json to ${CMAKE_BINARY_DIR}/compile_commands.json")
else()
    message(STATUS "cant find clangd: not generating compile_commands.json")
endif()

if (LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if (ipo_supported)
        message(STATUS "IPO / LTO enabled for ALL targets")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO / LTO not supported")
    endif()
endif()

# these needs to be globally set otherwise a link error happens when
# using std::filesystem::recursive_directory_iterator
if (EMSCRIPTEN AND EM_USE_THREADS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

message(STATUS "SYSTEM: ${CMAKE_SYSTEM_NAME}")
message(STATUS "COMPILER: ${CMAKE_CXX_COMPILER_ID}")

option(FRONTEND "enable frontend build" OFF)
option(SDL2 "basic sdl2 frontend" OFF)
option(IMGUI "imgui frontend" OFF)
option(BENCHMARK "benchmark frontend" OFF)
option(NATIVE "enable native build" OFF)

if (SDL2)
    set(FRONTEND ON)
endif()

if (IMGUI)
    set(FRONTEND ON)
endif()

if (BENCHMARK)
    set(FRONTEND ON)
endif()

if (EMSCRIPTEN)
    set(FRONTEND ON)
endif()

if (TESTING)
    include(CTest)
    set(FRONTEND ON)
endif()

add_subdirectory(src)
