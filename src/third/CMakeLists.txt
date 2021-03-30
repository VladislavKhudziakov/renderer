add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glfw)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/shaderc)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glm)
add_library(glm_include INTERFACE)
target_include_directories(glm_include INTERFACE ${CMAKE_CURRENT_LIST_DIR}/glm)


