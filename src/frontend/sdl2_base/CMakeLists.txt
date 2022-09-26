cmake_minimum_required(VERSION 3.20.0)

project(sdl2_base LANGUAGES CXX)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        af1a5bc
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(stb)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

add_library(sdl2_base sdl2_base.cpp)

find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(sdl2_base PUBLIC frontend_base)
target_link_libraries(sdl2_base PUBLIC SDL2::SDL2 SDL2::SDL2main stb_image)

target_include_directories(sdl2_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sdl2_base PROPERTIES CXX_STANDARD 23)


target_add_common_cflags(sdl2_base PRIVATE)
