cmake_minimum_required(VERSION 3.20.0)

# fetch imgui
include(FetchContent)

Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.88
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

FetchContent_Declare(minizip
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.2.12
    GIT_PROGRESS   TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)

SET(COMPRESSOR_ZLIB ON)

FetchContent_Declare(rewind
    GIT_REPOSITORY https://github.com/ITotalJustice/librewind.git
    GIT_TAG        master
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(rewind)
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

# memory editor
add_library(imgui_club INTERFACE)
target_include_directories(imgui INTERFACE ${imgui_club_SOURCE_DIR}/imgui_memory_editor)
# fetch imgui is done!

# always include backend after fetching imgui
add_subdirectory(backend)

add_executable(notorious_beeg
    main.cpp
    system.cpp
    debugger/io.cpp
)

target_link_libraries(notorious_beeg PRIVATE GBA)
target_link_libraries(notorious_beeg PRIVATE imgui imgui_club)
target_link_libraries(notorious_beeg PRIVATE backend)
target_link_libraries(notorious_beeg PRIVATE
    rewind
    compressor_zlib
)

set(FOUND_MINIZIP FALSE)

if (VCPKG_TOOLCHAIN)
    set(DISABLE_INSTALL_TOOLS ON)
    find_package(minizip CONFIG)
    if (DEFINED minizip_FOUND)
        target_link_libraries(notorious_beeg PRIVATE minizip::minizip)

        set(FOUND_MINIZIP TRUE)
        message(STATUS "using vcpkg minizip")
    endif()
else()
    # find package for minizip is broken on fedora
    # not sure other distros have .cmake for minizip either
    # so using the old fashioned find_() has to be used
    find_library(minizip_lib minizip)
    find_path(minizip_inc minizip)

    if (minizip_lib AND minizip_inc)
        target_link_libraries(notorious_beeg PRIVATE ${minizip_lib})
        target_include_directories(notorious_beeg PRIVATE ${minizip_inc})

        set(FOUND_MINIZIP TRUE)
        message(STATUS "using system minizip")
    endif()
endif()

# failed to find minizip installed, fetch and build manually
if (NOT FOUND_MINIZIP)
    # dont use FetchContent_MakeAvailable() here because it'll
    # run the cmake file in zlib as it implicitly calls add_subdirectory
    # causing all of zlib to be built.
    FetchContent_GetProperties(minizip)
    if (NOT minizip_POPULATED)
        FetchContent_Populate(minizip)
    endif()

    add_library(minizip
        ${minizip_SOURCE_DIR}/contrib/minizip/ioapi.c
        ${minizip_SOURCE_DIR}/contrib/minizip/zip.c
        ${minizip_SOURCE_DIR}/contrib/minizip/unzip.c
    )

    target_include_directories(minizip PRIVATE ${minizip_SOURCE_DIR}/contrib/minizip/)
    # need the paths to be minizip/unzip.h
    target_include_directories(minizip PUBLIC ${minizip_SOURCE_DIR}/contrib/)

    find_package(ZLIB REQUIRED)
    target_link_libraries(minizip PRIVATE ZLIB::ZLIB)
    target_link_libraries(notorious_beeg PRIVATE minizip)

    message(STATUS "using github minizip")
endif()

# target_add_common_cflags(notorious_beeg PRIVATE)
target_apply_lto_in_release(imgui)
target_apply_lto_in_release(notorious_beeg)

option(DUMP_AUDIO "dump audio to a file" OFF)
option(SPEED_TEST "run uncapped fps, used for perf test" OFF)
option(DEBUGGER "basic imgui debugger" ON)

target_compile_definitions(notorious_beeg PRIVATE
    SPEED_TEST=$<BOOL:${SPEED_TEST}>
    DUMP_AUDIO=$<BOOL:${DUMP_AUDIO}>
    DEBUGGER=$<BOOL:${DEBUGGER}>
)

set_target_properties(notorious_beeg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
)
