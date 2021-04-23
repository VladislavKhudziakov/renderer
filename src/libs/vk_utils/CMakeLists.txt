

make_bin(
    NAME
        vk_utils
    LIB
    LIB_TYPE
        STATIC
    DEPENDS
        stb
        tinyobjloader
        errors
        VulkanMemoryAllocator)