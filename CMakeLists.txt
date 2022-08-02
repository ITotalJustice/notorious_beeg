cmake_minimum_required(VERSION 3.20.0)

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

message(STATUS "SYSTEM: ${CMAKE_SYSTEM_NAME}")
message(STATUS "COMPILER: ${CMAKE_CXX_COMPILER_ID}")

option(FRONTEND "enable frontend build" ON)
option(SDL2 "basic sdl2 frontend" OFF)
option(IMGUI "imgui frontend" OFF)

add_subdirectory(src)
