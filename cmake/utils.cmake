include(CMakeParseArguments)

function (make_app)
    set(OPTIONS LIB)
    set(ONE_VAL_ARGS NAME LIB_TYPE)
    set(MULTI_VAL_ARGS DEPENDS)

    cmake_parse_arguments(NEW_APP "${OPTIONS}" "${ONE_VAL_ARGS}" "${MULTI_VAL_ARGS}" ${ARGN})

    file(GLOB_RECURSE ${NAME_APP_NAME}_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp  ${CMAKE_CURRENT_LIST_DIR}/*.c)
    if (${NEW_APP_LIB_TYPE})
        set(CURR_NEW_APP_LIB_TYPE ${NEW_APP_LIB_TYPE})
    else()
        set(CURR_NEW_APP_LIB_TYPE STATIC)
    endif()

    if (APPLE)
        file(GLOB_RECURSE ${NAME_APP_NAME}_OBJC_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.mm)
    endif()

    file(GLOB_RECURSE NEW_APP_SHADERS ${CMAKE_CURRENT_LIST_DIR}/*.glsl)

    if (${NEW_APP_LIB})
        add_library(${NEW_APP_NAME} ${CURR_NEW_APP_LIB_TYPE} ${${NAME_APP_NAME}_SOURCES} ${${NAME_APP_NAME}_OBJC_SOURCES})
        target_include_directories(${NEW_APP_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})
        target_include_directories(${NEW_APP_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

        target_link_libraries(${NEW_APP_NAME} PUBLIC ${NEW_APP_DEPENDS})
    else()
        add_executable(${NEW_APP_NAME} ${${NAME_APP_NAME}_SOURCES} ${${NAME_APP_NAME}_OBJC_SOURCES})
        target_link_libraries(${NEW_APP_NAME} PRIVATE ${NEW_APP_DEPENDS})
    endif()

    if (WIN32)
        set(EXEC_EXTENSION ".exe")
    endif()

    set(GLSL_VALIDATOR ${GLSL_TOOLS}/glslangValidator${EXEC_EXTENSION})

    foreach(GLSL ${NEW_APP_SHADERS})
        file(RELATIVE_PATH REL_GLSL_PATH ${CMAKE_CURRENT_LIST_DIR} ${GLSL})
        set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/${REL_GLSL_PATH}.spv")
        get_filename_component(SPIRV_DIR ${SPIRV} DIRECTORY)
        get_filename_component(GLSL_FILE ${GLSL} NAME)
        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy ${GLSL} ${SPIRV_DIR}/${GLSL_FILE}
                COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
                DEPENDS ${GLSL})

        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)

    add_custom_target(
            ${NEW_APP_NAME}_SHADERS
            DEPENDS ${SPIRV_BINARY_FILES})

    add_dependencies(${NEW_APP_NAME} ${NEW_APP_NAME}_SHADERS)
endfunction()
