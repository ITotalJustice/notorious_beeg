cmake_minimum_required(VERSION 3.20.0)

project(notorious_beeg_SDL2 LANGUAGES CXX)

add_executable(notorious_beeg_SDL2 main.cpp)

find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(notorious_beeg_SDL2 PRIVATE frontend_base)
target_link_libraries(notorious_beeg_SDL2 PRIVATE SDL2::SDL2 SDL2::SDL2main)

target_add_common_cflags(notorious_beeg_SDL2 PRIVATE)
target_apply_lto_in_release(notorious_beeg_SDL2)

set_target_properties(notorious_beeg_SDL2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)
