//> includes
#include "vk_engine.h"
// std
#include <chrono>
#include <thread>
#include <random>

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
#include "foundation/math_utils.h"
#include "graphics/backend/vk_initializers.h"
#include "graphics/scene_graph/scene_types.h"
#include "graphics/backend/vk_pipelines.h"
#include "graphics/backend/vk_profiler.h"
#include "graphics/backend/vk_device.h"
#include "graphics/scene_graph/gltf_loader.h"
#include "graphics/scene_graph/scene_view.h"
#include "graphics/render_pass/passes/mesh_pass.h"

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

		main_camera_.Init(glm::vec3(-10, 1, 0), 70.f, (float)window_extent_.width / (float)window_extent_.height, 10000.f, 0.1f);

		// Initialize sunlight parameters
		gpu_device_.scene_data_.sunlight_direction = glm::vec4(-0.5f, -1.0f, -0.5f, 5.0f); // w component is sun power
		gpu_device_.scene_data_.sunlight_color = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f);	   // Warm sunlight color

		InitResources();
		InitPasses();

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
			gbuffer_pass_.Shutdown();
			light_pass_.Shutdown();
			ssao_pass_.Shutdown();
			blur_pass_.Shutdown();
			sky_box_pass_.Shutdown();

			scene_graph_.reset();
			imgui_layer_.Shutdown();

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
			gpu_device_.profiler_.GrabQueries(cmd->GetVkCommandBuffer());

			// sky_background_pass_.Execute(cmd, &current_frame_data);

			scene::SceneView scene_view;
			scene_view.SetViewType(scene::SceneView::ViewType::Main);
			scene_view.SetCamera(&main_camera_);

			scene::DrawCullData draw_cull_data = scene_view.GetCullData();
			scene::GPUResourcePool gpu_resource_pool = scene_graph_->GetGPUResourcePool();
			draw_cull_data.draw_count = gpu_resource_pool.draw_count;

			culling_pass_.SetCullData(draw_cull_data);
			culling_pass_.Execute(cmd, &current_frame_data);

			// 更新FrameData中的场景GPU资源
			current_frame_data.scene_gpu_data.draw_indirect_buffer = gpu_resource_pool.draw_indirect_buffer;
			current_frame_data.scene_gpu_data.index_buffer = gpu_resource_pool.index_buffer;
			current_frame_data.scene_gpu_data.draw_count = gpu_resource_pool.draw_count;

			// mesh_pass_.Execute(cmd, &current_frame_data);
			gbuffer_pass_.Execute(cmd, &current_frame_data);

			ssao_pass_.Execute(cmd, &current_frame_data);

			blur_pass_.Execute(cmd, &current_frame_data);

			light_pass_.Execute(cmd, &current_frame_data);
			sky_box_pass_.Execute(cmd, &current_frame_data);
		}

		{
			VulkanScopeTimer timer(cmd->GetVkCommandBuffer(), &gpu_device_.profiler_, "imgui_pass");
			imgui_layer_.Draw(cmd, swapchain_image_index);
		}

		// Update mesh draw time from GPU profiler
		if (gpu_device_.profiler_.timing_.contains("mesh_pass"))
		{
			engine_stats_.mesh_draw_time = gpu_device_.profiler_.timing_["mesh_pass"];
		}

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
			auto frame_start = std::chrono::steady_clock::now();

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
				OnResize();
			}

			DrawImGui();

			// our draw function
			Draw();

			auto frame_end = std::chrono::steady_clock::now();
			auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
			engine_stats_.frame_time = frame_duration.count() / 1000.f; // Convert to milliseconds
		}
	}

	void VulkanEngine::UpdateScene()
	{
		engine_stats_.scene_update_time = 0;
		auto start = std::chrono::system_clock::now();

		main_camera_.Update();

		gpu_device_.scene_data_.view = main_camera_.GetViewMatrix();
		gpu_device_.scene_data_.proj = main_camera_.GetProjectionMatrix();
		gpu_device_.scene_data_.proj[1][1] *= -1;
		gpu_device_.scene_data_.viewproj = gpu_device_.scene_data_.proj * gpu_device_.scene_data_.view;
		// Update camera position for shaders
		gpu_device_.scene_data_.camera_position = main_camera_.position_;

		Buffer *gpu_scene_data_buffer = gpu_device_.GetResource<Buffer>(gpu_device_.global_scene_data_buffer_.index);
		scene::GPUSceneData *scene_uniform_data = (scene::GPUSceneData *)gpu_scene_data_buffer->vma_allocation->GetMappedData();
		*scene_uniform_data = gpu_device_.scene_data_;

		auto end = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		engine_stats_.scene_update_time = elapsed.count() / 1000.f;
	}

	void VulkanEngine::DrawImGui()
	{
		// imgui new frame
		imgui_layer_.NewFrame();

		// Performance window (fixed at top-right corner)
		{
			const float WINDOW_PADDING = 10.0f;
			const float MENU_BAR_HEIGHT = ImGui::GetFrameHeight(); // Get the height of the menu bar
			ImVec2 window_pos(window_extent_.width - WINDOW_PADDING, WINDOW_PADDING + MENU_BAR_HEIGHT);
			ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));

			ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove |
											ImGuiWindowFlags_NoResize |
											ImGuiWindowFlags_AlwaysAutoResize |
											ImGuiWindowFlags_NoSavedSettings |
											ImGuiWindowFlags_NoNav;

			ImGui::Begin("Performance", nullptr, window_flags);
			ImGui::Text("Frame Time: %.2f ms", engine_stats_.frame_time);
			ImGui::Text("Update Time: %.2f ms", engine_stats_.scene_update_time);

			float total_draw_time = 0.0f;
			for (auto &[k, v] : gpu_device_.profiler_.timing_)
			{
				total_draw_time += v;
			}
			ImGui::Text("Draw Time: %.2f ms", total_draw_time);

			ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
						main_camera_.position_.x,
						main_camera_.position_.y,
						main_camera_.position_.z);

			if (ImGui::TreeNode("Render Timings"))
			{
				for (auto &[k, v] : gpu_device_.profiler_.timing_)
				{
					ImGui::Text("%s: %.2f ms", k.c_str(), v);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Detailed Stats"))
			{
				for (auto &[k, v] : gpu_device_.profiler_.stats_)
				{
					ImGui::Text("%s: %d", k.c_str(), v);
				}
				ImGui::TreePop();
			}
			ImGui::End();
		}

		// Main control panel window
		ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

		// Rendering Settings section
		if (ImGui::CollapsingHeader("Rendering Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Render Scale");
			ImGui::SameLine(150);
			ImGui::SetNextItemWidth(200);
			ImGui::SliderFloat("##render_scale", &gpu_device_.render_scale_, 0.3f, 1.0f);

			ImGui::AlignTextToFramePadding();
			ImGui::Text("VSync");
			ImGui::SameLine(150);
			bool vsync = gpu_device_.IsVSyncEnabled();
			if (ImGui::Checkbox("##vsync", &vsync))
			{
				gpu_device_.ToggleVSync(vsync);
			}
		}

		// Lighting Settings section
		if (ImGui::CollapsingHeader("Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Direction control (w component is used for sun power)
			float direction[3] = {gpu_device_.scene_data_.sunlight_direction.x,
								  gpu_device_.scene_data_.sunlight_direction.y,
								  gpu_device_.scene_data_.sunlight_direction.z};
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Sun Direction");
			ImGui::SameLine(150);
			ImGui::SetNextItemWidth(200);
			if (ImGui::SliderFloat3("##sun_direction", direction, -100.0f, 100.0f))
			{
				gpu_device_.scene_data_.sunlight_direction.x = direction[0];
				gpu_device_.scene_data_.sunlight_direction.y = direction[1];
				gpu_device_.scene_data_.sunlight_direction.z = direction[2];
			}

			// Sun power control
			float power = gpu_device_.scene_data_.sunlight_direction.w;
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Sun Power");
			ImGui::SameLine(150);
			ImGui::SetNextItemWidth(200);
			if (ImGui::SliderFloat("##sun_power", &power, 0.0f, 1000.0f))
			{
				gpu_device_.scene_data_.sunlight_direction.w = power;
			}

			// Color control
			float color[3] = {gpu_device_.scene_data_.sunlight_color.x,
							  gpu_device_.scene_data_.sunlight_color.y,
							  gpu_device_.scene_data_.sunlight_color.z};
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Sun Color");
			ImGui::SameLine(150);
			ImGui::SetNextItemWidth(200);
			if (ImGui::ColorEdit3("##sun_color", color))
			{
				gpu_device_.scene_data_.sunlight_color.x = color[0];
				gpu_device_.scene_data_.sunlight_color.y = color[1];
				gpu_device_.scene_data_.sunlight_color.z = color[2];
			}
		}

		ImGui::End();

		// Keep the debug menu bar
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
	}

	void VulkanEngine::OnResize()
	{
		int w, h;
		SDL_GetWindowSize(window_, &w, &h);
		window_extent_.width = w;
		window_extent_.height = h;

		gpu_device_.ResizeSwapchain(w, h);

		resize_requested_ = false;
	}
	void VulkanEngine::InitResources()
	{

		scene::LoadConfig load_config;
		load_config.debug_name = "Main Scene";
		// std::shared_ptr<scene::LoadedGLTF> test = scene::GLTFLoader::LoadGLTF(&gpu_device_, GetAssetPath("assets/structure.glb"), load_config);
		std::shared_ptr<scene::LoadedGLTF> test = scene::GLTFLoader::LoadGLTF(&gpu_device_, GetAssetPath("assets/Sponza/glTF/Sponza.gltf"), load_config);
		// std::shared_ptr<scene::LoadedGLTF> test = scene::GLTFLoader::LoadGLTF(&gpu_device_, GetAssetPath("assets/FlightHelmet/glTF/FlightHelmet.gltf"), load_config);
		// std::shared_ptr<scene::LoadedGLTF> test = scene::GLTFLoader::LoadGLTF(&gpu_device_, GetAssetPath("assets/space-helmet/source/DamagedHelmet.glb"), load_config);
		scene_graph_ = std::make_unique<scene::SceneGraph>(&gpu_device_);
		scene_graph_->Init();
		scene_graph_->BeginSceneUpdate();
		scene_graph_->AddGLTFScene(test);
		scene_graph_->EndSceneUpdate();

		TextureCreation gbuffer_creation{};
		gbuffer_creation.SetName("gbuffer_normal_rough")
			.SetSize(window_extent_.width, window_extent_.height, 1, false)
			.SetFormatType(VK_FORMAT_R16G16B16A16_SFLOAT, TextureType::Enum::Texture2D)
			.SetFlags(TextureFlags::Default_mask | TextureFlags::RenderTarget_mask);
		gbuffer_normal_rough_handle_ = gpu_device_.CreateResource(gbuffer_creation);
		Texture *gbuffer_normal_rough_texture = gpu_device_.GetResource<Texture>(gbuffer_normal_rough_handle_.index);
		gbuffer_normal_rough_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);

		gbuffer_creation.SetName("gbuffer_albedo_spec")
			.SetSize(window_extent_.width, window_extent_.height, 1, false)
			.SetFormatType(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Enum::Texture2D)
			.SetFlags(TextureFlags::Default_mask | TextureFlags::RenderTarget_mask);
		gbuffer_albedo_spec_handle_ = gpu_device_.CreateResource(gbuffer_creation);
		Texture *gbuffer_albedo_spec_texture = gpu_device_.GetResource<Texture>(gbuffer_albedo_spec_handle_.index);
		gbuffer_albedo_spec_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);

		gbuffer_creation.SetName("gbuffer_emission")
			.SetSize(window_extent_.width, window_extent_.height, 1, false)
			.SetFormatType(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Enum::Texture2D)
			.SetFlags(TextureFlags::Default_mask | TextureFlags::RenderTarget_mask);
		gbuffer_emission_handle_ = gpu_device_.CreateResource(gbuffer_creation);
		Texture *gbuffer_emission_texture = gpu_device_.GetResource<Texture>(gbuffer_emission_handle_.index);
		gbuffer_emission_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);

		// [SSAO]
		{
			std::default_random_engine rnd_engine((unsigned int)time(nullptr));
			std::uniform_real_distribution<float> rnd_dist(0.0f, 1.0f);

			// Sample kernel
			std::vector<glm::vec4> ssao_kernel(kMAX_KERNEL_SIZE);
			for (uint32_t i = 0; i < kMAX_KERNEL_SIZE; ++i)
			{
				glm::vec3 sample(rnd_dist(rnd_engine) * 2.0f - 1.0f, rnd_dist(rnd_engine) * 2.0f - 1.0f, rnd_dist(rnd_engine));
				sample = glm::normalize(sample);
				sample *= rnd_dist(rnd_engine);
				float scale = float(i) / float(kMAX_KERNEL_SIZE);
				scale = math_utils::Lerp(0.1f, 1.0f, scale * scale);
				ssao_kernel[i] = glm::vec4(sample * scale, 0.0f);
			}

			BufferCreation buffer_creation{};
			buffer_creation.Reset()
				.SetName("ssao_kernel")
				.SetData(ssao_kernel.data(), kMAX_KERNEL_SIZE * sizeof(glm::vec4))
				.SetUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, ResourceUsageType::Immutable)
				.SetDeviceOnly();
			ssao_kernel_buffer_handle_ = gpu_device_.CreateResource(buffer_creation);

			// Random noise
			std::vector<glm::vec4> noise_value(kSSAO_NOISE_DIM * kSSAO_NOISE_DIM);
			for (uint32_t i = 0; i < kSSAO_NOISE_DIM * kSSAO_NOISE_DIM; i++)
			{
				noise_value[i] = glm::vec4(rnd_dist(rnd_engine) * 2.0f - 1.0f, rnd_dist(rnd_engine) * 2.0f - 1.0f, 0.0f, 0.0f);
			}

			TextureCreation noise_creation{};
			noise_creation.SetName("ssao_noise")
				.SetSize(kSSAO_NOISE_DIM, kSSAO_NOISE_DIM, 1, false)
				.SetFormatType(VK_FORMAT_R32G32B32A32_SFLOAT, TextureType::Enum::Texture2D)
				.SetFlags(TextureFlags::Default_mask | TextureFlags::RenderTarget_mask)
				.SetData(noise_value.data(), kSSAO_NOISE_DIM * kSSAO_NOISE_DIM * sizeof(glm::vec4));
			ssao_noise_handle_ = gpu_device_.CreateResource(noise_creation);

			Texture *ssao_noise_texture = gpu_device_.GetResource<Texture>(ssao_noise_handle_.index);
			ssao_noise_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);

			TextureCreation ssao_creation{};
			ssao_creation.SetName("ssao_color")
				.SetSize(window_extent_.width, window_extent_.height, 1, false)
				.SetFormatType(VK_FORMAT_R8_UNORM, TextureType::Enum::Texture2D)
				.SetFlags(TextureFlags::Default_mask | TextureFlags::RenderTarget_mask)
				.SetData(nullptr, window_extent_.width * window_extent_.height * sizeof(glm::vec4));
			ssao_color_handle_ = gpu_device_.CreateResource(ssao_creation);

			Texture *ssao_color_texture = gpu_device_.GetResource<Texture>(ssao_color_handle_.index);
			ssao_color_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);

			TextureCreation ssao_blur_creation{};
			ssao_blur_creation.SetName("ssao_blur")
				.SetSize(window_extent_.width, window_extent_.height, 1, false)
				.SetFormatType(VK_FORMAT_R8_UNORM, TextureType::Enum::Texture2D)
				.SetFlags(TextureFlags::Default_mask | TextureFlags::RenderTarget_mask)
				.SetData(nullptr, window_extent_.width * window_extent_.height * sizeof(glm::vec4));
			ssao_blur_handle_ = gpu_device_.CreateResource(ssao_blur_creation);

			Texture *ssao_blur_texture = gpu_device_.GetResource<Texture>(ssao_blur_handle_.index);
			ssao_blur_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);
		}

		// cubemap
		{
			std::vector<std::string> paths = {
				GetAssetPath("assets/skybox/right.jpg"),
				GetAssetPath("assets/skybox/left.jpg"),
				GetAssetPath("assets/skybox/top.jpg"),
				GetAssetPath("assets/skybox/bottom.jpg"),
				GetAssetPath("assets/skybox/front.jpg"),
				GetAssetPath("assets/skybox/back.jpg")};
			gpu_device_.CreateTextureFromPaths(paths, cubemap_handle_, "cubemap", TextureType::Enum::TextureCube);

			Texture *cubemap_texture = gpu_device_.GetResource<Texture>(cubemap_handle_.index);
			cubemap_texture->sampler = gpu_device_.GetResource<Sampler>(gpu_device_.default_resources_.samplers.linear.index);
		}
	}

	void VulkanEngine::InitPasses()
	{
		sky_background_pass_.Init(&gpu_device_)
			.SetPassName("sky_pass")
			.BindInputs({{"image", gpu_device_.draw_image_handle_.index}})
			.Finalize();

		scene::GPUResourcePool gpu_resource_pool = scene_graph_->GetGPUResourcePool();
		culling_pass_.Init(&gpu_device_)
			.SetPassName("culling_pass")
			.BindInputs({
				{"object_buffer", gpu_resource_pool.instance_data_buffer.index},
				{"draw_buffer", gpu_resource_pool.draw_indirect_buffer.index},
			})
			.Finalize();

		mesh_pass_.Init(&gpu_device_)
			.SetPassName("mesh_pass")
			.BindInputs({{"object_buffer", gpu_resource_pool.instance_data_buffer.index},
						 {"vertex_buffer", gpu_resource_pool.vertex_buffer.index},
						 {"visible_draw_buffer", gpu_resource_pool.draw_indirect_buffer.index},
						 {"scene_data", gpu_device_.global_scene_data_buffer_.index},
						 {"material_data_buffer", gpu_resource_pool.material_buffer.index}})
			.BindRenderTargets({{"color_attachment", gpu_device_.draw_image_handle_}},
							   {{"depth_attachment", gpu_device_.depth_image_handle_}})
			.Finalize();

		gbuffer_pass_.Init(&gpu_device_)
			.SetPassName("gbuffer_pass")
			.BindInputs({{"object_buffer", gpu_resource_pool.instance_data_buffer.index},
						 {"vertex_buffer", gpu_resource_pool.vertex_buffer.index},
						 {"visible_draw_buffer", gpu_resource_pool.draw_indirect_buffer.index},
						 {"scene_data", gpu_device_.global_scene_data_buffer_.index},
						 {"material_data_buffer", gpu_resource_pool.material_buffer.index}})
			.BindRenderTargets({{"g_normal_rough", gbuffer_normal_rough_handle_.index},
								{"g_albedo_spec", gbuffer_albedo_spec_handle_.index},
								{"g_emission", gbuffer_emission_handle_.index}},
							   {{"depth_attachment", gpu_device_.depth_image_handle_}})
			.Finalize();

		ssao_pass_.Init(&gpu_device_)
			.SetPassName("ssao_pass")
			.BindInputs({{"scene_data", gpu_device_.global_scene_data_buffer_.index},
						 {"g_normal", gbuffer_normal_rough_handle_.index},
						 {"g_depth", gpu_device_.depth_image_handle_.index},
						 {"ssao_noise", ssao_noise_handle_.index},
						 {"ssao_kernel_buffer", ssao_kernel_buffer_handle_.index}})
			.BindRenderTargets({{"ssao_color", ssao_color_handle_.index}})
			.Finalize();

		blur_pass_.Init(&gpu_device_)
			.SetPassName("blur_pass")
			.BindInputs({{"ao_texture", ssao_color_handle_.index}})
			.BindRenderTargets({{"ssao_blur", ssao_blur_handle_.index}})
			.Finalize();

		light_pass_.Init(&gpu_device_)
			.SetPassName("light_pass")
			.BindInputs({{"scene_data", gpu_device_.global_scene_data_buffer_.index},
						 {"g_normal_rough", gbuffer_normal_rough_handle_.index},
						 {"g_albedo_spec", gbuffer_albedo_spec_handle_.index},
						 {"g_emission", gbuffer_emission_handle_.index},
						 {"depth_texture", gpu_device_.depth_image_handle_.index},
						 {"ssao_blur", ssao_blur_handle_.index}})
			.BindRenderTargets({{"color_attachment", gpu_device_.draw_image_handle_}})
			.Finalize();

		sky_box_pass_.Init(&gpu_device_)
			.SetPassName("sky_box_pass")
			.BindInputs({{"scene_data", gpu_device_.global_scene_data_buffer_.index},
						 {"skybox", cubemap_handle_.index}})
			.BindRenderTargets({{"color_attachment", gpu_device_.draw_image_handle_}},
							   {{"depth_attachment", gpu_device_.depth_image_handle_}})
			.Finalize();
	}
}