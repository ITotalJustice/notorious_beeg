cmake_minimum_required(VERSION 3.20.0)

# fetch imgui
include(FetchContent)

Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.87
    GIT_PROGRESS   TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_Declare(imgui_club
    GIT_REPOSITORY https://github.com/ocornut/imgui_club.git
    GIT_TAG        d4cd9896e15a03e92702a578586c3f91bbde01e8
    GIT_PROGRESS   TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

FetchContent_MakeAvailable(imgui imgui_club)

add_library(imgui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/imgui_demo.cpp)
endif()

target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}/ ${imgui_SOURCE_DIR}/backends)

# the backend we want
find_package(SDL2 CONFIG REQUIRED)
target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer.cpp)
target_link_libraries(imgui PRIVATE SDL2::SDL2)

# memory editor
add_library(imgui_club INTERFACE)
target_include_directories(imgui INTERFACE ${imgui_club_SOURCE_DIR}/imgui_memory_editor)
# fetch imgui is done!

add_executable(main
    main.cpp
    system.cpp
    debugger/io.cpp
)

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(main PRIVATE GBA)
target_link_libraries(main PRIVATE SDL2::SDL2)
target_link_libraries(main PRIVATE SDL2::SDL2main)
target_link_libraries(main PRIVATE imgui imgui_club)

# target_add_common_cflags(main PRIVATE)
target_apply_lto_in_release(imgui)
target_apply_lto_in_release(main)

option(DUMP_AUDIO "dump audio to a file" OFF)
option(SPEED_TEST "run uncapped fps, used for perf test" OFF)
option(DEBUGGER "basic imgui debugger" ON)

target_compile_definitions(main PRIVATE
    SPEED_TEST=$<BOOL:${SPEED_TEST}>
    DUMP_AUDIO=$<BOOL:${DUMP_AUDIO}>
    DEBUGGER=$<BOOL:${DEBUGGER}>
)
