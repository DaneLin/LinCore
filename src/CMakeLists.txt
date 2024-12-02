
# Add source to this project's executable.
add_executable (lincore
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  
   
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp
  camera.cpp
  camera.h
  "logging.h" "cvars.cpp" "frustum_cull.h" "vk_pushbuffer.h" "vk_pushbuffer.cpp" "config.h" "vk_shaders.h" "vk_shaders.cpp" "vk_scene.h" "vk_scene.cpp" "vk_profiler.h" "vk_profiler.cpp"    "command_buffer.h" "command_buffer.cpp" "frame_graph.h" "frame_graph.cpp")

set_property(TARGET lincore PROPERTY CXX_STANDARD 20)
target_compile_definitions(lincore PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(lincore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_dependencies(lincore Shaders)
target_link_libraries(lincore PUBLIC volk vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf spdlog::spdlog spirv_reflect enkiTS)

target_precompile_headers(lincore PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <volk/volk.h>)

add_custom_command(TARGET lincore POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:lincore> $<TARGET_FILE_DIR:lincore>
  COMMAND_EXPAND_LISTS
  )