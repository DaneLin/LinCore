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
#include "fundation/cvars.h"
#include "fundation/logging.h"
#include "graphics/vk_initializers.h"
#include "graphics/vk_types.h"
#include "graphics/vk_pipelines.h"
#include "graphics/vk_profiler.h"
#include "graphics/vk_loader.h"
#include "graphics/frustum_cull.h"
#include "graphics/vk_device.h"

namespace lincore
{
	AutoCVar_Float CVAR_DrawDistance("gpu.drawDistance", "Distance cull", 5000);

	VulkanEngine* loaded_engine = nullptr;

	VulkanEngine& VulkanEngine::Get() { return *loaded_engine; }

	struct CommandRecordingTask : enki::ITaskSet {
		VulkanEngine* engine;
		CommandBuffer* primary_cmd;
		std::vector<uint32_t>* draw_indices;
		uint32_t thread_count;
		uint32_t frame_index;
		VkDescriptorSet global_descriptor;
		VkDescriptorSet bindless_descriptor;
		VkExtent2D draw_extent;
		std::vector<RenderObject>* render_objects;
		uint32_t start;
		uint32_t end;

		virtual void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override {
			CommandBuffer* thread_cmd = engine->gpu_device_.command_buffer_manager_.GetSecondaryCommandBuffer(frame_index, threadnum);
			CommandBufferInheritanceInfo inheritance_info{};
			inheritance_info.color_attachment_count = 1;
			inheritance_info.color_formats = &engine->gpu_device_.GetDrawImage()->vk_format;
			inheritance_info.depth_format = engine->gpu_device_.GetDepthImage()->vk_format;
			inheritance_info.samples = VK_SAMPLE_COUNT_1_BIT;
			inheritance_info.render_area = { 0, 0, draw_extent.width, draw_extent.height };

			thread_cmd->Reset();
			thread_cmd->BeginSecondary(inheritance_info);

#if LC_DRAW_INDIRECT
			thread_cmd->BindIndexBuffer(engine->gpu_device_.GetResource<Buffer>(engine->global_mesh_buffer_.index_buffer_handle.index)->vk_buffer, 0, VK_INDEX_TYPE_UINT32);
#endif

			thread_cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
				engine->mesh_pipeline_layout_, 0, 1, &global_descriptor, 0, nullptr);
			thread_cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS,
				engine->mesh_pipeline_layout_, 2, 1, &bindless_descriptor, 0, nullptr);

			RenderInfo info{};
			for (uint32_t i = start; i < end; i++)
			{
				if (draw_indices != nullptr)
				{
					engine->DrawObject(thread_cmd, (*render_objects)[(*draw_indices)[i]], info);
				}
				else
				{
					engine->DrawObject(thread_cmd, (*render_objects)[i], info);
				}
			}
			thread_cmd->End();

