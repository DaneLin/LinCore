
# Add source to this project's executable.
file(GLOB_RECURSE SOURCE CONFIGURE_DEPENDS *.cpp *h)

add_executable (lincore ${SOURCE})

set_property(TARGET lincore PROPERTY CXX_STANDARD 20)
target_compile_definitions(lincore PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE _CRT_SECURE_NO_WARNINGS)
target_include_directories(lincore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_dependencies(lincore Shaders)
target_link_libraries(lincore PUBLIC volk vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf spdlog::spdlog spirv_reflect enkiTS simdjson meshoptimizer)

target_precompile_headers(lincore PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <volk/volk.h>)

add_custom_command(TARGET lincore POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:lincore> $<TARGET_FILE_DIR:lincore>
  COMMAND_EXPAND_LISTS
  )