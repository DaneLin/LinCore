//> includes
#include "vk_engine.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_pipelines.h>
#include <vk_profiler.h>
#include <vk_loader.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <chrono>
#include <thread>
#include <glm/gtx/transform.hpp>

#include <cvars.h>
#include <logging.h>

#include <volk.h>
#include "frustum_cull.h"

AutoCVar_Float CVAR_DrawDistance("gpu.drawDistance", "Distance cull", 5000);

VulkanEngine *loaded_engine = nullptr;

VulkanEngine &VulkanEngine::Get() { return *loaded_engine; }

bool is_visible(const RenderObject &obj, const glm::mat4 &viewproj)
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

	glm::vec3 min = {1.5, 1.5, 1.5};
	glm::vec3 max = {-1.5, -1.5, -1.5};

	for (int c = 0; c < 8; c++)
	{
		// project the corners of the bounding box into screen space
		glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + corners[c] * obj.bounds.extents, 1.0f);

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
		max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
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

	InitVulkan();

	profiler_ = new vkutils::VulkanProfiler();

	profiler_->Init(device_, gpu_properties_.limits.timestampPeriod);

	InitSwapchain();

	InitCommands();

	InitSyncStructures();

	InitDescriptors();

	InitPipelines();

	InitImGui();

	InitDefaultData();

	InitTaskSystem();

	main_camera_.velocity_ = glm::vec3(0.f);
	main_camera_.position_ = glm::vec3(0, 0, 5);

	main_camera_.pitch_ = 0;
	main_camera_.yaw_ = 0;

	//std::string structure_path = GetAssetPath("assets/structure.glb");
	std::string structure_path = GetAssetPath("assets/Sponza/glTF/Sponza.gltf");
	auto structure_file = lc::LoadGltf(this, structure_path);

	assert(structure_file.has_value());

	global_mesh_buffer_.UploadToGPU(this);
	main_deletion_queue_.PushFunction([=](){
		DestroyBuffer(global_mesh_buffer_.vertex_buffer);
		DestroyBuffer(global_mesh_buffer_.index_buffer);
		DestroyBuffer(global_mesh_buffer_.indirect_command_buffer); });

	loaded_scenes_["structure"] = *structure_file;

	// everything went fine
	is_initialized_ = true;
}

void VulkanEngine::CleanUp()
{
	if (is_initialized_)
	{
		// make sure the GPU has stopped doing its thing
		vkDeviceWaitIdle(device_);

		profiler_->CleanUp();
		delete profiler_;

		command_buffer_manager_.Shutdown();

		shader_cache_.Clear();
		loaded_scenes_.clear();

		for (int i = 0; i < kFRAME_OVERLAP; i++)
		{
			vkDestroyFence(device_, frames_[i].render_fence, nullptr);
			vkDestroySemaphore(device_, frames_[i].render_semaphore, nullptr);
			vkDestroySemaphore(device_, frames_[i].swapchain_semaphore, nullptr);

			frames_[i].deletion_queue.Flush();
		}

		metal_rough_material_.ClearResources(device_);
		main_deletion_queue_.Flush();

		DestroySwapchain();

		vkDestroySurfaceKHR(instance_, surface_, nullptr);
		vkDestroyDevice(device_, nullptr);

		vkb::destroy_debug_utils_messenger(instance_, debug_messenger_, nullptr);
		vkDestroyInstance(instance_, nullptr);

		SDL_DestroyWindow(window_);
	}

	// clear engine pointer
	loaded_engine = nullptr;
}

