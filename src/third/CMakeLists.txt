add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glfw)

set(SHADERC_SKIP_TESTS ON)
set(SKIP_SPIRV_TOOLS_INSTALL ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/shaderc)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glm)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/VulkanMemoryAllocator)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/stb)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tinyobjloader)
