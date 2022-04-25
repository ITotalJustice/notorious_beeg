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

# todo: use this if minizip is not installed
FetchContent_Declare(minizip
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.2.12
    GIT_PROGRESS   TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
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

# memory editor
add_library(imgui_club INTERFACE)
target_include_directories(imgui INTERFACE ${imgui_club_SOURCE_DIR}/imgui_memory_editor)
# fetch imgui is done!

# always include backend after fetching imgui
add_subdirectory(backend)

add_executable(main
    main.cpp
    system.cpp
    debugger/io.cpp
)

target_link_libraries(main PRIVATE GBA)
target_link_libraries(main PRIVATE imgui imgui_club)
target_link_libraries(main PRIVATE backend)

set(FOUND_MINIZIP FALSE)

if (VCPKG_TOOLCHAIN)
    find_package(minizip CONFIG)
    if (DEFINED minizip_FOUND)
        target_link_libraries(main PRIVATE minizip::minizip)

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
        target_link_libraries(main PRIVATE ${minizip_lib})
        target_include_directories(main PRIVATE ${minizip_inc})

        # on switch, i need to manually link against
        # zlib as well, and has to be linked after minizip!
        find_package(ZLIB REQUIRED)
        target_link_libraries(main PRIVATE ZLIB::ZLIB)

        set(FOUND_MINIZIP TRUE)
        message(STATUS "using system minizip lib: ${minizip_lib} inc: ${minizip_inc}")
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
    target_link_libraries(main PRIVATE minizip)

    message(STATUS "using github minizip")
endif()

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

if (NINTENDO_SWITCH)
    # create romfs folder
    dkp_add_asset_target(main_romfs ${CMAKE_CURRENT_BINARY_DIR}/romfs)

    # setup nacp
    nx_generate_nacp(main.nacp
        NAME "Notorious BEEG"
        AUTHOR TotalJustice
        VERSION 0.0.3
    )

    # create nro (final binary)
    nx_create_nro(main
        ICON ${CMAKE_SOURCE_DIR}/src/frontend/backend/nx/icon.jpg
        NACP main.nacp
        ROMFS main_romfs
    )

    # compile and add shaders to romfs
    set(SHADER_FOLDER ${CMAKE_SOURCE_DIR}/src/frontend/backend/nx/shaders)

    nx_add_shader_program(imgui_fsh ${SHADER_FOLDER}/imgui_fsh.glsl frag)
    nx_add_shader_program(imgui_vsh ${SHADER_FOLDER}/imgui_vsh.glsl vert)

    dkp_install_assets(main_romfs
        DESTINATION shaders
        TARGETS
            imgui_fsh
            imgui_vsh
    )
endif()
