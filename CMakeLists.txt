cmake_minimum_required(VERSION 3.17)

project(renderer)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils.cmake)

set(GLSL_TOOLS ${CMAKE_CURRENT_LIST_DIR}/tools/glsl)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src)

add_definitions(-DVERSION_MAJOR=0 -DVERSION_MINOR=1 -DVERSION_PATCH=0)