void VulkanEngine::Draw()
{
	UpdateScene();
	// wait until the gpu has finished rendering the last frame
	VK_CHECK(vkWaitForFences(device_, 1, &GetCurrentFrame().render_fence, VK_TRUE, UINT64_MAX));

	GetCurrentFrame().deletion_queue.Flush();
	GetCurrentFrame().frame_descriptors.ClearPools(device_);

	VK_CHECK(vkResetFences(device_, 1, &GetCurrentFrame().render_fence));

	// request image from the swapchain
	uint32_t swapchain_image_index;
	VkResult e = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, GetCurrentFrame().swapchain_semaphore, VK_NULL_HANDLE, &swapchain_image_index);
	if (e == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resize_requested_ = true;
		return;
	}

	// VkCommandBuffer cmd = GetCurrentFrame().main_command_buffer;
	command_buffer_manager_.ResetPools(frame_number_ % kFRAME_OVERLAP);
	CommandBuffer *cmd = command_buffer_manager_.GetCommandBuffer(frame_number_ % kFRAME_OVERLAP, 0, true);

	draw_extent_.width = static_cast<uint32_t>(std::min(swapchain_extent_.width, draw_image_.extent.width) * render_scale_);
	draw_extent_.height = static_cast<uint32_t>(std::min(swapchain_extent_.height, draw_image_.extent.height) * render_scale_);

	{
		profiler_->GrabQueries(cmd->GetCommandBuffer());

		cmd->TransitionImage(draw_image_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

		DrawBackground(cmd);

		cmd->TransitionImage(draw_image_.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		cmd->TransitionImage(depth_image_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

		DrawGeometry(cmd);

		cmd->TransitionImage(draw_image_.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		cmd->TransitionImage(swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// execute a copy from the draw image into the swapchain
		cmd->CopyImageToImage(draw_image_.image, swapchain_images_[swapchain_image_index], draw_extent_, swapchain_extent_);
		cmd->TransitionImage(swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		// draw imgui into the swapchain image
		DrawImGui(cmd, swapchain_image_views_[swapchain_image_index]);

		cmd->TransitionImage(swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	cmd->End();

	// prepare the submission to the queue
	VkCommandBufferSubmitInfo cmd_info = vkinit::CommandBufferSubmitInfo(cmd->command_buffer_);

	VkSemaphoreSubmitInfo wait_info = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, GetCurrentFrame().swapchain_semaphore);
	VkSemaphoreSubmitInfo signal_info = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().render_semaphore);

	VkSubmitInfo2 submit_info = vkinit::SubmitInfo(&cmd_info, &signal_info, &wait_info);

	// submit command buffer to the queue and execute it.
	VK_CHECK(vkQueueSubmit2(main_queue_, 1, &submit_info, GetCurrentFrame().render_fence));

	// prepare present info
	VkPresentInfoKHR presentInfo = vkinit::PresentInfo();
	presentInfo.pSwapchains = &swapchain_;
	presentInfo.swapchainCount = 1;

	presentInfo.pImageIndices = &swapchain_image_index;

	presentInfo.pWaitSemaphores = &GetCurrentFrame().render_semaphore;
	presentInfo.waitSemaphoreCount = 1;

	VkResult presentResult = vkQueuePresentKHR(main_queue_, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resize_requested_ = true;
	}

	frame_number_++;
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
			ResizeSwapchain();
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		if (ImGui::Begin("background"))
		{
			ImGui::SliderFloat("Render Scale", &render_scale_, 0.3f, 1.0f);
			ComputeEffect &selected = background_effects_[current_background_effect_];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index: ", &current_background_effect_, 0, static_cast<int>(background_effects_.size() - 1));

			ImGui::InputFloat4("data1", (float *)&selected.data.data1);
			ImGui::InputFloat4("data2", (float *)&selected.data.data2);
			ImGui::InputFloat4("data3", (float *)&selected.data.data3);
			ImGui::InputFloat4("data4", (float *)&selected.data.data4);
		}
		ImGui::End();

		ImGui::Begin("Stats");

		ImGui::Text("frame_time %f ms", engine_stats_.frame_time);
		ImGui::Text("draw time %f ms", engine_stats_.mesh_draw_time);
		ImGui::Text("update time %f ms", engine_stats_.scene_update_time);
		ImGui::Text("triangles %i", engine_stats_.triangle_count);
		ImGui::Text("draws %i", engine_stats_.drawcall_count);

		ImGui::Separator();

		for (auto &[k, v] : profiler_->timing_)
		{
			ImGui::Text("%s: %f ms", k.c_str(), v);
		}

		ImGui::Separator();

		for (auto &[k, v] : profiler_->stats_)
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

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function)
{
	command_buffer_manager_.ImmediateSubmit([&](CommandBuffer *cmd)
											{ function(cmd->GetCommandBuffer()); });
}

AllocatedBufferUntyped VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
	AllocatedBufferUntyped newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

	return newBuffer;
}

void VulkanEngine::DestroyBuffer(const AllocatedBufferUntyped &buffer)
{
	vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertex_buffer_size = vertices.size() * sizeof(Vertex);
	const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers new_surface;

	// create the vertex buffer
	new_surface.vertex_buffer = CreateBuffer(vertex_buffer_size,
											 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
											 VMA_MEMORY_USAGE_GPU_ONLY);

	// find the address of the vertex buffer
	VkBufferDeviceAddressInfo deviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = new_surface.vertex_buffer.buffer};
	new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(device_, &deviceAddressInfo);

	// create index buffer
	new_surface.index_buffer = CreateBuffer(index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBufferUntyped staging = CreateBuffer(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void *data = staging.allocation->GetMappedData();

	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy((char *)data + vertex_buffer_size, indices.data(), index_buffer_size);

	ImmediateSubmit([&](VkCommandBuffer cmd)
					{
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, new_surface.vertex_buffer.buffer, 1, &vertex_copy);

		VkBufferCopy index_copy{ 0 };
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, new_surface.index_buffer.buffer, 1, &index_copy); });

	DestroyBuffer(staging);

	return new_surface;
}

AllocatedImage VulkanEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	AllocatedImage new_image;
	new_image.format = format;
	new_image.extent = size;

	VkImageCreateInfo imgInfo = vkinit::ImageCreateInfo(format, usage, size);
	if (mipmapped)
	{
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(allocator_, &imgInfo, &allocInfo, &new_image.image, &new_image.allocation, nullptr));

	VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT)
	{
		aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::ImageViewCreateInfo(format, new_image.image, aspect_flag);
	view_info.subresourceRange.levelCount = imgInfo.mipLevels;

	VK_CHECK(vkCreateImageView(device_, &view_info, nullptr, &new_image.view));

	return new_image;
}

AllocatedImage VulkanEngine::CreateImage(void *data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBufferUntyped upload_buffer = CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(upload_buffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	command_buffer_manager_.ImmediateSubmit(([&](CommandBuffer *cmd)
											 {
			cmd->TransitionImage(new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			VkBufferImageCopy copy_region{};
			copy_region.bufferOffset = 0;
			copy_region.bufferRowLength = 0;
			copy_region.bufferImageHeight = 0;

			copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy_region.imageSubresource.mipLevel = 0;
			copy_region.imageSubresource.baseArrayLayer = 0;
			copy_region.imageSubresource.layerCount = 1;
			copy_region.imageExtent = size;

			// copy the buffer into the image
			vkCmdCopyBufferToImage(cmd->command_buffer_, upload_buffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

			if (mipmapped)
			{
				cmd->GenerateMipmaps(new_image.image, VkExtent2D{ new_image.extent.width, new_image.extent.height });
			}
			else
			{
				cmd->TransitionImage(new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			} }));

	DestroyBuffer(upload_buffer);

	return new_image;
}

void VulkanEngine::DestroyImage(const AllocatedImage &image)
{
	vkDestroyImageView(device_, image.view, nullptr);
	vmaDestroyImage(allocator_, image.image, image.allocation);
}

void VulkanEngine::InitVulkan()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		LOGE("Failed to initialize volk");
		return;
	}
	vkb::InstanceBuilder builder;

	// make the vulkan instance with basic debug features
	auto inst_ret = builder.set_app_name("Lindot")
						.request_validation_layers(bUseValidationLayers)
						.use_default_debug_messenger()
						.require_api_version(1, 3, 0)
						.build();

	if (!inst_ret.has_value())
	{
		LOGE("Failed to create Vulkan instance: {}", inst_ret.error().message());
		return;
	}
	vkb::Instance vkb_inst = inst_ret.value();

	// grab the instance
	instance_ = vkb_inst.instance;
	debug_messenger_ = vkb_inst.debug_messenger;

	volkLoadInstance(instance_);

	SDL_Vulkan_CreateSurface(window_, instance_, &surface_);

	// get the list of physical devices
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

	if (device_count == 0)
	{
		LOGE("Failed to find GPUs with Vulkan support!");
		vkDestroyInstance(instance_, nullptr);
		return;
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

	LOGI("Found {} GPU(s) with Vulkan support:", device_count);

	// outputs the information of the available devices
	for (const auto &device : devices)
	{
		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(device, &device_properties);

		// outputs the device info
		LOGI("GPU: {}", device_properties.deviceName);
		LOGI("    - Type: {}", device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated" : device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete"
																												   : device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU	? "Virtual"
																												   : device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU			? "CPU"
																																															: "Other");

		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

		if (extension_count == 0)
		{
			LOGW("This device does not support any Vulkan extensions.");
			continue;
		}

		std::vector<VkExtensionProperties> extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

		// Check if required extensions are supported
		for (const char *extension : required_extensions)
		{
			bool found = false;
			for (const auto &ext : extensions)
			{
				if (strcmp(extension, ext.extensionName) == 0)
				{
					found = true;
					LOGI("    - {} (Version: {})", ext.extensionName, ext.specVersion);
					break;
				}
			}
			if (!found)
			{
				LOGE("Extension {} is not supported!", extension);
			}
		}
	}

	// vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
	};
	features.dynamicRendering = true;
	features.synchronization2 = true;

	// vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features_12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	features_12.bufferDeviceAddress = true;
	features_12.descriptorIndexing = true;
	features_12.descriptorBindingPartiallyBound = true;
	features_12.descriptorBindingSampledImageUpdateAfterBind = true;
	features_12.descriptorBindingUniformBufferUpdateAfterBind = true;
	features_12.runtimeDescriptorArray = true;
	features_12.descriptorBindingVariableDescriptorCount = true;
	features_12.hostQueryReset = true;
	features_12.drawIndirectCount = true;

	// Enable pipelineStatisticsQuery in the physical device features
	VkPhysicalDeviceFeatures features_10{};
	features_10.pipelineStatisticsQuery = true;
	features_10.multiDrawIndirect = true;
	features_10.geometryShader = true;

	// use vkbootstrap to select a gpu
	// we want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	vkb::PhysicalDevice physical_device = selector
											  .set_minimum_version(1, 3)
											  .set_required_features_13(features)
											  .set_required_features_12(features_12)
											  .set_required_features(features_10)
											  .set_surface(surface_)
											  .add_required_extensions(required_extensions)
											  .select()
											  .value();

	// create the final vulkan device
	vkb::DeviceBuilder device_builder{physical_device};

	auto device_build_ret = device_builder.build();
	if (!device_build_ret.has_value())
	{
		LOGE("Failed to create Vulkan device: {}", device_build_ret.error().message());
		return;
	}
	vkb::Device vkb_device = device_build_ret.value();

	// get the vkdevice handle used in the rest of a vulkan application
	device_ = vkb_device.device;
	volkLoadDevice(device_);
	chosen_gpu_ = physical_device.physical_device;
	gpu_properties_ = physical_device.properties;

	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(chosen_gpu_, &device_properties);

	LOGI("Vulkan API Version: {}.{}.{}",
		 VK_VERSION_MAJOR(device_properties.apiVersion),
		 VK_VERSION_MINOR(device_properties.apiVersion),
		 VK_VERSION_PATCH(device_properties.apiVersion));

	auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
	if (!graphics_queue_ret.has_value())
	{
		LOGE("Failed to find a graphics queue!");
		return;
	}
	main_queue_ = graphics_queue_ret.value();
	main_queue_family_ = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	auto transfer_queue_ret = vkb_device.get_queue(vkb::QueueType::transfer);
	if (!transfer_queue_ret.has_value())
	{
		LOGE("Failed to find a transfer queue!");
		return;
	}

	transfer_queue_ = transfer_queue_ret.value();
	transfer_queue_family_ = vkb_device.get_queue_index(vkb::QueueType::transfer).value();

	VmaVulkanFunctions vulkan_functions = {};
	vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	// Init the memory allocator
	VmaAllocatorCreateInfo allocator_create_info = {};
	allocator_create_info.physicalDevice = chosen_gpu_;
	allocator_create_info.device = device_;
	allocator_create_info.instance = instance_;
	allocator_create_info.pVulkanFunctions = &vulkan_functions;
	allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocator_create_info, &allocator_);

	main_deletion_queue_.PushFunction([&]()
									  { vmaDestroyAllocator(allocator_); });
}

void VulkanEngine::InitSwapchain()
{
	CreateSwapchain(window_extent_.width, window_extent_.height);

	// draw image size will match the window
	VkExtent3D draw_image_extent = {window_extent_.width, window_extent_.height, 1};

	// hardcoding the draw format to 32 bit float
	draw_image_.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	draw_image_.extent = draw_image_extent;

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;

	VkImageCreateInfo rimg_info = vkinit::ImageCreateInfo(draw_image_.format, draw_image_usages, draw_image_extent);

	// for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo{};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	vmaCreateImage(allocator_, &rimg_info, &rimg_allocinfo, &draw_image_.image, &draw_image_.allocation, nullptr);

	// build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::ImageViewCreateInfo(draw_image_.format, draw_image_.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device_, &rview_info, nullptr, &draw_image_.view));

	depth_image_.format = VK_FORMAT_D32_SFLOAT;
	depth_image_.extent = draw_image_extent;
	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::ImageCreateInfo(depth_image_.format, depth_image_usages, draw_image_extent);

	// allocate and create the image
	vmaCreateImage(allocator_, &dimg_info, &rimg_allocinfo, &depth_image_.image, &depth_image_.allocation, nullptr);

	VkImageViewCreateInfo dview_info = vkinit::ImageViewCreateInfo(depth_image_.format, depth_image_.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(device_, &dview_info, nullptr, &depth_image_.view));

	// add the image to the deletion queue
	main_deletion_queue_.PushFunction([=]()
									  {
		vkDestroyImageView(device_, draw_image_.view, nullptr);
		vmaDestroyImage(allocator_, draw_image_.image, draw_image_.allocation);

		vkDestroyImageView(device_, depth_image_.view, nullptr);
		vmaDestroyImage(allocator_, depth_image_.image, depth_image_.allocation); });
}

void VulkanEngine::InitCommands()
{
	// initialize commandbuffermanager, suppose to use 4 threads
	command_buffer_manager_.Init(4);
}

void VulkanEngine::InitSyncStructures()
{
	// create syncronization structures
	// one fence to control when the gpu has finished rendering the frame,
	// and 2 semaphores to syncronize rendering with swapchain
	// we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fence_create_info = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphore_create_info = vkinit::SemaphoreCreateInfo();

	for (int i = 0; i < kFRAME_OVERLAP; ++i)
	{
		VK_CHECK(vkCreateFence(device_, &fence_create_info, nullptr, &frames_[i].render_fence));

		VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &frames_[i].render_semaphore));
		VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &frames_[i].swapchain_semaphore));
	}
}

