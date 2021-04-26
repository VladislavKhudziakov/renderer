
make_bin(
    NAME
        errors
    LIB
    LIB_TYPE
        STATIC
    DEPENDS
        logger)


if (${RENDERER_NO_EXCEPTIONS})
    target_compile_definitions(errors PUBLIC -DNO_EXCEPTIONS)
endif()
