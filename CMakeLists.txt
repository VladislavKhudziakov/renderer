cmake_minimum_required(VERSION 3.17)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
 endif()

project(renderer)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils.cmake)

set(GLSL_TOOLS ${CMAKE_CURRENT_LIST_DIR}/tools/glsl)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src)

add_definitions(-DVERSION_MAJOR=0 -DVERSION_MINOR=1 -DVERSION_PATCH=0)