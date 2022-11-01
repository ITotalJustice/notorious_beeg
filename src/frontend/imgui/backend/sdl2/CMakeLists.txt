cmake_minimum_required(VERSION 3.20.0)

project(notorious_beeg_IMGUI_SDL2 LANGUAGES CXX)

add_executable(notorious_beeg_IMGUI_SDL2
    main.cpp
    gl1_renderer.cpp
    sdl2_renderer.cpp
    sdl2_gl_renderer.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer.cpp
    # ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
)

find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(notorious_beeg_IMGUI_SDL2 PRIVATE imgui_base)
target_link_libraries(notorious_beeg_IMGUI_SDL2 PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(notorious_beeg_IMGUI_SDL2 PRIVATE glad)

target_include_directories(notorious_beeg_IMGUI_SDL2 PRIVATE ${imgui_SOURCE_DIR}/backends/)

target_compile_definitions(notorious_beeg_IMGUI_SDL2 PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM=1
)

set_target_properties(notorious_beeg_IMGUI_SDL2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)
