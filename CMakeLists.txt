cmake_minimum_required(VERSION 3.20.0)

find_program(CLANGD_INSTALLED clangd)

if (CLANGD_INSTALLED)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    message("found clangd: generating compile_commands.json to ${CMAKE_BINARY_DIR}/compile_commands.json")
else()
    message("cant find clangd: not generating compile_commands.json")
endif()

project(notorious_beeg
    VERSION 0.0.1
    DESCRIPTION "GBA emulator written in c++23"
    HOMEPAGE_URL "https://github.com/ITotalJustice/notorious_beeg"
    LANGUAGES C CXX
)

message("SYSTEM: ${CMAKE_SYSTEM_NAME}")
message("COMPILER: ${CMAKE_CXX_COMPILER_ID}")

add_subdirectory(src)
