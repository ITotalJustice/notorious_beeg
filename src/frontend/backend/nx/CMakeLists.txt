cmake_minimum_required(VERSION 3.20.0)

add_library(backend
    backend_nx.cpp
    ftpd_imgui/imgui_deko3d.cpp
    ftpd_imgui/imgui_nx.cpp

    audio/audio.cpp
)

target_link_libraries(backend PRIVATE imgui)
target_link_libraries(backend PRIVATE GBA)
target_link_libraries(backend PRIVATE deko3dd)

# why is this even needed???
target_include_directories(backend PRIVATE ${DEVKITPRO}/portlibs/switch/include)

target_apply_lto_in_release(backend)
