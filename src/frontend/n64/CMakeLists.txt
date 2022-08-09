cmake_minimum_required(VERSION 3.18.0)

project(beeg_N64
    LANGUAGES C CXX
    VERSION 0.0.1
)

add_executable(beeg_N64 main.cpp)
target_link_libraries(beeg_N64 PRIVATE GBA)
set_target_properties(beeg_N64 PROPERTIES
    # RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)

target_add_common_cflags(beeg_N64 PRIVATE)
target_apply_lto_in_release(beeg_N64)

n64_create_dfs(
    ${CMAKE_CURRENT_SOURCE_DIR}/roms # src
    roms.dfs # output
)

n64_create_z64(beeg_N64
    NAME "beeg_N64"
    DFS "roms.dfs"
)
