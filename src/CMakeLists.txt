add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/samples)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs)

include_directories(${CMAKE_CURRENT_LIST_DIR}/libs)