			primary_cmd->ExecuteCommands(&thread_cmd->vk_command_buffer_, 1);
		}
	};

	bool is_visible(const RenderObject& obj, const glm::mat4& viewproj)
	{
		std::array<glm::vec3, 8> corners{
			glm::vec3{1, 1, 1},
			glm::vec3{1, 1, -1},
			glm::vec3{1, -1, 1},
			glm::vec3{1, -1, -1},
			glm::vec3{-1, 1, 1},
			glm::vec3{-1, 1, -1},
			glm::vec3{-1, -1, 1},
			glm::vec3{-1, -1, -1},
		};

		glm::mat4 matrix = viewproj * obj.transform;

		glm::vec3 min = { 1.5, 1.5, 1.5 };
		glm::vec3 max = { -1.5, -1.5, -1.5 };

		for (int c = 0; c < 8; c++)
		{
			// project the corners of the bounding box into screen space
			glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + corners[c] * obj.bounds.extents, 1.0f);

			// perspective correction
			v.x = v.x / v.w;
			v.y = v.y / v.w;
			v.z = v.z / v.w;

			min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
			max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
		}

		// Check the clip space box is within the view
		if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

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

		InitPipelines();

		InitImGui();

		InitDefaultData();

		InitTaskSystem();

		main_camera_.velocity_ = glm::vec3(0.f);
		main_camera_.position_ = glm::vec3(-10, 1, 0);

		main_camera_.pitch_ = 0;
		main_camera_.yaw_ = 1.5;

		std::vector<std::string> structure_paths = {
			GetAssetPath("assets/structure.glb"),
			GetAssetPath("assets/Sponza/glTF/Sponza.gltf")
		};

		std::vector<std::optional<std::shared_ptr<LoadedGLTF>>> loaded_scenes;
		for (auto& path : structure_paths)
		{
			auto scene = LoadGltf(&gpu_device_, path);
			assert(scene.has_value());
			loaded_scenes.push_back(scene);
			LOGI("Loaded scene: {}", path);
		}

		global_mesh_buffer_.UploadToGPU(this);

		for (size_t i = 0; i < loaded_scenes.size(); ++i)
		{
			loaded_scenes_[std::to_string(i)] = *loaded_scenes[i];
		}

		// everything went fine
		is_initialized_ = true;
	}

	void VulkanEngine::CleanUp()
	{
		if (is_initialized_)
		{
			// make sure the GPU has stopped doing its thing
			vkDeviceWaitIdle(gpu_device_.device_);

			shader_cache_.Clear();
			loaded_scenes_.clear();

			metal_rough_material_.ClearResources();
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
		VK_CHECK(vkWaitForFences(gpu_device_.device_, 1, &gpu_device_.GetCurrentFrame().render_fence, VK_TRUE, UINT64_MAX));

		gpu_device_.GetCurrentFrame().deletion_queue.Flush();
		gpu_device_.GetCurrentFrame().frame_descriptors.ClearPools(gpu_device_.device_);

		VK_CHECK(vkResetFences(gpu_device_.device_, 1, &gpu_device_.GetCurrentFrame().render_fence));

		// request image from the swapchain
		uint32_t swapchain_image_index;
		VkResult e = vkAcquireNextImageKHR(gpu_device_.device_, gpu_device_.swapchain_, UINT64_MAX, gpu_device_.GetCurrentFrame().swapchain_semaphore, VK_NULL_HANDLE, &swapchain_image_index);
		if (e == VK_ERROR_OUT_OF_DATE_KHR)
		{
			resize_requested_ = true;
			return;
		}

		// VkCommandBuffer cmd = gpu_device_.GetCurrentFrame().main_command_buffer;
		gpu_device_.command_buffer_manager_.ResetPools(gpu_device_.current_frame_ % kFRAME_OVERLAP);
		CommandBuffer* cmd = gpu_device_.command_buffer_manager_.GetCommandBuffer(gpu_device_.current_frame_ % kFRAME_OVERLAP, 0, true);

		gpu_device_.draw_extent_.width = static_cast<uint32_t>(std::min(gpu_device_.swapchain_extent_.width, gpu_device_.GetDrawImage()->vk_extent.width) * gpu_device_.render_scale_);
		gpu_device_.draw_extent_.height = static_cast<uint32_t>(std::min(gpu_device_.swapchain_extent_.height, gpu_device_.GetDrawImage()->vk_extent.height) * gpu_device_.render_scale_);

		{
			gpu_device_.profiler_.GrabQueries(cmd->GetCommandBuffer());

			cmd->TransitionImage(gpu_device_.GetDrawImage()->vk_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			DrawBackground(cmd);

			cmd->TransitionImage(gpu_device_.GetDrawImage()->vk_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			cmd->TransitionImage(gpu_device_.GetDepthImage()->vk_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

			DrawGeometry(cmd);

			cmd->TransitionImage(gpu_device_.GetDrawImage()->vk_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			cmd->TransitionImage(gpu_device_.swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			// execute a copy from the draw image into the swapchain
			cmd->CopyImageToImage(gpu_device_.GetDrawImage()->vk_image, gpu_device_.swapchain_images_[swapchain_image_index], gpu_device_.draw_extent_, gpu_device_.swapchain_extent_);
			cmd->TransitionImage(gpu_device_.swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			// draw imgui into the swapchain image
			DrawImGui(cmd, gpu_device_.swapchain_image_views_[swapchain_image_index]);

			cmd->TransitionImage(gpu_device_.swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		cmd->End();

		// prepare the submission to the queue
		VkCommandBufferSubmitInfo cmd_info = vkinit::CommandBufferSubmitInfo(cmd->vk_command_buffer_);

		VkSemaphoreSubmitInfo wait_info = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, gpu_device_.GetCurrentFrame().swapchain_semaphore);
		VkSemaphoreSubmitInfo signal_info = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, gpu_device_.GetCurrentFrame().render_semaphore);

		VkSubmitInfo2 submit_info = vkinit::SubmitInfo(&cmd_info, &signal_info, &wait_info);

		// submit command buffer to the queue and execute it.
		VK_CHECK(vkQueueSubmit2(gpu_device_.graphics_queue_, 1, &submit_info, gpu_device_.GetCurrentFrame().render_fence));

		// prepare present info
		VkPresentInfoKHR presentInfo = vkinit::PresentInfo();
		presentInfo.pSwapchains = &gpu_device_.swapchain_;
		presentInfo.swapchainCount = 1;

		presentInfo.pImageIndices = &swapchain_image_index;

		presentInfo.pWaitSemaphores = &gpu_device_.GetCurrentFrame().render_semaphore;
		presentInfo.waitSemaphoreCount = 1;

		VkResult presentResult = vkQueuePresentKHR(gpu_device_.graphics_queue_, &presentInfo);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			resize_requested_ = true;
		}

		gpu_device_.current_frame_++;
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
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			if (ImGui::Begin("background"))
			{

				ImGui::SliderFloat("Render Scale", &gpu_device_.render_scale_, 0.3f, 1.0f);
				ComputeEffect& selected = background_effects_[current_background_effect_];

				ImGui::Text("Selected effect: ", selected.name);

				ImGui::SliderInt("Effect Index: ", &current_background_effect_, 0, static_cast<int>(background_effects_.size() - 1));
				ImGui::SliderInt("Scene Index: ", &current_scene_, 0, static_cast<int>(loaded_scenes_.size() - 1));


				ImGui::InputFloat4("data1", (float*)&selected.data.data1);
				ImGui::InputFloat4("data2", (float*)&selected.data.data2);
				ImGui::InputFloat4("data3", (float*)&selected.data.data3);
				ImGui::InputFloat4("data4", (float*)&selected.data.data4);
			}
			ImGui::End();

			if (ImGui::Begin("Camera Status"))
			{
				ImGui::Text("Camera Position: %f %f %f", main_camera_.position_.x, main_camera_.position_.y, main_camera_.position_.z);
				ImGui::Text("Camera Pitch: %f", main_camera_.pitch_);
				ImGui::Text("Camera Yaw: %f", main_camera_.yaw_);
			}
			ImGui::End();

			ImGui::Begin("Stats");

			ImGui::Text("frame_time %f ms", engine_stats_.frame_time);
			ImGui::Text("draw time %f ms", engine_stats_.mesh_draw_time);
			ImGui::Text("update time %f ms", engine_stats_.scene_update_time);
			ImGui::Text("triangles %i", engine_stats_.triangle_count);
			ImGui::Text("draws %i", engine_stats_.drawcall_count);

			ImGui::Separator();

			for (auto& [k, v] : gpu_device_.profiler_.timing_)
			{
				ImGui::Text("%s: %f ms", k.c_str(), v);
			}

			ImGui::Separator();

			for (auto& [k, v] : gpu_device_.profiler_.stats_)
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
			ImGui::Render();

			// our draw function
			Draw();

			auto end = std::chrono::system_clock::now();

			// convert to ms
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			engine_stats_.frame_time = elapsed.count() / 1000.f;
		}
	}

	GPUMeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
	{
		const uint32_t vertex_buffer_size = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
		const uint32_t index_buffer_size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

		GPUMeshBuffers new_surface;

		BufferCreation buffer_creation{};
		buffer_creation.Reset()
			.SetName("vertex_buffer")
			.Set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				ResourceUsageType::Immutable,
				vertex_buffer_size)
			.SetDeviceOnly(true);
		new_surface.vertex_buffer_handle = gpu_device_.CreateResource(buffer_creation);

		buffer_creation
			.SetName("Index buffer")
			.Set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				ResourceUsageType::Immutable,
				index_buffer_size);
		new_surface.index_buffer_handle = gpu_device_.CreateResource(buffer_creation);

		// staging buffer
		BufferCreation staging_buffer_creation{};
		staging_buffer_creation.Reset()
			.SetName("Staging Buffer")
			.Set(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ResourceUsageType::Immutable, vertex_buffer_size + index_buffer_size)
			.SetPersistent(true);
		BufferHandle staging_buffer_handle = gpu_device_.CreateResource(staging_buffer_creation);
		Buffer* staging = gpu_device_.GetResource<Buffer>(staging_buffer_handle.index);

		void* data = staging->vma_allocation->GetMappedData();

		memcpy(data, vertices.data(), vertex_buffer_size);
		memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

		gpu_device_.command_buffer_manager_.ImmediateSubmit([&](CommandBuffer* cmd)
			{
				VkBufferCopy vertex_copy{ 0 };
				vertex_copy.dstOffset = 0;
				vertex_copy.srcOffset = 0;
				vertex_copy.size = vertex_buffer_size;

				vkCmdCopyBuffer(cmd->vk_command_buffer_, staging->vk_buffer, gpu_device_.GetResource<Buffer>(new_surface.vertex_buffer_handle.index)->vk_buffer, 1, &vertex_copy);

				VkBufferCopy index_copy{ 0 };
				index_copy.dstOffset = 0;
				index_copy.srcOffset = vertex_buffer_size;
				index_copy.size = index_buffer_size;

				vkCmdCopyBuffer(cmd->vk_command_buffer_, staging->vk_buffer, gpu_device_.GetResource<Buffer>(new_surface.index_buffer_handle.index)->vk_buffer, 1, &index_copy); }, gpu_device_.graphics_queue_);

		// clean up staging buffer
		gpu_device_.DestroyResource(staging_buffer_handle);

		return new_surface;
	}

	void VulkanEngine::InitPipelines()
	{
		InitBackgounrdPipelines();
		metal_rough_material_.BuildPipelines(this);
	}

	void VulkanEngine::InitBackgounrdPipelines()
	{
		ShaderEffect* gradient_effect = shader_cache_.GetShaderEffect("shaders/gradient_color.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		ShaderEffect* sky_effect = shader_cache_.GetShaderEffect("shaders/sky.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		gradient_effect->ReflectLayout(gpu_device_.device_, nullptr, 0);
		sky_effect->ReflectLayout(gpu_device_.device_, nullptr, 0);

		PipelineBuilder builder{};
		builder.SetShaders(gradient_effect);

		ComputeEffect gradient{};
		gradient.name = "gradient";
		gradient.data = {};
		gradient.data.data1 = glm::vec4(1, 0, 0, 1);
		gradient.data.data2 = glm::vec4(0, 0, 1, 1);
		gradient.pipeline = builder.BuildPipeline(gpu_device_.device_, gpu_device_.pipeline_cache_.GetCache());
		gradient.layout = gradient_effect->built_layout_;

		gradient.descriptor_binder.SetShader(gradient_effect);
		gradient.descriptor_binder.BindImage("image",
			{ .sampler = VK_NULL_HANDLE, .imageView = gpu_device_.GetDrawImage()->vk_image_view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL });
		gradient.descriptor_binder.BuildSets(gpu_device_.device_, gpu_device_.descriptor_allocator_);

		builder.SetShaders(sky_effect);
		ComputeEffect sky{};
		sky.name = "sky";
		sky.data = {};
		sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
		sky.pipeline = builder.BuildPipeline(gpu_device_.device_, gpu_device_.pipeline_cache_.GetCache());
		sky.layout = sky_effect->built_layout_;

		sky.descriptor_binder.SetShader(sky_effect);
		sky.descriptor_binder.BindImage("image", { .sampler = VK_NULL_HANDLE, .imageView = gpu_device_.GetDrawImage()->vk_image_view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL });
		sky.descriptor_binder.BuildSets(gpu_device_.device_, gpu_device_.descriptor_allocator_);

		background_effects_.push_back(gradient);
		background_effects_.push_back(sky);

		main_deletion_queue_.PushFunction([&]()
			{
				for (auto& effect : background_effects_) {
					vkDestroyPipeline(gpu_device_.device_, effect.pipeline, nullptr);
					vkDestroyPipelineLayout(gpu_device_.device_, effect.layout, nullptr);
				} });
	}

	PFN_vkVoidFunction imgui_load_func(const char* funtion_name, void* user_data)
	{
		return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), funtion_name);
	}

	void VulkanEngine::InitImGui()
	{
		// 1: create descriptor pool for IMGUI
		//  the size of the pool is very oversize, but it's copied from imgui demo itself.
		VkDescriptorPoolSize pool_sizes[] = { {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
											 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
											 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
											 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
											 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000;
		pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;

		VkDescriptorPool imgui_pool;
		VK_CHECK(vkCreateDescriptorPool(gpu_device_.device_, &pool_info, nullptr, &imgui_pool));

		// 2: initialize imgui library

		// this initializes the core structures of imgui
		ImGui::CreateContext();

		// this initializes imgui for SDL
		ImGui_ImplSDL2_InitForVulkan(window_);

		// this initializes imgui for Vulkan
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = gpu_device_.instance_;
		init_info.PhysicalDevice = gpu_device_.physical_device_;
		init_info.Device = gpu_device_.device_;
		init_info.Queue = gpu_device_.graphics_queue_;
		init_info.DescriptorPool = imgui_pool;
		init_info.MinImageCount = 3;
		init_info.ImageCount = 3;
		init_info.UseDynamicRendering = true;

		// dynamic rendering parameters for imgui to use
		init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
		init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &gpu_device_.swapchain_image_format_;

		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		if (!ImGui_ImplVulkan_LoadFunctions(imgui_load_func, gpu_device_.instance_))
		{
			throw std::runtime_error("Failed to load imgui functions");
		}

		ImGui_ImplVulkan_Init(&init_info);

		ImGui_ImplVulkan_CreateFontsTexture();

		// add the destroy the imgui created structures
		main_deletion_queue_.PushFunction([=]()
			{
				ImGui_ImplVulkan_Shutdown();
				vkDestroyDescriptorPool(gpu_device_.device_, imgui_pool, nullptr); });
	}

	void VulkanEngine::InitDefaultData()
	{
		GLTFMetallic_Roughness::MaterialResources material_resources;
		material_resources.color_image = gpu_device_.default_resources_.images.white_image;
		material_resources.color_sampler = gpu_device_.default_resources_.samplers.linear;
		material_resources.metal_rough_image = gpu_device_.default_resources_.images.white_image;
		material_resources.metal_rought_sampler = gpu_device_.default_resources_.samplers.linear;

		BufferCreation buffer_info{};
		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Immutable, sizeof(GLTFMetallic_Roughness::MaterialConstants))
			.SetPersistent(true);
		BufferHandle material_constants_handle = gpu_device_.CreateResource(buffer_info);
		Buffer* material_constants = gpu_device_.GetResource<Buffer>(material_constants_handle.index);

		// write the buffer
		GLTFMetallic_Roughness::MaterialConstants* scene_uniform_data =
			(GLTFMetallic_Roughness::MaterialConstants*)material_constants->vma_allocation->GetMappedData();
		scene_uniform_data->color_factors = glm::vec4(1, 1, 1, 1);
		scene_uniform_data->metal_rough_factors = glm::vec4(1, 0.5, 0, 0);

		material_resources.data_buffer = material_constants_handle;
		material_resources.data_buffer_offset = 0;

		dafault_data_ = metal_rough_material_.WriteMaterial(MeshPassType::kMainColor, material_resources, gpu_device_.descriptor_allocator_);
	}

	void VulkanEngine::InitTaskSystem()
	{
		// 渲染任务线程池
		enki::TaskSchedulerConfig render_config;
		render_config.numTaskThreadsToCreate = kNUM_RENDER_THREADS - 1;
		render_task_scheduler_.Initialize(render_config);

		// IO 任务线程池
		enki::TaskSchedulerConfig io_config;
		io_config.numTaskThreadsToCreate = 1; // 专门为 IO 任务预留一个线程
		io_task_scheduler_.Initialize(io_config);

		async_loader_.Init(&io_task_scheduler_);

		// 配置 Pinned Task 到 IO 线程池
		run_pinned_task.threadNum = io_task_scheduler_.GetNumTaskThreads() - 1;
		run_pinned_task.task_scheduler = &io_task_scheduler_;
		io_task_scheduler_.AddPinnedTask(&run_pinned_task);

		async_load_task.threadNum = run_pinned_task.threadNum;
		async_load_task.async_loader = &async_loader_;
		async_load_task.task_scheduler = &io_task_scheduler_;
		async_load_task.state = async_loader_.GetState();
		io_task_scheduler_.AddPinnedTask(&async_load_task);

		main_deletion_queue_.PushFunction([&]() {
			async_loader_.Shutdown();
			run_pinned_task.execute = false;
			render_task_scheduler_.WaitforAllAndShutdown();
			io_task_scheduler_.WaitforAllAndShutdown();
			});
	}

	void VulkanEngine::DrawBackground(CommandBuffer* cmd)
	{
		VulkanScopeTimer timer(cmd->GetCommandBuffer(), &gpu_device_.profiler_, "Background");
		VulkanPipelineStatRecorder timers(cmd->GetCommandBuffer(), &gpu_device_.profiler_, "Background Primitives");

		ComputeEffect& effect = background_effects_[current_background_effect_];
		cmd->BindPipeline(effect.pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
		effect.descriptor_binder.ApplyBinds(cmd->vk_command_buffer_, effect.layout);
		cmd->PushConstants(effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
		cmd->Dispatch(static_cast<uint32_t>(std::ceil(gpu_device_.draw_extent_.width / 16.0)), static_cast<uint32_t>(std::ceil(gpu_device_.draw_extent_.height / 16.0)), 1);
	}

	void VulkanEngine::DrawImGui(CommandBuffer* cmd, VkImageView target_image_view)
	{
		VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_.profiler_, "ImGui");
		VulkanPipelineStatRecorder timers(cmd->vk_command_buffer_, &gpu_device_.profiler_, "ImGui Primitives");
		VkRenderingAttachmentInfo color_attachment = vkinit::AttachmentInfo(target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_.swapchain_extent_, &color_attachment, nullptr);

		cmd->BeginRendering(render_info);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->vk_command_buffer_);

		cmd->EndRendering();
	}

	void VulkanEngine::DrawGeometry(CommandBuffer* cmd)
	{
		VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_.profiler_, "Geometry");
		VulkanPipelineStatRecorder timers(cmd->vk_command_buffer_, &gpu_device_.profiler_, "Geometry Primitives");
		// reset counters
		engine_stats_.drawcall_count = 0;
		engine_stats_.triangle_count = 0;

		// begin clock
		auto start = std::chrono::system_clock::now();

		std::vector<uint32_t> opaqueDraws;
		opaqueDraws.reserve(main_draw_context_.opaque_surfaces.size());

		for (uint32_t i = 0; i < main_draw_context_.opaque_surfaces.size(); ++i)
		{
			RenderObject& obj = main_draw_context_.opaque_surfaces[i];
			if (is_visible(obj, gpu_device_.scene_data_.viewproj))
			{
				opaqueDraws.push_back(i);
			}
		}

		// sort the opaque surfaces by material and mesh
		std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&](const auto& iA, const auto& iB)
			{
				const RenderObject& A = main_draw_context_.opaque_surfaces[iA];
				const RenderObject& B = main_draw_context_.opaque_surfaces[iB];
				if (A.material == B.material) {
					return gpu_device_.GetResource<Buffer>(A.index_buffer_handle.index)->vk_buffer < gpu_device_.GetResource<Buffer>(B.index_buffer_handle.index)->vk_buffer;
				}
				else {
					return A.material < B.material;
				} });

				BufferCreation buffer_info{};
				buffer_info.Reset()
					.Set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Immutable, sizeof(GPUSceneData))
					.SetPersistent(true);
				BufferHandle gpu_handle = gpu_device_.CreateResource(buffer_info);
				Buffer* gpu_scene_data_buffer = gpu_device_.GetResource<Buffer>(gpu_handle.index);

				// write the buffer
				GPUSceneData* scene_uniform_data = (GPUSceneData*)gpu_scene_data_buffer->vma_allocation->GetMappedData();
				*scene_uniform_data = gpu_device_.scene_data_;

				// create a descriptor set that binds that buffer and update it
				VkDescriptorSet global_descriptor = gpu_device_.GetCurrentFrame().frame_descriptors.Allocate(gpu_device_.device_, gpu_device_.gpu_scene_data_descriptor_layout_);

				{
					DescriptorWriter writer;
					writer.WriteBuffer(0, gpu_scene_data_buffer->vk_buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
#if LC_DRAW_INDIRECT
					writer.WriteBuffer(1, gpu_device_.GetResource<Buffer>(global_mesh_buffer_.vertex_buffer_handle.index)->vk_buffer, sizeof(Vertex) * global_mesh_buffer_.vertex_data.size(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
#endif // LC_DRAW_INDIRECT
					writer.UpdateSet(gpu_device_.device_, global_descriptor);
				}

				// begin a render pass connected to our draw image
				VkRenderingAttachmentInfo color_attachment = vkinit::AttachmentInfo(gpu_device_.GetDrawImage()->vk_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				VkRenderingAttachmentInfo depth_attachment = vkinit::DepthAttachmentInfo(gpu_device_.GetDepthImage()->vk_image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

				VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_.draw_extent_, &color_attachment, &depth_attachment);
				render_info.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
				cmd->BeginRendering(render_info);

				const uint32_t num_threads = kNUM_RENDER_THREADS;
				std::vector<CommandBuffer*> secondary_cmds(num_threads);

				for (uint32_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
					secondary_cmds[thread_idx] = gpu_device_.command_buffer_manager_.GetSecondaryCommandBuffer(
						gpu_device_.current_frame_ % kFRAME_OVERLAP,
						thread_idx);
				}

				uint32_t obj_count_per_thread = static_cast<uint32_t>((opaqueDraws.size() + num_threads - 1) / num_threads);

				for (uint32_t thread = 0; thread < num_threads; ++thread)
				{
					CommandRecordingTask recordTask;
					recordTask.engine = this;
					recordTask.primary_cmd = cmd;
					recordTask.draw_indices = &opaqueDraws;
					recordTask.thread_count = 1;
					recordTask.frame_index = gpu_device_.current_frame_ % kFRAME_OVERLAP;
					recordTask.global_descriptor = global_descriptor;
					recordTask.bindless_descriptor = gpu_device_.bindless_texture_set_;
					recordTask.draw_extent = gpu_device_.draw_extent_;
					recordTask.render_objects = &main_draw_context_.opaque_surfaces;
					recordTask.start = thread * obj_count_per_thread;
					recordTask.end = std::min((thread + 1) * obj_count_per_thread, (uint32_t)opaqueDraws.size());


					render_task_scheduler_.AddTaskSetToPipe(&recordTask);
					render_task_scheduler_.WaitforTask(&recordTask);
				}

				obj_count_per_thread = static_cast<uint32_t>((main_draw_context_.transparent_surfaces.size() + num_threads - 1) / num_threads);

				for (uint32_t thread = 0; thread < num_threads; ++thread)
				{
					CommandRecordingTask recordTask;
					recordTask.engine = this;
					recordTask.primary_cmd = cmd;
					recordTask.draw_indices = nullptr;
					recordTask.thread_count = 1;
					recordTask.frame_index = gpu_device_.current_frame_ % kFRAME_OVERLAP;
					recordTask.global_descriptor = global_descriptor;
					recordTask.bindless_descriptor = gpu_device_.bindless_texture_set_;
					recordTask.draw_extent = gpu_device_.draw_extent_;
					recordTask.render_objects = &main_draw_context_.transparent_surfaces;
					recordTask.start = thread * obj_count_per_thread;
					recordTask.end = std::min((thread + 1) * obj_count_per_thread, (uint32_t)main_draw_context_.transparent_surfaces.size());

					render_task_scheduler_.AddTaskSetToPipe(&recordTask);
					render_task_scheduler_.WaitforTask(&recordTask);
				}

				cmd->EndRendering();

				auto end = std::chrono::system_clock::now();

				// convert to microseconds (integer), and then come back to miliseconds
				auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				engine_stats_.mesh_draw_time = elapsed.count() / 1000.f;

				main_draw_context_.opaque_surfaces.clear();
				main_draw_context_.transparent_surfaces.clear();
	}

	const std::string VulkanEngine::GetAssetPath(const std::string& path) const
	{
		return std::string("../../" + path);
	}

	void VulkanEngine::DrawObject(CommandBuffer* cmd, const RenderObject& r, RenderInfo& render_info)
	{

		if (r.material != render_info.last_material)
		{
			render_info.last_material = r.material;

			// rebind pipeline and descriptors if the material changed
			if (r.material->pipeline != render_info.last_pipeline)
			{
				render_info.last_pipeline = r.material->pipeline;
				cmd->BindPipeline(r.material->pipeline->pipeline);
				cmd->SetViewport(0, 0, static_cast<float>(gpu_device_.draw_extent_.width), static_cast<float>(gpu_device_.draw_extent_.height), 0.f, 1.f);
				cmd->SetScissor(0, 0, gpu_device_.draw_extent_.width, gpu_device_.draw_extent_.height);
			}

			// bind the material descriptor set
			cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 1, 1, &r.material->set, 0, nullptr);
		}

#if LC_DRAW_INDIRECT
		GPUDrawIndirectPushConstants gpu_draw_indirect_push_constants;
		gpu_draw_indirect_push_constants.world_matrix = r.transform;
		cmd->PushConstants(r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawIndirectPushConstants), &gpu_draw_indirect_push_constants);

		cmd->DrawIndexedIndirect(gpu_device_.GetResource<Buffer>(global_mesh_buffer_.indirect_command_buffer_handle.index)->vk_buffer,
			static_cast<uint32_t>(r.indirect_draw_index * sizeof(VkDrawIndexedIndirectCommand)),
			static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand)));
