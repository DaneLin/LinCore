//> includes
#include "vk_engine.h"
// std
#include <chrono>
#include <thread>
// external
#include <SDL.h>
#include <SDL_vulkan.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <volk.h>
#include <glm/gtx/transform.hpp>
// lincore
#include "foundation/cvars.h"
#include "foundation/logging.h"
#include "graphics/vk_initializers.h"
#include "graphics/vk_types.h"
#include "graphics/vk_pipelines.h"
#include "graphics/vk_profiler.h"
#include "graphics/vk_device.h"

#include "graphics/scene/gltf_loader.h"
#include "graphics/scene/scene_view.h"

#include "graphics/renderer/passes/mesh_pass.h"

namespace lincore
{
	AutoCVar_Float CVAR_DrawDistance("gpu.drawDistance", "Distance cull", 5000);

	VulkanEngine *loaded_engine = nullptr;

	VulkanEngine &VulkanEngine::Get() { return *loaded_engine; }

	void VulkanEngine::Init()
	{
		spdlog::set_pattern(LOGGER_FORMAT);
		// only one engine initialization is allowed with the application.
		assert(loaded_engine == nullptr);
		loaded_engine = this;

		// We initialize SDL and create a window with it.
		SDL_Init(SDL_INIT_VIDEO);

		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

		window_ = SDL_CreateWindow(
			"LinCore",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			window_extent_.width,
			window_extent_.height,
			window_flags);

		// Initialize gpu device
		GpuDevice::CreateInfo create_info{};
		create_info.app_name = "LinCore";
		create_info.engine_name = "LinCore";
		create_info.api_version = VK_API_VERSION_1_3;
		create_info.window_extent = window_extent_;
		create_info.window = window_;

		gpu_device_.default_swapchain_info_.extent = window_extent_;
		gpu_device_.Init(create_info);

		imgui_layer_.Init(&gpu_device_);

		InitDefaultData();

		main_camera_.Init(glm::vec3(-10, 1, 0), 70.f, (float)window_extent_.width / (float)window_extent_.height, 10000.f, 0.1f);

		scene::LoadConfig load_config;
		load_config.debug_name = "Main Scene";

		sky_background_pass_.Init(&gpu_device_)
            .BindInputs({{"image", gpu_device_.draw_image_handle_.index}})
            .Finalize();

		std::shared_ptr<scene::LoadedGLTF> test = scene::GLTFLoader::LoadGLTF(&gpu_device_, GetAssetPath("assets/structure.glb"), load_config);
		scene_graph_ = std::make_unique<scene::SceneGraph>(&gpu_device_);
		scene_graph_->Init();
		scene_graph_->BeginSceneUpdate();
		scene_graph_->AddGLTFScene(test);
		scene_graph_->EndSceneUpdate();

		scene::GPUResourcePool gpu_resource_pool = scene_graph_->GetGPUResourcePool();
		culling_pass_.Init(&gpu_device_)
			.BindInputs({
				{"object_buffer", gpu_resource_pool.instance_data_buffer.index},
				{"draw_buffer", gpu_resource_pool.draw_indirect_buffer.index},
			})
			.Finalize();

		mesh_pass_.Init(&gpu_device_)
			.BindInputs({
				{"object_buffer", gpu_resource_pool.instance_data_buffer.index},
				{"vertex_buffer", gpu_resource_pool.vertex_buffer.index},
				{"visible_draw_buffer", gpu_resource_pool.draw_indirect_buffer.index},
				{"scene_data", global_scene_data_buffer_.index},
			})
			//{"material_data", gpu_resource_pool.material_buffer.index}})
			.BindRenderTargets({{"color_attachment", gpu_device_.draw_image_handle_}},
							   {{"depth_attachment", gpu_device_.depth_image_handle_}})
			.SetSceneGraph(scene_graph_.get())
			.Finalize();

		// everything went fine
		is_initialized_ = true;
	}

	void VulkanEngine::CleanUp()
	{
		if (is_initialized_)
		{
			// make sure the GPU has stopped doing its thing
			vkDeviceWaitIdle(gpu_device_.device_);

			sky_background_pass_.Shutdown();
			culling_pass_.Shutdown();
			mesh_pass_.Shutdown();
			scene_graph_.reset();
			imgui_layer_.Shutdown();
			main_deletion_queue_.Flush();

			gpu_device_.Shutdown();

			SDL_DestroyWindow(window_);
		}

		// clear engine pointer
		loaded_engine = nullptr;
	}

	void VulkanEngine::Draw()
	{
		UpdateScene();
		// wait until the gpu has finished rendering the last frame

		FrameData &current_frame_data = gpu_device_.BeginFrame();
		if (current_frame_data.result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			resize_requested_ = true;
			return;
		}

		CommandBuffer *cmd = current_frame_data.cmd;
		uint32_t swapchain_image_index = current_frame_data.swapchain_index;

		// Main Render Pass Logic
		{
			gpu_device_.profiler_.GrabQueries(cmd->GetCommandBuffer());

			sky_background_pass_.Execute(cmd, &current_frame_data);

			scene::SceneView scene_view;
			scene_view.SetViewType(scene::SceneView::ViewType::Main);
			scene_view.SetCamera(&main_camera_);

			scene::DrawCullData draw_cull_data = scene_view.GetCullData();
			scene::GPUResourcePool gpu_resource_pool = scene_graph_->GetGPUResourcePool();
			draw_cull_data.draw_count = gpu_resource_pool.draw_count;

			scene_graph_->ReadyCullingData(cmd);

			culling_pass_.SetCullData(draw_cull_data);

			culling_pass_.Execute(cmd, &current_frame_data);

			// 在执行计算着色器后，绘制前添加内存屏障
			UtilAddStateBarrier(&gpu_device_, cmd->vk_command_buffer_, PipelineStage::ComputeShader, PipelineStage::DrawIndirect);

			mesh_pass_.Execute(cmd, &current_frame_data);
		}

		imgui_layer_.Draw(cmd, swapchain_image_index);

		gpu_device_.EndFrame();
		if (current_frame_data.result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			resize_requested_ = true;
		}
	}

