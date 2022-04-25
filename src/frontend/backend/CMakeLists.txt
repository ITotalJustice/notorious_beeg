cmake_minimum_required(VERSION 3.20.0)

if (NINTENDO_SWITCH)
    add_subdirectory(nx)
else()
    add_subdirectory(sdl2)
endif()
