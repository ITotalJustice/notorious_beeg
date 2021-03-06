cmake_minimum_required(VERSION 3.20.0)

add_library(backend
    backend_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer.cpp
)

target_include_directories(backend PRIVATE
    ${imgui_SOURCE_DIR}/backends
)

target_include_directories(backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(SDL2 CONFIG REQUIRED)

target_link_libraries(backend PRIVATE imgui)
target_link_libraries(backend PRIVATE SDL2::SDL2)
target_link_libraries(backend PRIVATE GBA)
target_link_libraries(backend PRIVATE rewind) # todo: remove this

set_target_properties(backend PROPERTIES CXX_STANDARD 23)

target_apply_lto_in_release(backend)
