
if (UNIX AND NOT APPLE)
    set(VK_DEPS_LIBS vulkan X11 Xrandr Xi dl)
else()
    error("TODO")
endif()

make_bin(
    NAME
        test_app
    DEPENDS
        glfw
        shaderc
        glm_
        stb
        tinyobjloader
        ${VK_DEPS_LIBS}
        vk_utils
        base_app
        logger
    RESOURCES_EXTENSIONS
        .jpg
        .png
        .obj
        .mtl
)