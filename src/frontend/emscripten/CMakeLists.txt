cmake_minimum_required(VERSION 3.20.0)

project(notorious_beeg_EMSDK LANGUAGES CXX)

add_executable(notorious_beeg_EMSDK main.cpp)

target_link_libraries(notorious_beeg_EMSDK PRIVATE sdl2_base)

target_add_common_cflags(notorious_beeg_EMSDK PRIVATE)

set_target_properties(notorious_beeg_EMSDK PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 23
    SUFFIX ".html"
    OUTPUT_NAME "index"
)

option(EM_USE_THREADS
    "enables building with threads
    this limits the number of browsers / hosts this
    emulator can be run on, however, audio should
    pop less frequently!" OFF)

option(EMRUN "build with emrun support, used for testing" OFF)

if (EM_USE_THREADS)
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    # target_link_options(notorious_beeg_EMSDK PRIVATE "-pthread")
    # target_compile_options(notorious_beeg_EMSDK PRIVATE "-pthread")
    target_compile_definitions(notorious_beeg_EMSDK PRIVATE EM_THREADS=1)
endif()

if (EMRUN)
    target_link_options(notorious_beeg_EMSDK PRIVATE "--emrun")
    target_compile_definitions(notorious_beeg_EMSDK PRIVATE EMRUN=1)
endif()

file(COPY "${CMAKE_SOURCE_DIR}/assets/buttons" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")
file(COPY "${CMAKE_SOURCE_DIR}/assets/menu" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/assets")
file(COPY "${CMAKE_SOURCE_DIR}/assets/web/" DESTINATION "${CMAKE_BINARY_DIR}/bin/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/netlify.toml" DESTINATION "${CMAKE_BINARY_DIR}/bin/")

target_link_options(notorious_beeg_EMSDK PRIVATE
    "-s;--shell-file;${CMAKE_CURRENT_SOURCE_DIR}/emscripten.html"
    "-s;EXPORTED_FUNCTIONS=[_malloc,_free]"
    "-sINITIAL_MEMORY=150mb"
    "-lidbfs.js"
    "--preload-file;${CMAKE_CURRENT_BINARY_DIR}/assets/@assets"
    "--use-preload-plugins"
)
