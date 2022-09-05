cmake_minimum_required(VERSION 3.20.0)

project(benchmark LANGUAGES CXX)

add_executable(benchmark main.cpp)

target_link_libraries(benchmark PUBLIC frontend_base)
set_target_properties(benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)

target_add_common_cflags(benchmark PRIVATE)