	void VulkanEngine::Run()
	{
		SDL_Event e;
		bool bQuit = false;

		// main loop
		while (!bQuit)
		{
			// begin clock
			auto start = std::chrono::system_clock::now();
			// Handle events on queue
			while (SDL_PollEvent(&e) != 0)
			{
				// close the window when user alt-f4s or clicks the X button
				if (e.type == SDL_QUIT)
					bQuit = true;

				if (e.type == SDL_WINDOWEVENT)
				{
					if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
					{
						freeze_rendering_ = true;
					}
					if (e.window.event == SDL_WINDOWEVENT_RESTORED)
					{
						freeze_rendering_ = false;
					}
				}
				main_camera_.ProcessSdlEvent(e);
				// send SDL events to imgui
				ImGui_ImplSDL2_ProcessEvent(&e);
			}

			// do not draw if we are minimized
			if (freeze_rendering_)
			{
				// throttle the speed to avoid the endless spinning
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			if (resize_requested_)
			{
				int w, h;
				SDL_GetWindowSize(window_, &w, &h);
				window_extent_.width = w;
				window_extent_.height = h;

				gpu_device_.ResizeSwapchain(w, h);
				resize_requested_ = false;
			}

			// imgui new frame
			imgui_layer_.NewFrame();

			if (ImGui::Begin("background"))
			{

				ImGui::SliderFloat("Render Scale", &gpu_device_.render_scale_, 0.3f, 1.0f);
			}
			ImGui::End();

			ImGui::Begin("Stats");

			ImGui::Text("frame_time %f ms", engine_stats_.frame_time);
			ImGui::Text("draw time %f ms", engine_stats_.mesh_draw_time);
			ImGui::Text("update time %f ms", engine_stats_.scene_update_time);
			ImGui::Text("triangles %i", engine_stats_.triangle_count);
			ImGui::Text("draws %i", engine_stats_.drawcall_count);

			ImGui::Separator();

			for (auto &[k, v] : gpu_device_.profiler_.timing_)
			{
				ImGui::Text("%s: %f ms", k.c_str(), v);
			}

			ImGui::Separator();

			for (auto &[k, v] : gpu_device_.profiler_.stats_)
			{
				ImGui::Text("%s: %i", k.c_str(), v);
			}

			ImGui::End();

			if (ImGui::BeginMainMenuBar())
			{

				if (ImGui::BeginMenu("Debug"))
				{
					if (ImGui::BeginMenu("CVAR"))
					{
						CVarSystem::Get()->DrawImguiEditor();
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			// make imgui calculate internal draw structures
			imgui_layer_.EndFrame();

			// our draw function
			Draw();

			auto end = std::chrono::system_clock::now();

			// convert to ms
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			engine_stats_.frame_time = elapsed.count() / 1000.f;
		}
	}

	void VulkanEngine::InitDefaultData()
	{
		BufferCreation buffer_info{};
		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Immutable, sizeof(GPUSceneData))
			.SetPersistent();
		global_scene_data_buffer_ = gpu_device_.CreateResource(buffer_info);
	}

	const std::string VulkanEngine::GetAssetPath(const std::string &path) const
	{
		return std::string("../../" + path);
	}

	void VulkanEngine::UpdateScene()
	{
		engine_stats_.scene_update_time = 0;

		auto start = std::chrono::system_clock::now();

		main_draw_context_.opaque_surfaces.clear();
		main_draw_context_.transparent_surfaces.clear();

		main_camera_.Update();

		gpu_device_.scene_data_.view = main_camera_.GetViewMatrix();
		// camera projection
		gpu_device_.scene_data_.proj = main_camera_.GetProjectionMatrix();

		// invert the Y direction on projection matrix so that we are more similar
		// to opengl and gltf axis
		gpu_device_.scene_data_.proj[1][1] *= -1;
		gpu_device_.scene_data_.viewproj = gpu_device_.scene_data_.proj * gpu_device_.scene_data_.view;

		// some default lighting parameters
		gpu_device_.scene_data_.ambient_color = glm::vec4(.1f);
		gpu_device_.scene_data_.sunlight_color = glm::vec4(1.f);
		gpu_device_.scene_data_.sunlight_direction = glm::vec4(0, 1, 0.5, 1.f);

		auto end = std::chrono::system_clock::now();

		// convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		engine_stats_.scene_update_time = elapsed.count() / 1000.f;

		Buffer *gpu_scene_data_buffer = gpu_device_.GetResource<Buffer>(global_scene_data_buffer_.index);

		// write the buffer
		GPUSceneData *scene_uniform_data = (GPUSceneData *)gpu_scene_data_buffer->vma_allocation->GetMappedData();
		*scene_uniform_data = gpu_device_.scene_data_;
	}

}