
make_bin(
    NAME
        test_app
    DEPENDS
        glm_
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