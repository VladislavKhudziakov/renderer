
if (UNIX AND NOT APPLE)
    set(VK_DEPS_LIBS vulkan X11 Xrandr Xi dl)
else()
    error("TODO")
endif()

make_app(
    NAME
        test_app
    DEPENDS
        glfw
        shaderc
        ${VK_DEPS_LIBS}
        vk_utils
        base_app
        logger
)
