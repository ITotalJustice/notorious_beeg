cmake_minimum_required(VERSION 3.20.0)

project(sdl2_base LANGUAGES CXX)

add_library(sdl2_base sdl2_base.cpp)

find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(sdl2_base PUBLIC frontend_base)
target_link_libraries(sdl2_base PUBLIC SDL2::SDL2 SDL2::SDL2main)

target_include_directories(sdl2_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sdl2_base PROPERTIES CXX_STANDARD 23)


target_add_common_cflags(sdl2_base PRIVATE)
