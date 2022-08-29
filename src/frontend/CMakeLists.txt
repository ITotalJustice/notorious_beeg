cmake_minimum_required(VERSION 3.20.0)

project(frontend_base LANGUAGES CXX)

add_library(frontend_base frontend_base.cpp)

target_link_libraries(frontend_base PUBLIC GBA)
target_include_directories(frontend_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(frontend_base PROPERTIES CXX_STANDARD 23)


target_add_common_cflags(frontend_base PRIVATE)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(minizip
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.2.12
    GIT_PROGRESS   TRUE
)

set(FOUND_MINIZIP FALSE)


# NOTE: these seems to be broken, tested using ubuntu in vm
# if (VCPKG_TOOLCHAIN)
#     set(DISABLE_INSTALL_TOOLS ON)
#     find_package(minizip CONFIG)
#     if (DEFINED minizip_FOUND)
#         target_link_libraries(frontend_base PRIVATE minizip::minizip)

#         set(FOUND_MINIZIP TRUE)
#         message(STATUS "using vcpkg minizip")
#     endif()
# else()
    # find package for minizip is broken on fedora
    # not sure other distros have .cmake for minizip either
    # so using the old fashioned find_() has to be used
    find_library(minizip_lib minizip)
    find_path(minizip_inc minizip)

    if (minizip_lib AND minizip_inc)
        target_link_libraries(frontend_base PRIVATE ${minizip_lib})
        target_include_directories(frontend_base PRIVATE ${minizip_inc})

        # this has to be linked after minizip!
        find_package(ZLIB REQUIRED)
        target_link_libraries(frontend_base PRIVATE ZLIB::ZLIB)

        set(FOUND_MINIZIP TRUE)
        message(STATUS "using system minizip")
    endif()
# endif()

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
    target_link_libraries(frontend_base PRIVATE minizip)

    message(STATUS "using github minizip")
endif()

if (IMGUI)
    add_subdirectory(sdl2_base)
    add_subdirectory(imgui)
endif()

if (SDL2)
    add_subdirectory(sdl2_base)
    add_subdirectory(sdl2)
endif()

if (EMSCRIPTEN)
    add_subdirectory(sdl2_base)
    add_subdirectory(emscripten)
endif()
