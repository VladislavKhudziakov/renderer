include(CMakeParseArguments)

function (make_bin)
    set(OPTIONS LIB)
    set(ONE_VAL_ARGS NAME LIB_TYPE)
    set(MULTI_VAL_ARGS DEPENDS RESOURCES_EXTENSIONS)

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

    file(
    GLOB_RECURSE 
    NEW_APP_SHADERS
        ${CMAKE_CURRENT_LIST_DIR}/*.glsl
        ${CMAKE_CURRENT_LIST_DIR}/*.vert
        ${CMAKE_CURRENT_LIST_DIR}/*.frag
        ${CMAKE_CURRENT_LIST_DIR}/*.tesc
        ${CMAKE_CURRENT_LIST_DIR}/*.tese
        ${CMAKE_CURRENT_LIST_DIR}/*.geom
        ${CMAKE_CURRENT_LIST_DIR}/*.comp
        ${CMAKE_CURRENT_LIST_DIR}/*.mesh
        ${CMAKE_CURRENT_LIST_DIR}/*.task
        ${CMAKE_CURRENT_LIST_DIR}/*.rgen
        ${CMAKE_CURRENT_LIST_DIR}/*.rhit
        ${CMAKE_CURRENT_LIST_DIR}/*.rahit
        ${CMAKE_CURRENT_LIST_DIR}/*.rchit
        ${CMAKE_CURRENT_LIST_DIR}/*.rmiss
        ${CMAKE_CURRENT_LIST_DIR}/*.rcall)

    if (${NEW_APP_LIB})
        add_library(${NEW_APP_NAME} ${CURR_NEW_APP_LIB_TYPE} ${${NAME_APP_NAME}_SOURCES} ${${NAME_APP_NAME}_OBJC_SOURCES})
        target_include_directories(${NEW_APP_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})
        target_include_directories(${NEW_APP_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

        target_link_libraries(${NEW_APP_NAME} PUBLIC ${NEW_APP_DEPENDS})
    else()   
        add_executable(${NEW_APP_NAME} ${${NAME_APP_NAME}_SOURCES} ${${NAME_APP_NAME}_OBJC_SOURCES})
        target_link_libraries(${NEW_APP_NAME} PRIVATE ${NEW_APP_DEPENDS})
    endif()

    target_compile_definitions(${NEW_APP_NAME} PRIVATE -DWORK_DIR="${CMAKE_CURRENT_BINARY_DIR}")

    if (WIN32)
        set(GLSL_VALIDATOR $ENV{VULKAN_SDK}/Bin/glslangValidator.exe)
    else()
        set(GLSL_VALIDATOR ${GLSL_TOOLS}/glslangValidator)
    endif()


    foreach(GLSL ${NEW_APP_SHADERS})
        file(RELATIVE_PATH REL_GLSL_PATH ${CMAKE_CURRENT_LIST_DIR} ${GLSL})
        set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/${REL_GLSL_PATH}.spv")
        get_filename_component(SPIRV_DIR ${SPIRV} DIRECTORY)
        get_filename_component(GLSL_FILE ${GLSL} NAME)
        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy ${GLSL} ${SPIRV_DIR}/${GLSL_FILE}
                COMMAND ${GLSL_VALIDATOR} -V -Os ${GLSL} -o ${SPIRV}
                DEPENDS ${GLSL})

        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)

    add_custom_target(
            ${NEW_APP_NAME}_SHADERS
            DEPENDS ${SPIRV_BINARY_FILES})

    add_dependencies(${NEW_APP_NAME} ${NEW_APP_NAME}_SHADERS)

    foreach(EXT ${NEW_APP_RESOURCES_EXTENSIONS})
        list(APPEND NEW_APP_RESOURCES_EXTENSIONS ${CMAKE_CURRENT_LIST_DIR}/*${EXT})
    endforeach()

    file(GLOB_RECURSE NEW_APP_RESOURCES ${NEW_APP_RESOURCES_EXTENSIONS})

    foreach(RESOURCE ${NEW_APP_RESOURCES})
        file(RELATIVE_PATH REL_RESOURCE_PATH ${CMAKE_CURRENT_LIST_DIR} ${RESOURCE})
        set(RESOURCE_COPY_PATH "${CMAKE_CURRENT_BINARY_DIR}/${REL_RESOURCE_PATH}")
        get_filename_component(RESOURCE_DIR ${RESOURCE_COPY_PATH} DIRECTORY)
        add_custom_command(
                OUTPUT ${RESOURCE_COPY_PATH}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy ${RESOURCE} ${RESOURCE_COPY_PATH})
        list(APPEND FINAL_RESOURCES_FILES ${RESOURCE_COPY_PATH})
    endforeach()

    add_custom_target(
            ${NEW_APP_NAME}_RESOURCES
            DEPENDS ${FINAL_RESOURCES_FILES})

    add_dependencies(${NEW_APP_NAME} ${NEW_APP_NAME}_RESOURCES)
endfunction()
