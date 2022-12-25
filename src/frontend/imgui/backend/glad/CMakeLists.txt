cmake_minimum_required(VERSION 3.20.0)

project(glad LANGUAGES C)
add_library(glad STATIC glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
