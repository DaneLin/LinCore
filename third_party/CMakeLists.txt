find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)
add_library(volk STATIC)
add_library(stb_image INTERFACE)
add_library(spirv_reflect STATIC)
add_library(enkiTS STATIC)
add_subdirectory(meshoptimizer)
add_subdirectory(simdjson)
add_subdirectory(fastgltf)
add_subdirectory(fmt EXCLUDE_FROM_ALL)
add_subdirectory(SDL EXCLUDE_FROM_ALL)



set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(spdlog)
set_property(TARGET spdlog PROPERTY FOLDER "third_party")

target_sources(volk PRIVATE
	volk/volk.c
	volk/volk.h
)

# 添加必要的包含路径
target_include_directories(volk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/volk)
target_link_libraries(volk PUBLIC Vulkan::Vulkan)
# 确保使用 Volk 时定义 VK_NO_PROTOTYPES
target_compile_definitions(volk INTERFACE VK_NO_PROTOTYPES)

target_sources(enkiTS PRIVATE
	enkiTS/LockLessMultiReadPipe.h
	enkiTS/TaskScheduler.h
	enkiTS/TaskScheduler.cpp
)
target_include_directories(enkiTS PUBLIC enkiTS)

target_sources(vkbootstrap PRIVATE 
  vkbootstrap/VkBootstrap.h
  vkbootstrap/VkBootstrap.cpp
  )

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)

target_include_directories(glm INTERFACE glm)

#add_library(sdl2 INTERFACE)

#target_include_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Include/SDL2 )
#target_link_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Lib )

#target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE 
  imgui/imgui.h
  imgui/imgui.cpp
  imgui/imgui_stdlib.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_impl_vulkan.cpp
  imgui/imgui_impl_sdl2.cpp
  )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

target_include_directories(stb_image INTERFACE stb_image)

target_sources(spirv_reflect PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.c"
)

target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect" )
target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/include" )