#else
		// rebind index buffer if needed
		if (gpu_device_.GetResource<Buffer>(r.index_buffer_handle.index).buffer != render_info.last_index_buffer)
		{
			render_info.last_index_buffer = gpu_device_.GetResource<Buffer>(r.index_buffer_handle.index).buffer;
			cmd->BindIndexBuffer(render_info.last_index_buffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants gpu_draw_push_constants;
		gpu_draw_push_constants.vertex_buffer_address = r.vertex_buffer_address;
		gpu_draw_push_constants.world_matrix = r.transform;
		cmd->PushConstants(r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &gpu_draw_push_constants);

		cmd->DrawIndexed(r.index_count, 1, r.first_index, 0, 0);
#endif // LC_DRAW_INDIRECT

		engine_stats_.drawcall_count++;
		engine_stats_.triangle_count += r.index_count / 3;

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
		gpu_device_.scene_data_.proj = glm::perspective(glm::radians(70.f), (float)window_extent_.width / (float)window_extent_.height, 10000.f, 0.1f);

		// invert the Y direction on projection matrix so that we are more similar
		// to opengl and gltf axis
		gpu_device_.scene_data_.proj[1][1] *= -1;
		gpu_device_.scene_data_.viewproj = gpu_device_.scene_data_.proj * gpu_device_.scene_data_.view;

		// some default lighting parameters
		gpu_device_.scene_data_.ambient_color = glm::vec4(.1f);
		gpu_device_.scene_data_.sunlight_color = glm::vec4(1.f);
		gpu_device_.scene_data_.sunlight_direction = glm::vec4(0, 1, 0.5, 1.f);

		std::string scene_name = std::to_string(current_scene_);
		if (loaded_scenes_.find(scene_name) != loaded_scenes_.end())
		{
			loaded_scenes_[scene_name]->Draw(glm::mat4{ 1.0f }, main_draw_context_);
		}

		auto end = std::chrono::system_clock::now();

		// convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		engine_stats_.scene_update_time = elapsed.count() / 1000.f;
	}

	void GLTFMetallic_Roughness::BuildPipelines(VulkanEngine* engine)
	{
		this->engine = engine;
		ShaderEffect* meshEffect = engine->shader_cache_.GetShaderEffect();
#if LC_DRAW_INDIRECT
		meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh_indirect.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
#else
		meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
#endif // LC_DRAW_INDIRECT
		meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.frag.spv"), VK_SHADER_STAGE_FRAGMENT_BIT);

		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

		material_layout = layoutBuilder.Build(engine->gpu_device_.device_, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

		VkDescriptorSetLayout layouts[] = {
			engine->gpu_device_.gpu_scene_data_descriptor_layout_,
			material_layout,
			engine->gpu_device_.bindless_texture_layout_ };

		VkPushConstantRange matrixRange{};
		matrixRange.offset = 0;
#if LC_DRAW_INDIRECT
		matrixRange.size = sizeof(GPUDrawIndirectPushConstants);
#else
		matrixRange.size = sizeof(GPUDrawPushConstants);
#endif // LC_DRAW_INDIRECT
		matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::PipelineLayoutCreateInfo();
		meshLayoutInfo.setLayoutCount = 3;
		meshLayoutInfo.pSetLayouts = layouts;
		meshLayoutInfo.pPushConstantRanges = &matrixRange;
		meshLayoutInfo.pushConstantRangeCount = 1;

		VK_CHECK(vkCreatePipelineLayout(engine->gpu_device_.device_, &meshLayoutInfo, nullptr, &engine->mesh_pipeline_layout_));

		// build the stage-create-info for both vertx and fragment stages
		PipelineBuilder pipelineBuilder;
		pipelineBuilder.SetShaders(meshEffect);
		pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
		pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		pipelineBuilder.SetMultisamplingNone();
		pipelineBuilder.DisableBlending();
		pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

		// render format
		pipelineBuilder.SetColorAttachmentFormat(engine->gpu_device_.GetDrawImage()->vk_format);
		pipelineBuilder.SetDepthFormat(engine->gpu_device_.GetDepthImage()->vk_format);
		pipelineBuilder.pipeline_layout_ = engine->mesh_pipeline_layout_;

		opaque_pipeline.layout = engine->mesh_pipeline_layout_;
		opaque_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->gpu_device_.device_, engine->gpu_device_.pipeline_cache_.GetCache());

		// create the transparent variant
		pipelineBuilder.EnableBlendingAdditive();

		pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

		transparent_pipeline.layout = engine->mesh_pipeline_layout_;
		transparent_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->gpu_device_.device_, engine->gpu_device_.pipeline_cache_.GetCache());
	}

	void GLTFMetallic_Roughness::ClearResources()
	{
		vkDestroyDescriptorSetLayout(engine->gpu_device_.device_, material_layout, nullptr);
		vkDestroyPipelineLayout(engine->gpu_device_.device_, transparent_pipeline.layout, nullptr);

		vkDestroyPipeline(engine->gpu_device_.device_, transparent_pipeline.pipeline, nullptr);
		vkDestroyPipeline(engine->gpu_device_.device_, opaque_pipeline.pipeline, nullptr);
	}

	MaterialInstance GLTFMetallic_Roughness::WriteMaterial(MeshPassType pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptor_allocator)
	{
		MaterialInstance matData;
		matData.pass_type = pass;
		if (pass == MeshPassType::kTransparent)
		{
			matData.pipeline = &transparent_pipeline;
		}
		else
		{
			matData.pipeline = &opaque_pipeline;
		}

		matData.set = descriptor_allocator.Allocate(engine->gpu_device_.device_, material_layout);

		writer.Clear();
		writer.WriteBuffer(0, engine->gpu_device_.GetResource<Buffer>(resources.data_buffer.index)->vk_buffer, sizeof(MaterialConstants), resources.data_buffer_offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.UpdateSet(engine->gpu_device_.device_, matData.set);

		return matData;
	}

	void MeshNode::Draw(const glm::mat4& top_matrix, DrawContext& ctx)
	{
		glm::mat4 node_matrix = top_matrix * world_transform;

		if (mesh.get())
		{
			for (auto& s : mesh->surfaces)
			{
				RenderObject def;
				def.index_count = s.count;
				def.first_index = s.start_index;
				def.index_buffer_handle = mesh->mesh_buffers.index_buffer_handle;
				def.material = &s.material->data;
				def.bounds = s.bounds;
				def.transform = node_matrix;
				def.indirect_draw_index = s.indirect_offset;
				def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;

				if (s.material->data.pass_type == MeshPassType::kTransparent)
					ctx.transparent_surfaces.push_back(def);
				else
					ctx.opaque_surfaces.push_back(def);
			}
		}

		Node::Draw(top_matrix, ctx);
	}

	void GlobalMeshBuffer::UploadToGPU(VulkanEngine* engine)
	{
		LOGI("Uploading mesh data to GPU");
		const uint32_t vertex_buffer_size = static_cast<uint32_t>(vertex_data.size() * sizeof(Vertex));
		const uint32_t index_buffer_size = static_cast<uint32_t>(index_data.size() * sizeof(uint32_t));
		const uint32_t command_buffer_size = static_cast<uint32_t>(indirect_commands.size() * sizeof(VkDrawIndexedIndirectCommand));

		// BufferCreationInfo vertex_buffer_info{};
		// vertex_buffer_info.Reset().Set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		// 	VMA_MEMORY_USAGE_GPU_ONLY,
		// 	vertex_buffer_size);

		BufferCreation buffer_info{};
		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				ResourceUsageType::Immutable,
				vertex_buffer_size)
			.SetDeviceOnly(true);
		vertex_buffer_handle = engine->gpu_device_.CreateResource(buffer_info);

		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				ResourceUsageType::Immutable,
				index_buffer_size)
			.SetDeviceOnly(true);
		index_buffer_handle = engine->gpu_device_.CreateResource(buffer_info);

		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				ResourceUsageType::Immutable,
				command_buffer_size)
			.SetDeviceOnly(true);
		indirect_command_buffer_handle = engine->gpu_device_.CreateResource(buffer_info);

		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				ResourceUsageType::Immutable,
				vertex_buffer_size + index_buffer_size + command_buffer_size)
			.SetPersistent(true);
		BufferHandle staging_buffer_handle = engine->gpu_device_.CreateResource(buffer_info);
		Buffer* staging_buffer = engine->gpu_device_.GetResource<Buffer>(staging_buffer_handle.index);

		void* data = staging_buffer->vma_allocation->GetMappedData();
		memcpy(data, vertex_data.data(), vertex_buffer_size);
		memcpy((char*)data + vertex_buffer_size, index_data.data(), index_buffer_size);
		memcpy((char*)data + vertex_buffer_size + index_buffer_size, indirect_commands.data(), command_buffer_size);

		engine->gpu_device_.command_buffer_manager_.ImmediateSubmit([&](CommandBuffer* cmd)
			{
				VkBufferCopy vertex_copy{ 0 };
				vertex_copy.dstOffset = 0;
				vertex_copy.srcOffset = 0;
				vertex_copy.size = vertex_buffer_size;

				vkCmdCopyBuffer(cmd->vk_command_buffer_, staging_buffer->vk_buffer, engine->gpu_device_.GetResource<Buffer>(vertex_buffer_handle.index)->vk_buffer, 1, &vertex_copy);

				VkBufferCopy index_copy{ 0 };
				index_copy.dstOffset = 0;
				index_copy.srcOffset = vertex_buffer_size;
				index_copy.size = index_buffer_size;

				vkCmdCopyBuffer(cmd->vk_command_buffer_, staging_buffer->vk_buffer, engine->gpu_device_.GetResource<Buffer>(index_buffer_handle.index)->vk_buffer, 1, &index_copy);

				VkBufferCopy command_copy{ 0 };
				command_copy.dstOffset = 0;
				command_copy.srcOffset = vertex_buffer_size + index_buffer_size;
				command_copy.size = command_buffer_size;

				vkCmdCopyBuffer(cmd->vk_command_buffer_, staging_buffer->vk_buffer, engine->gpu_device_.GetResource<Buffer>(indirect_command_buffer_handle.index)->vk_buffer, 1, &command_copy);
			},
			engine->gpu_device_.graphics_queue_);

		engine->gpu_device_.resource_manager_.DestroyBuffer(staging_buffer_handle);

		//engine->gpu_device_.resource_manager_.DestroyBuffer(staging_buffer_handle);
	}

}