void VulkanEngine::InitDescriptors()
{
	std::vector<lc::DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMAX_BINDLESS_RESOURCES}};

	global_descriptor_allocator_.Init(device_, 2000, sizes);

	{
		lc::DescriptorLayoutBuilder builder;
		// make sure the binding number matches the shader layout
		builder.AddBinding(kBINDLESS_TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMAX_BINDLESS_RESOURCES);

		bindless_texture_layout_ = builder.Build(device_,
												 VK_SHADER_STAGE_FRAGMENT_BIT,
												 nullptr,
												 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

		// allocate a descriptor set for our bindless textures
		VkDescriptorSetVariableDescriptorCountAllocateInfo count_allocate_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
			.descriptorSetCount = 1,
			.pDescriptorCounts = &kMAX_BINDLESS_RESOURCES};

		bindless_texture_set_ = global_descriptor_allocator_.Allocate(device_, bindless_texture_layout_, &count_allocate_info);
		texture_cache_.SetDescriptorSet(bindless_texture_set_);
	}

	// make the descriptor set layout for out compute draw
	{
		lc::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		draw_image_descriptor_layout_ = builder.Build(device_, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	// allocate a descriptor set for our draw image
	draw_image_descriptor_ = global_descriptor_allocator_.Allocate(device_, draw_image_descriptor_layout_);

	{
		lc::DescriptorWriter writer;
		writer.WriteImage(0, draw_image_.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		writer.UpdateSet(device_, draw_image_descriptor_);
	}

	// make sure both the descriptor allocator and the new layout get cleaned up properly
	main_deletion_queue_.PushFunction([&]()
									  {
		global_descriptor_allocator_.DestroyPools(device_);

		vkDestroyDescriptorSetLayout(device_, draw_image_descriptor_layout_, nullptr);

		vkDestroyDescriptorSetLayout(device_, bindless_texture_layout_, nullptr); });

	for (int i = 0; i < kFRAME_OVERLAP; i++)
	{
		// create a descriptor pool
		std::vector<lc::DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		frames_[i].frame_descriptors = lc::DescriptorAllocatorGrowable{};
		frames_[i].frame_descriptors.Init(device_, 1000, frameSizes);

		main_deletion_queue_.PushFunction([&, i]()
										  { frames_[i].frame_descriptors.DestroyPools(device_); });
	}

	{
		lc::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
#if LC_DRAW_INDIRECT
		builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
#endif
		gpu_scene_data_descriptor_layout_ = builder.Build(device_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		main_deletion_queue_.PushFunction([&]()
										  { vkDestroyDescriptorSetLayout(device_, gpu_scene_data_descriptor_layout_, nullptr); });
	}
}

void VulkanEngine::InitPipelines()
{
	global_pipeline_cache_ = new lc::PipelineCache(device_, cache_file_path);

	InitBackgounrdPipelines();
	metal_rough_material_.BuildPipelines(this);

	main_deletion_queue_.PushFunction([&]()
									  {
		global_pipeline_cache_->SaveCache();
		delete global_pipeline_cache_; });
}

void VulkanEngine::InitBackgounrdPipelines()
{
	lc::ShaderEffect *gradient_effect = shader_cache_.GetShaderEffect("shaders/gradient_color.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
	lc::ShaderEffect *sky_effect = shader_cache_.GetShaderEffect("shaders/sky.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
	gradient_effect->ReflectLayout(device_, nullptr, 0);
	sky_effect->ReflectLayout(device_, nullptr, 0);

	lc::PipelineBuilder builder{};
	builder.SetShaders(gradient_effect);

	ComputeEffect gradient{};
	gradient.name = "gradient";
	gradient.data = {};
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);
	gradient.pipeline = builder.BuildPipeline(device_, global_pipeline_cache_->GetCache());
	gradient.layout = gradient_effect->built_layout_;

	gradient.descriptor_binder.SetShader(gradient_effect);
	gradient.descriptor_binder.BindImage("image", {.sampler = VK_NULL_HANDLE, .imageView = draw_image_.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL});
	gradient.descriptor_binder.BuildSets(device_, global_descriptor_allocator_);

	builder.SetShaders(sky_effect);
	ComputeEffect sky{};
	sky.name = "sky";
	sky.data = {};
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
	sky.pipeline = builder.BuildPipeline(device_, global_pipeline_cache_->GetCache());
	sky.layout = sky_effect->built_layout_;

	sky.descriptor_binder.SetShader(sky_effect);
	sky.descriptor_binder.BindImage("image", {.sampler = VK_NULL_HANDLE, .imageView = draw_image_.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL});
	sky.descriptor_binder.BuildSets(device_, global_descriptor_allocator_);

	background_effects_.push_back(gradient);
	background_effects_.push_back(sky);

	main_deletion_queue_.PushFunction([&]()
									  {
		for (auto& effect : background_effects_) {
			vkDestroyPipeline(device_, effect.pipeline, nullptr);
			vkDestroyPipelineLayout(device_, effect.layout, nullptr);
		} });
}

PFN_vkVoidFunction imgui_load_func(const char *funtion_name, void *user_data)
{
	return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), funtion_name);
}

void VulkanEngine::InitImGui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imgui_pool;
	VK_CHECK(vkCreateDescriptorPool(device_, &pool_info, nullptr, &imgui_pool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(window_);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = instance_;
	init_info.PhysicalDevice = chosen_gpu_;
	init_info.Device = device_;
	init_info.Queue = main_queue_;
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	// dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_image_format_;

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	if (!ImGui_ImplVulkan_LoadFunctions(imgui_load_func, instance_))
	{
		throw std::runtime_error("Failed to load imgui functions");
	}

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	main_deletion_queue_.PushFunction([=]()
									  {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(device_, imgui_pool, nullptr); });
}

void VulkanEngine::InitDefaultData()
{

	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	default_images_.white_image = CreateImage((void *)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66, 0.66, 0.66, 1));
	default_images_.grey_image = CreateImage((void *)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	default_images_.black_image = CreateImage((void *)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16> pixels; // for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++)
	{
		for (int y = 0; y < 16; y++)
		{
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	default_images_.error_checker_board_image = CreateImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampler_create_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampler_create_info.magFilter = VK_FILTER_NEAREST;
	sampler_create_info.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(device_, &sampler_create_info, nullptr, &default_samplers_.nearest);

	sampler_create_info.magFilter = VK_FILTER_LINEAR;
	sampler_create_info.minFilter = VK_FILTER_LINEAR;

	vkCreateSampler(device_, &sampler_create_info, nullptr, &default_samplers_.linear);

	main_deletion_queue_.PushFunction([=]()
									  {
										  vkDestroySampler(device_, default_samplers_.nearest, nullptr);
										  vkDestroySampler(device_, default_samplers_.linear, nullptr);

										  DestroyImage(default_images_.white_image);
										  DestroyImage(default_images_.grey_image);
										  DestroyImage(default_images_.black_image);
										  DestroyImage(default_images_.error_checker_board_image); });

	GLTFMetallic_Roughness::MaterialResources material_resources;
	material_resources.color_image = default_images_.white_image;
	material_resources.color_sampler = default_samplers_.linear;
	material_resources.metal_rough_image = default_images_.white_image;
	material_resources.metal_rought_sampler = default_samplers_.linear;

	// set the uniform buffer for the material data
	AllocatedBufferUntyped material_constants = CreateBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	// write the buffer
	GLTFMetallic_Roughness::MaterialConstants *scene_uniform_data = (GLTFMetallic_Roughness::MaterialConstants *)material_constants.allocation->GetMappedData();
	scene_uniform_data->color_factors = glm::vec4(1, 1, 1, 1);
	scene_uniform_data->metal_rough_factors = glm::vec4(1, 0.5, 0, 0);

	main_deletion_queue_.PushFunction([=, this]()
									  { DestroyBuffer(material_constants); });

	material_resources.data_buffer = material_constants.buffer;
	material_resources.data_buffer_offset = 0;

	dafault_data_ = metal_rough_material_.WriteMaterial(device_, MeshPassType::kMainColor, material_resources, global_descriptor_allocator_);
}

void VulkanEngine::InitTaskSystem()
{
	enki::TaskSchedulerConfig config;
	// In this example we create more threads than the hardware can run,
	// because the IO thread will spend most of it's time idle or blocked
	// and therefore not scheduled for CPU time by the OS
	config.numTaskThreadsToCreate += 1;
	task_scheduler_.Initialize(task_config_);

	async_loader_.Init(&task_scheduler_);
	// Create IO threads at the end

	run_pinned_task.threadNum = task_scheduler_.GetNumTaskThreads() - 1;
	run_pinned_task.task_scheduler = &task_scheduler_;
	task_scheduler_.AddPinnedTask(&run_pinned_task);

	// Create the actual task responsible for asynchronous loading
	// associating it with the same thread as the pinned task
	async_load_task.threadNum = run_pinned_task.threadNum;
	async_load_task.async_loader = &async_loader_;
	async_load_task.task_scheduler = &task_scheduler_;
	task_scheduler_.AddPinnedTask(&async_load_task);

	main_deletion_queue_.PushFunction([&]()
									  {
		run_pinned_task.execute = false;
		async_load_task.execute = false;
		async_loader_.Shutdown(); });
}

void VulkanEngine::ResizeSwapchain()
{
	vkDeviceWaitIdle(device_);

	DestroySwapchain();

	int w, h;
	SDL_GetWindowSize(window_, &w, &h);
	window_extent_.width = w;
	window_extent_.height = h;

	CreateSwapchain(window_extent_.width, window_extent_.height);

	resize_requested_ = false;
}

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchain_builder{chosen_gpu_, device_, surface_};

	swapchain_image_format_ = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchain_builder
									  .set_desired_format(VkSurfaceFormatKHR{.format = swapchain_image_format_, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
									  .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
									  .set_desired_extent(width, height)
									  .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
									  .build()
									  .value();

	swapchain_extent_ = vkbSwapchain.extent;
	// store swapchain and images
	swapchain_ = vkbSwapchain.swapchain;
	swapchain_images_ = vkbSwapchain.get_images().value();
	swapchain_image_views_ = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain()
{
	vkDestroySwapchainKHR(device_, swapchain_, nullptr);

	for (int i = 0; i < swapchain_image_views_.size(); i++)
	{
		vkDestroyImageView(device_, swapchain_image_views_[i], nullptr);
	}
}

void VulkanEngine::DrawBackground(CommandBuffer *cmd)
{
	vkutils::VulkanScopeTimer timer(cmd->GetCommandBuffer(), profiler_, "Background");
	vkutils::VulkanPipelineStatRecorder timers(cmd->GetCommandBuffer(), profiler_, "Background Primitives");

	ComputeEffect &effect = background_effects_[current_background_effect_];
	cmd->BindPipeline(effect.pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
	effect.descriptor_binder.ApplyBinds(cmd->command_buffer_, effect.layout);
	cmd->PushConstants(effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	cmd->Dispatch(static_cast<uint32_t>(std::ceil(draw_extent_.width / 16.0)), static_cast<uint32_t>(std::ceil(draw_extent_.height / 16.0)), 1);
}

void VulkanEngine::DrawImGui(CommandBuffer *cmd, VkImageView target_image_view)
{
	vkutils::VulkanScopeTimer timer(cmd->command_buffer_, profiler_, "ImGui");
	vkutils::VulkanPipelineStatRecorder timers(cmd->command_buffer_, profiler_, "ImGui Primitives");
	VkRenderingAttachmentInfo color_attachment = vkinit::AttachmentInfo(target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo render_info = vkinit::RenderingInfo(swapchain_extent_, &color_attachment, nullptr);

	cmd->BeginRendering(render_info);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->command_buffer_);

	cmd->EndRendering();
}

void VulkanEngine::DrawGeometry(CommandBuffer *cmd)
{
	vkutils::VulkanScopeTimer timer(cmd->command_buffer_, profiler_, "Geometry");
	vkutils::VulkanPipelineStatRecorder timers(cmd->command_buffer_, profiler_, "Geometry Primitives");
	// reset counters
	engine_stats_.drawcall_count = 0;
	engine_stats_.triangle_count = 0;

	// begin clock
	auto start = std::chrono::system_clock::now();

	std::vector<uint32_t> opaqueDraws;
	opaqueDraws.reserve(main_draw_context_.opaque_surfaces.size());

	Frustum frustum(scene_data_.viewproj);

	for (uint32_t i = 0; i < main_draw_context_.opaque_surfaces.size(); ++i)
	{
		RenderObject &obj = main_draw_context_.opaque_surfaces[i];
		if (is_visible(obj, scene_data_.viewproj))
		{
			opaqueDraws.push_back(i);
		}
	}

	// sort the opaque surfaces by material and mesh
	std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&](const auto &iA, const auto &iB)
			  {
		const RenderObject& A = main_draw_context_.opaque_surfaces[iA];
		const RenderObject& B = main_draw_context_.opaque_surfaces[iB];
		if (A.material == B.material) {
			return A.index_buffer < B.index_buffer;
		}
		else {
			return A.material < B.material;
		} });

	// begin a render pass connected to our draw image
	VkRenderingAttachmentInfo color_attachment = vkinit::AttachmentInfo(draw_image_.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depth_attachment = vkinit::DepthAttachmentInfo(depth_image_.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo render_info = vkinit::RenderingInfo(draw_extent_, &color_attachment, &depth_attachment);
	cmd->BeginRendering(render_info);

	// allocate a new uniform buffer for the scene data
	AllocatedBufferUntyped gpu_scene_data_buffer = CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	// add it to the deletion queue of this frame so it gets deleted once its been used
	GetCurrentFrame().deletion_queue.PushFunction([=, this]()
												  { DestroyBuffer(gpu_scene_data_buffer); });

	// write the buffer
	GPUSceneData *scene_uniform_data = (GPUSceneData *)gpu_scene_data_buffer.allocation->GetMappedData();
	*scene_uniform_data = scene_data_;

	// create a descriptor set that binds that buffer and update it
	VkDescriptorSet global_descriptor = GetCurrentFrame().frame_descriptors.Allocate(device_, gpu_scene_data_descriptor_layout_);

	{
		lc::DescriptorWriter writer;
		writer.WriteBuffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
#if LC_DRAW_INDIRECT
		writer.WriteBuffer(1, global_mesh_buffer_.vertex_buffer.buffer, sizeof(Vertex) * global_mesh_buffer_.vertex_data.size(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
#endif // LC_DRAW_INDIRECT
		writer.UpdateSet(device_, global_descriptor);
	}

#if LC_DRAW_INDIRECT
	{
		cmd->BindIndexBuffer(global_mesh_buffer_.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	}
#endif // LC_DRAW_INDIRECT
	// this is the state we will try to skip
	MaterialPipeline *last_pipeline = nullptr;
	MaterialInstance *last_material = nullptr;
	VkBuffer last_index_buffer = VK_NULL_HANDLE;

	cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 0, 1, &global_descriptor, 0, nullptr);
	cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 2, 1, &bindless_texture_set_, 0, nullptr);

	auto Draw = [&](const RenderObject &r)
	{
		if (r.material != last_material)
		{
			last_material = r.material;

			// rebind pipeline and descriptors if the material changed
			if (r.material->pipeline != last_pipeline)
			{
				last_pipeline = r.material->pipeline;
				cmd->BindPipeline(r.material->pipeline->pipeline);
				cmd->SetViewport(0, 0, static_cast<float>(draw_extent_.width), static_cast<float>(draw_extent_.height), 0.f, 1.f);
				cmd->SetScissor(0, 0, draw_extent_.width, draw_extent_.height);
			}

			// bind the material descriptor set
			cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 1, 1, &r.material->set, 0, nullptr);
		}

#if LC_DRAW_INDIRECT
		GPUDrawIndirectPushConstants gpu_draw_indirect_push_constants;
		gpu_draw_indirect_push_constants.world_matrix = r.transform;
		cmd->PushConstants(r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawIndirectPushConstants), &gpu_draw_indirect_push_constants);

		cmd->DrawIndexedIndirect(global_mesh_buffer_.indirect_command_buffer.buffer, r.indirect_draw_index * sizeof(VkDrawIndexedIndirectCommand), sizeof(VkDrawIndexedIndirectCommand));
#else
		// rebind index buffer if needed
		if (r.index_buffer != last_index_buffer)
		{
			last_index_buffer = r.index_buffer;
			cmd->BindIndexBuffer(r.index_buffer, 0, VK_INDEX_TYPE_UINT32);
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
	};

	for (auto &r : opaqueDraws)
	{
		Draw(main_draw_context_.opaque_surfaces[r]);
	}

	for (auto &r : main_draw_context_.transparent_surfaces)
	{
		Draw(r);
	}

	cmd->EndRendering();

	auto end = std::chrono::system_clock::now();

	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	engine_stats_.mesh_draw_time = elapsed.count() / 1000.f;

	main_draw_context_.opaque_surfaces.clear();
	main_draw_context_.transparent_surfaces.clear();
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

	scene_data_.view = main_camera_.GetViewMatrix();
	// camera projection
	scene_data_.proj = glm::perspective(glm::radians(70.f), (float)window_extent_.width / (float)window_extent_.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	scene_data_.proj[1][1] *= -1;
	scene_data_.viewproj = scene_data_.proj * scene_data_.view;

	// some default lighting parameters
	scene_data_.ambient_color = glm::vec4(.1f);
	scene_data_.sunlight_color = glm::vec4(1.f);
	scene_data_.sunlight_direction = glm::vec4(0, 1, 0.5, 1.f);

	loaded_scenes_["structure"]->Draw(glm::mat4{1.0f}, main_draw_context_);

	auto end = std::chrono::system_clock::now();

	// convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	engine_stats_.scene_update_time = elapsed.count() / 1000.f;
}

void GLTFMetallic_Roughness::BuildPipelines(VulkanEngine *engine)
{
	lc::ShaderEffect *meshEffect = engine->shader_cache_.GetShaderEffect();
#if LC_DRAW_INDIRECT
	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh_indirect.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
#else
	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
#endif // LC_DRAW_INDIRECT
	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.frag.spv"), VK_SHADER_STAGE_FRAGMENT_BIT);

	lc::DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	material_layout = layoutBuilder.Build(engine->device_, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

	VkDescriptorSetLayout layouts[] = {
		engine->gpu_scene_data_descriptor_layout_,
		material_layout,
		engine->bindless_texture_layout_};

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

	VK_CHECK(vkCreatePipelineLayout(engine->device_, &meshLayoutInfo, nullptr, &engine->mesh_pipeline_layout_));

	// build the stage-create-info for both vertx and fragment stages
	lc::PipelineBuilder pipelineBuilder;
	pipelineBuilder.SetShaders(meshEffect);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.SetMultisamplingNone();
	pipelineBuilder.DisableBlending();
	pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	// render format
	pipelineBuilder.SetColorAttachmentFormat(engine->draw_image_.format);
	pipelineBuilder.SetDepthFormat(engine->depth_image_.format);
	pipelineBuilder.pipeline_layout_ = engine->mesh_pipeline_layout_;

	opaque_pipeline.layout = engine->mesh_pipeline_layout_;
	opaque_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->device_, engine->global_pipeline_cache_->GetCache());

	// create the transparent variant
	pipelineBuilder.EnableBlendingAdditive();

	pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparent_pipeline.layout = engine->mesh_pipeline_layout_;
	transparent_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->device_, engine->global_pipeline_cache_->GetCache());
}

void GLTFMetallic_Roughness::ClearResources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, material_layout, nullptr);
	vkDestroyPipelineLayout(device, transparent_pipeline.layout, nullptr);

	vkDestroyPipeline(device, transparent_pipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaque_pipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::WriteMaterial(VkDevice device, MeshPassType pass, const MaterialResources &resources, lc::DescriptorAllocatorGrowable &descriptor_allocator)
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

	matData.set = descriptor_allocator.Allocate(device, material_layout);

	writer.Clear();
	writer.WriteBuffer(0, resources.data_buffer, sizeof(MaterialConstants), resources.data_buffer_offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.UpdateSet(device, matData.set);

	return matData;
}

void MeshNode::Draw(const glm::mat4 &top_matrix, DrawContext &ctx)
{
	glm::mat4 node_matrix = top_matrix * world_transform;

	if (mesh.get())
	{
		for (auto &s : mesh->surfaces)
		{
			RenderObject def;
			def.index_count = s.count;
			def.first_index = s.start_index;
			def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
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

void GlobalMeshBuffer::UploadToGPU(VulkanEngine *engine)
{
	LOGI("Uploading mesh data to GPU");
	size_t vertex_buffer_size = vertex_data.size() * sizeof(Vertex);
	size_t index_buffer_size = index_data.size() * sizeof(uint32_t);

	vertex_buffer = engine->CreateBuffer(vertex_buffer_size,
										 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
										 VMA_MEMORY_USAGE_GPU_ONLY);

	index_buffer = engine->CreateBuffer(index_buffer_size,
										VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBufferUntyped staging_buffer = engine->CreateBuffer(vertex_buffer_size + index_buffer_size,
																 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																 VMA_MEMORY_USAGE_CPU_ONLY);

	void *data = staging_buffer.allocation->GetMappedData();
	memcpy(data, vertex_data.data(), vertex_buffer_size);
	memcpy((char *)data + vertex_buffer_size, index_data.data(), index_buffer_size);

	engine->ImmediateSubmit([&](VkCommandBuffer cmd)
							{
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		vkCmdCopyBuffer(cmd, staging_buffer.buffer, vertex_buffer.buffer, 1, &vertex_copy);

		VkBufferCopy index_copy{ 0 };
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		vkCmdCopyBuffer(cmd, staging_buffer.buffer, index_buffer.buffer, 1, &index_copy); });

	engine->DestroyBuffer(staging_buffer);

	size_t command_buffer_size = indirect_commands.size() * sizeof(VkDrawIndexedIndirectCommand);

	indirect_command_buffer = engine->CreateBuffer(command_buffer_size,
												   VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
												   VMA_MEMORY_USAGE_GPU_ONLY);

	staging_buffer = engine->CreateBuffer(command_buffer_size,
										  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
										  VMA_MEMORY_USAGE_CPU_ONLY);

	data = staging_buffer.allocation->GetMappedData();
	memcpy(data, indirect_commands.data(), command_buffer_size);

	engine->ImmediateSubmit([&](VkCommandBuffer cmd)
							{
		VkBufferCopy copy_region{};
		copy_region.srcOffset = 0;
		copy_region.dstOffset = 0;
		copy_region.size = command_buffer_size;

		vkCmdCopyBuffer(cmd, staging_buffer.buffer, indirect_command_buffer.buffer, 1, &copy_region); });

	engine->DestroyBuffer(staging_buffer);
}
