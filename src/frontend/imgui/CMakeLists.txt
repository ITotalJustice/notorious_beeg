cmake_minimum_required(VERSION 3.20.0)

project(imgui_base LANGUAGES CXX)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.88
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(imgui_club
    GIT_REPOSITORY https://github.com/ocornut/imgui_club.git
    GIT_TAG        d4cd9896e15a03e92702a578586c3f91bbde01e8
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(imgui)
FetchContent_MakeAvailable(imgui_club)

add_library(imgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}/backends)

# memory editor
add_library(imgui_club INTERFACE)
target_include_directories(imgui INTERFACE ${imgui_club_SOURCE_DIR}/imgui_memory_editor)
# fetch imgui is done!

add_library(imgui_base
    imgui_base.cpp
    debugger_io_gb.cpp
    debugger_io_gba.cpp
    imgui_log.cpp
)

target_include_directories(imgui_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(imgui_base PUBLIC frontend_base)
target_link_libraries(imgui_base PUBLIC imgui)
target_link_libraries(imgui_base PRIVATE imgui_club)

# target_add_common_cflags(imgui_base PRIVATE)

option(DUMP_AUDIO "dump audio to a file" OFF)
option(SPEED_TEST "run uncapped fps, used for perf test" OFF)
option(DEBUGGER "basic imgui debugger" ON)

target_compile_definitions(imgui_base PRIVATE
    SPEED_TEST=$<BOOL:${SPEED_TEST}>
    DUMP_AUDIO=$<BOOL:${DUMP_AUDIO}>
    DEBUGGER=$<BOOL:${DEBUGGER}>
)

set_target_properties(imgui_base PROPERTIES CXX_STANDARD 23)

add_subdirectory(backend)
