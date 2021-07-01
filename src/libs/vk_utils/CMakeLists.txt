
if (UNIX AND NOT APPLE)
    set(VK_DEPS_LIBS vulkan X11 Xrandr Xi dl)
else()
    find_package(Vulkan REQUIRED)
    set(VK_DEPS_LIBS Vulkan::Vulkan)
endif()

make_bin(
    NAME
        vk_utils
    LIB
    LIB_TYPE
        STATIC
    DEPENDS
        ${VK_DEPS_LIBS}
        shaderc
        stb
        tinyobjloader
        glm_
        errors
        VulkanMemoryAllocator)

if (WIN32)
    target_compile_options(vk_utils PRIVATE /permissive)
endif()

if (${RENDERER_ENABLE_VALIDATION_LAYERS})
    target_compile_definitions(vk_utils PRIVATE -DUSE_VALIDATION_LAYERS)
endif()
