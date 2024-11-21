# CMakeList.txt : CMake project for lindot, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("LinCore")

find_package(Vulkan REQUIRED)

add_subdirectory(third_party)
add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
if (NOT GLSL_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found!")
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
# Ensure the shader output directory exists
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/*.geom"
    )

message(STATUS "BUILDING SHADER")
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${SHADER_OUTPUT_DIR}/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  # Define a custom command to compile the shader
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Compiling shader: ${GLSL}"
    )
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
