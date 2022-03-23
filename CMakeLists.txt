cmake_minimum_required(VERSION 3.20.0)

project(TotalGB
    LANGUAGES CXX
)

message("SYSTEM: ${CMAKE_SYSTEM_NAME}")
message("COMPILER: ${CMAKE_CXX_COMPILER_ID}")

# adding the below breaks clangd atm because cmake
# adds the flag "-std=c++23" which is unsupported yet by clang
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2b")
else()
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

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

add_subdirectory(src)
