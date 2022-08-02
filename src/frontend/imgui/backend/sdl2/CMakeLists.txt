cmake_minimum_required(VERSION 3.20.0)

project(notorious_beeg_IMGUI_SDL2 LANGUAGES CXX)

add_executable(notorious_beeg_IMGUI_SDL2
    main.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer.cpp
)

find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(notorious_beeg_IMGUI_SDL2 PRIVATE imgui_base)
target_link_libraries(notorious_beeg_IMGUI_SDL2 PRIVATE SDL2::SDL2)

set_target_properties(notorious_beeg_IMGUI_SDL2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)

target_apply_lto_in_release(notorious_beeg_IMGUI_SDL2)
