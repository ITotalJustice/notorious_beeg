cmake_minimum_required(VERSION 3.18.0)

project(TotalGB
    LANGUAGES CXX
)

message("SYSTEM: ${CMAKE_SYSTEM_NAME}")
message("COMPILER: ${CMAKE_CXX_COMPILER_ID}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND cmake_supported_compiler_features
    AppleClang
    Clang
    GNU
    MSVC
    SunPro
    Intel
)

if (${CMAKE_CXX_COMPILER_ID} IN_LIST cmake_supported_compiler_features)
    set(COMPILER_FEATURES ON)
    message("compiler features are enabled for this compiler")
else()
    set(COMPILER_FEATURES OFF)
    message("compiler features are NOT enabled for this compiler")
endif()


find_program(CLANGD_INSTALLED clangd)
if (CLANGD_INSTALLED)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    message("found clangd: generating compile_commands.json to ${CMAKE_BINARY_DIR}/compile_commands.json")
else()
    message("cant find clangd: not generating compile_commands.json")
endif()

option(GBA_DEV "" OFF)
option(GBA_DEBUG "" ON)

add_subdirectory(src)
