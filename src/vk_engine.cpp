//> includes
#include "vk_engine.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>
#include <vk_profiler.h>

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

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }



#if NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

const std::vector<const char*> requiredExtensions = {
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
};

bool is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
	std::array<glm::vec3, 8> corners{
		glm::vec3 { 1, 1, 1 },
		glm::vec3 { 1, 1, -1 },
		glm::vec3 { 1, -1, 1 },
		glm::vec3 { 1, -1, -1 },
		glm::vec3 { -1, 1, 1 },
		glm::vec3 { -1, 1, -1 },
		glm::vec3 { -1, -1, 1 },
		glm::vec3 { -1, -1, -1 },
	};

	glm::mat4 matrix = viewproj * obj.transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };

	for (int c = 0; c < 8; c++) {
		// project the corners of the bounding box into screen space
		glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + corners[c] * obj.bounds.extents, 1.0f);

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min(glm::vec3{ v.x, v.y,v.z }, min);
		max = glm::max(glm::vec3{ v.x, v.y,v.z }, max);
	}

	// Check the clip space box is within the view
	if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
		return false;
	}
	else {
		return true;
	}
}


void VulkanEngine::Init()
{
	spdlog::set_pattern(LOGGER_FORMAT);
	// only one engine initialization is allowed with the application.
	assert(loadedEngine == nullptr);
	loadedEngine = this;

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

	main_camera_.velocity_ = glm::vec3(0.f);
	main_camera_.position_ = glm::vec3(0, 0, 5);

	main_camera_.pitch_ = 0;
	main_camera_.yaw_ = 0;

	std::string structurePath = GetAssetPath("assets/structure.glb");
	auto structureFile = lc::LoadGltf(this, structurePath);

	assert(structureFile.has_value());

	loaded_scenes_["structure"] = *structureFile;

	// everything went fine
	is_initialized_ = true;
}

void VulkanEngine::CleanUp()
{
	if (is_initialized_) {

		// make sure the GPU has stopped doing its thing
		vkDeviceWaitIdle(device_);

		profiler_->CleanUp();
		delete profiler_;

		shader_cache_.Clear();
		loaded_scenes_.clear();

		for (int i = 0; i < kFRAME_OVERLAP; i++) {
			vkDestroyCommandPool(device_, frames_[i].command_pool, nullptr);

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
	loadedEngine = nullptr;
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
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, GetCurrentFrame().swapchain_semaphore, VK_NULL_HANDLE, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested_ = true;
		return;
	}

	VkCommandBuffer cmd = GetCurrentFrame().main_command_buffer;

	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know it may be recorded only once
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	draw_extent_.width = static_cast<uint32_t>(std::min(swapchain_extent_.width, draw_image_.extent.width) * render_scale_);
	draw_extent_.height = static_cast<uint32_t>(std::min(swapchain_extent_.height, draw_image_.extent.height) * render_scale_);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	{
	profiler_->GrabQueries(cmd);

	vkutils::TransitionImageLayout(cmd, draw_image_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	DrawBackground(cmd);

	// make the swapchain image into presentable mode
	vkutils::TransitionImageLayout(cmd, draw_image_.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutils::TransitionImageLayout(cmd, depth_image_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	DrawGeometry(cmd);

	vkutils::TransitionImageLayout(cmd, draw_image_.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutils::TransitionImageLayout(cmd, swapchain_images_[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutils::CopyImageToImage(cmd, draw_image_.image, swapchain_images_[swapchainImageIndex], draw_extent_, swapchain_extent_);

	// transition the swapchain image to be ready for display
	vkutils::TransitionImageLayout(cmd, swapchain_images_[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// draw imgui into the swapchain image
	DrawImGui(cmd, swapchain_image_views_[swapchainImageIndex]);

	vkutils::TransitionImageLayout(cmd, swapchain_images_[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	

	// finish recording the command buffer
	VK_CHECK(vkEndCommandBuffer(cmd));

	// prepare the submission to the queue
	VkCommandBufferSubmitInfo cmdInfo = vkinit::CommandBufferSubmitInfo(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, GetCurrentFrame().swapchain_semaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().render_semaphore);

	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo(&cmdInfo, &signalInfo, &waitInfo);

	// submit command buffer to the queue and execute it.
	VK_CHECK(vkQueueSubmit2(graphics_queue_, 1, &submitInfo, GetCurrentFrame().render_fence));

	// prepare present info
	VkPresentInfoKHR presentInfo = vkinit::PresentInfo();
	presentInfo.pSwapchains = &swapchain_;
	presentInfo.swapchainCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	presentInfo.pWaitSemaphores = &GetCurrentFrame().render_semaphore;
	presentInfo.waitSemaphoreCount = 1;

	VkResult presentResult = vkQueuePresentKHR(graphics_queue_, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested_ = true;
	}

	frame_number++;
}

void VulkanEngine::Run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit) {
		// begin clock
		auto start = std::chrono::system_clock::now();
		// Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_QUIT)
				bQuit = true;

			if (e.type == SDL_WINDOWEVENT) {
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					freeze_rendering_ = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					freeze_rendering_ = false;
				}
			}
			main_camera_.ProcessSdlEvent(e);
			// send SDL events to imgui
			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		// do not draw if we are minimized
		if (freeze_rendering_) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		if (resize_requested_) {
			ResizeSwapchain();
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		if (ImGui::Begin("background"))
		{
			ImGui::SliderFloat("Render Scale", &render_scale_, 0.3f, 1.0f);
			ComputeEffect& selected = background_effects_[current_background_effect_];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index: ", &current_background_effect_, 0, static_cast<int>(background_effects_.size() - 1));

			ImGui::InputFloat4("data1", (float*)&selected.data.data1);
			ImGui::InputFloat4("data2", (float*)&selected.data.data2);
			ImGui::InputFloat4("data3", (float*)&selected.data.data3);
			ImGui::InputFloat4("data4", (float*)&selected.data.data4);
		}
		ImGui::End();

		ImGui::Begin("Stats");

		ImGui::Text("frame_time %f ms", engine_stats_.frame_time);
		ImGui::Text("draw time %f ms", engine_stats_.mesh_draw_time);
		ImGui::Text("update time %f ms", engine_stats_.scene_update_time);
		ImGui::Text("triangles %i", engine_stats_.triangle_count);
		ImGui::Text("draws %i", engine_stats_.drawcall_count);

		ImGui::Separator();

		for (auto &[k,v] : profiler_->timing_) {
			ImGui::Text("%s: %f ms", k.c_str(), v);
		}

		ImGui::Separator();

		for (auto &[k,v] : profiler_->stats_) {
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


		//make imgui calculate internal draw structures
		ImGui::Render();

		// our draw function
		Draw();

		auto end = std::chrono::system_clock::now();

		// convert to ms
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		engine_stats_.frame_time = elapsed.count() / 1000.f;
	}
}

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(device_, 1, &imm_fence_));
	VK_CHECK(vkResetCommandBuffer(imm_command_buffer_, 0));

	VkCommandBuffer& cmd = imm_command_buffer_;

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdInfo = vkinit::CommandBufferSubmitInfo(cmd);
	VkSubmitInfo2 submitInfo = vkinit::SubmitInfo(&cmdInfo, nullptr, nullptr);


	VK_CHECK(vkQueueSubmit2(graphics_queue_, 1, &submitInfo, imm_fence_));

	VK_CHECK(vkWaitForFences(device_, 1, &imm_fence_, VK_TRUE, UINT64_MAX));
}

AllocatedBuffer VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

	return newBuffer;
}

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
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
	VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = new_surface.vertex_buffer.buffer };
	new_surface.vertex_buffer_address = vkGetBufferDeviceAddress(device_, &deviceAddressInfo);

	// create index buffer
	new_surface.index_buffer = CreateBuffer(index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	AllocatedBuffer staging = CreateBuffer(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* data = staging.allocation->GetMappedData();

	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

	ImmediateSubmit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, new_surface.vertex_buffer.buffer, 1, &vertex_copy);

		VkBufferCopy index_copy{ 0 };
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, new_surface.index_buffer.buffer, 1, &index_copy);
		});

	DestroyBuffer(staging);

	return new_surface;
}

AllocatedImage VulkanEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	AllocatedImage newImage;
	newImage.format = format;
	newImage.extent = size;

	VkImageCreateInfo imgInfo = vkinit::ImageCreateInfo(format, usage, size);
	if (mipmapped) {
		imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(allocator_, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(format, newImage.image, aspectFlag);
	viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

	VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &newImage.view));

	return newImage;
}

AllocatedImage VulkanEngine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	size_t dataSize = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadBuffer.info.pMappedData, data, dataSize);

	AllocatedImage newImage = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	ImmediateSubmit([&](VkCommandBuffer cmd) {
		vkutils::TransitionImageLayout(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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
		vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		if (mipmapped) {
			vkutils::GenerateMipmaps(cmd, newImage.image, VkExtent2D{ newImage.extent.width, newImage.extent.height });
		}
		else {
			vkutils::TransitionImageLayout(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		});

	DestroyBuffer(uploadBuffer);

	return newImage;
}

void VulkanEngine::DestroyImage(const AllocatedImage& image)
{
	vkDestroyImageView(device_, image.view, nullptr);
	vmaDestroyImage(allocator_, image.image, image.allocation);
}

void VulkanEngine::InitVulkan()
{
	if (volkInitialize() != VK_SUCCESS) {
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

	if (!inst_ret.has_value()) {
		LOGE("Failed to create Vulkan instance: {}", inst_ret.error().message());
		return;
	}
	vkb::Instance vkb_inst = inst_ret.value();

	// grab the instance
	instance_ = vkb_inst.instance;
	debug_messenger_ = vkb_inst.debug_messenger;

	volkLoadInstance(instance_);

	SDL_Vulkan_CreateSurface(window_, instance_, &surface_);

	// 获取物理设备数量
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

	if (deviceCount == 0) {
		LOGE("Failed to find GPUs with Vulkan support!");
		vkDestroyInstance(instance_, nullptr);
		return;
	}
	// 获取所有物理设备
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

	LOGI("Found {} GPU(s) with Vulkan support:", deviceCount);

	// 输出每个设备的信息以及其支持的扩展
	for (const auto& device : devices) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// 输出设备名称和类型
		LOGI("GPU: {}", deviceProperties.deviceName);
		LOGI("    - Type: {}", deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated" :
			deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete" :
			deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "Virtual" :
			deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU" : "Other");

		// 输出该设备支持的扩展
		// 获取设备支持的扩展数量
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			LOGW("This device does not support any Vulkan extensions.");
			continue;
		}

		// 分配存储扩展属性的空间
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

		// Check if required extensions are supported
		for (const char* extension : requiredExtensions) {
			bool found = false;
			for (const auto& ext : extensions) {
				if (strcmp(extension, ext.extensionName) == 0) {
					found = true;
					LOGI("    - {} (Version: {})", ext.extensionName, ext.specVersion);
					break;
				}
			}
			if (!found) {
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
	

	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;
	features12.descriptorBindingPartiallyBound = true;
	features12.descriptorBindingSampledImageUpdateAfterBind = true;
	features12.descriptorBindingUniformBufferUpdateAfterBind = true;
	features12.runtimeDescriptorArray = true;
	features12.descriptorBindingVariableDescriptorCount = true;
	features12.hostQueryReset = true;

	// Enable pipelineStatisticsQuery in the physical device features
	VkPhysicalDeviceFeatures features10{};
	features10.pipelineStatisticsQuery = VK_TRUE;

	// use vkbootstrap to select a gpu
	// we want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_required_features(features10)
		.set_surface(surface_)
		.add_required_extensions(requiredExtensions)
		.select()
		.value();

	// create the final vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	auto deviceBuilderRet = deviceBuilder.build();
	if (!deviceBuilderRet.has_value()) {
		LOGE("Failed to create Vulkan device: {}", deviceBuilderRet.error().message());
		return;
	}
	vkb::Device vkbDevice = deviceBuilderRet.value();

	// get the vkdevice handle used in the rest of a vulkan application
	device_ = vkbDevice.device;
	volkLoadDevice(device_);
	chosen_gpu_ = physicalDevice.physical_device;
	gpu_properties_ = physicalDevice.properties;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(chosen_gpu_, &deviceProperties);

	LOGI("Vulkan API Version: {}.{}.{}",
		VK_VERSION_MAJOR(deviceProperties.apiVersion),
		VK_VERSION_MINOR(deviceProperties.apiVersion),
		VK_VERSION_PATCH(deviceProperties.apiVersion));

	auto graphicsQueueResult = vkbDevice.get_queue(vkb::QueueType::graphics);
	if (!graphicsQueueResult.has_value()) {
		LOGE("Failed to find a graphics queue!");
		return;
	}
	graphics_queue_ = graphicsQueueResult.value();
	graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	// Init the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = chosen_gpu_;
	allocatorInfo.device = device_;
	allocatorInfo.instance = instance_;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &allocator_);

	main_deletion_queue_.PushFunction([&]() {
		vmaDestroyAllocator(allocator_);
		});

}

void VulkanEngine::InitSwapchain()
{
	CreateSwapchain(window_extent_.width, window_extent_.height);

	// draw image size will match the window
	VkExtent3D drawImageExtent = { window_extent_.width, window_extent_.height, 1 };

	// hardcoding the draw format to 32 bit float
	draw_image_.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	draw_image_.extent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

	VkImageCreateInfo rimg_info = vkinit::ImageCreateInfo(draw_image_.format, drawImageUsages, drawImageExtent);

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
	depth_image_.extent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::ImageCreateInfo(depth_image_.format, depthImageUsages, drawImageExtent);

	// allocate and create the image
	vmaCreateImage(allocator_, &dimg_info, &rimg_allocinfo, &depth_image_.image, &depth_image_.allocation, nullptr);

	VkImageViewCreateInfo dview_info = vkinit::ImageViewCreateInfo(depth_image_.format, depth_image_.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(device_, &dview_info, nullptr, &depth_image_.view));

	// add the image to the deletion queue
	main_deletion_queue_.PushFunction([=]() {
		vkDestroyImageView(device_, draw_image_.view, nullptr);
		vmaDestroyImage(allocator_, draw_image_.image, draw_image_.allocation);

		vkDestroyImageView(device_, depth_image_.view, nullptr);
		vmaDestroyImage(allocator_, depth_image_.image, depth_image_.allocation);
		});

}

void VulkanEngine::InitCommands()
{
	// create command pool for commands submitted to the graphics queue
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::CommandPoolCreateInfo(graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < kFRAME_OVERLAP; ++i)
	{
		VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &frames_[i].command_pool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::CommandBufferAllocateInfo(frames_[i].command_pool, 1);

		VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &frames_[i].main_command_buffer));
	}

	VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &imm_command_pool_));

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::CommandBufferAllocateInfo(imm_command_pool_, 1);

	VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &imm_command_buffer_));

	main_deletion_queue_.PushFunction([=]() {
		vkDestroyCommandPool(device_, imm_command_pool_, nullptr);
		});
}

void VulkanEngine::InitSyncStructures()
{
	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::SemaphoreCreateInfo();

	for (int i = 0; i < kFRAME_OVERLAP; ++i) {
		VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &frames_[i].render_fence));

		VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &frames_[i].render_semaphore));
		VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &frames_[i].swapchain_semaphore));
	}

	VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &imm_fence_));
	main_deletion_queue_.PushFunction([=]() {
		vkDestroyFence(device_, imm_fence_, nullptr);
		});
}

void VulkanEngine::InitDescriptors()
{
	std::vector<lc::DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMAX_BINDLESS_RESOURCES}
	};

	global_descriptor_allocator_.Init(device_, 2000, sizes);

	{
		lc::DescriptorLayoutBuilder builder;
		// 确保绑定点11有足够的描述符数量
		builder.AddBinding(kBINDLESS_TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMAX_BINDLESS_RESOURCES);

		bindless_texture_layout_ = builder.Build(device_,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			nullptr,
			VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

		// 分配描述符集时需要指定变长数组的大小
		VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
			.descriptorSetCount = 1,
			.pDescriptorCounts = &kMAX_BINDLESS_RESOURCES
		};

		bindless_texture_set_ = global_descriptor_allocator_.Allocate(device_, bindless_texture_layout_, &countInfo);
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

	//make sure both the descriptor allocator and the new layout get cleaned up properly
	main_deletion_queue_.PushFunction([&]() {
		global_descriptor_allocator_.DestroyPools(device_);

		vkDestroyDescriptorSetLayout(device_, draw_image_descriptor_layout_, nullptr);

		vkDestroyDescriptorSetLayout(device_, bindless_texture_layout_, nullptr);
		});

	for (int i = 0; i < kFRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<lc::DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		frames_[i].frame_descriptors = lc::DescriptorAllocatorGrowable{};
		frames_[i].frame_descriptors.Init(device_, 1000, frameSizes);

		main_deletion_queue_.PushFunction([&, i]() {
			frames_[i].frame_descriptors.DestroyPools(device_);
			});
	}

	{
		lc::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		gpu_scene_data_descriptor_layout_ = builder.Build(device_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		main_deletion_queue_.PushFunction([&]() {
			vkDestroyDescriptorSetLayout(device_, gpu_scene_data_descriptor_layout_, nullptr);
			});
	}
}

void VulkanEngine::InitPipelines()
{
	global_pipeline_cache_ = new lc::PipelineCache(device_, cache_file_path);

	InitBackgounrdPipelines();
	metal_rough_material_.BuildPipelines(this);

	main_deletion_queue_.PushFunction([&]() {
		global_pipeline_cache_->SaveCache();
		delete global_pipeline_cache_;
		});
}

void VulkanEngine::InitBackgounrdPipelines()
{
	lc::ShaderEffect* gradientEffect = shader_cache_.GetShaderEffect("shaders/gradient_color.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
	lc::ShaderEffect* skyEffect = shader_cache_.GetShaderEffect("shaders/sky.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
	gradientEffect->ReflectLayout(device_, nullptr, 0);
	skyEffect->ReflectLayout(device_, nullptr, 0);


	lc::PipelineBuilder builder{};
	builder.SetShaders(gradientEffect);

	ComputeEffect gradient{};
	gradient.name = "gradient";
	gradient.data = {};
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);
	gradient.pipeline = builder.BuildPipeline(device_,global_pipeline_cache_->GetCache());
	gradient.layout = gradientEffect->built_layout_;

	gradient.descriptor_binder.SetShader(gradientEffect);
	gradient.descriptor_binder.BindImage("image", { .sampler = VK_NULL_HANDLE, .imageView = draw_image_.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL });
	gradient.descriptor_binder.BuildSets(device_, global_descriptor_allocator_);

	builder.SetShaders(skyEffect);
	ComputeEffect sky{};
	sky.name = "sky";
	sky.data = {};
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
	sky.pipeline = builder.BuildPipeline(device_, global_pipeline_cache_->GetCache());
	sky.layout = skyEffect->built_layout_;


	sky.descriptor_binder.SetShader(skyEffect);
	sky.descriptor_binder.BindImage("image", { .sampler = VK_NULL_HANDLE, .imageView = draw_image_.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL });
	sky.descriptor_binder.BuildSets(device_, global_descriptor_allocator_);

	background_effects_.push_back(gradient);
	background_effects_.push_back(sky);

	main_deletion_queue_.PushFunction([&]() {
		for (auto& effect : background_effects_) {
			vkDestroyPipeline(device_, effect.pipeline, nullptr);
			vkDestroyPipelineLayout(device_, effect.layout, nullptr);
		}
		});
}

PFN_vkVoidFunction imgui_load_func(const char* funtion_name, void* user_data) {
	return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), funtion_name);
}

void VulkanEngine::InitImGui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(device_, &pool_info, nullptr, &imguiPool));

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
	init_info.Queue = graphics_queue_;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_image_format_;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	if (!ImGui_ImplVulkan_LoadFunctions(imgui_load_func, instance_)) {
		throw std::runtime_error("Failed to load imgui functions");
	}

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	main_deletion_queue_.PushFunction([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(device_, imguiPool, nullptr);
		});

}

void VulkanEngine::InitDefaultData()
{

	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	default_images_.white_image = CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66, 0.66, 0.66, 1));
	default_images_.grey_image = CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	default_images_.black_image = CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16> pixels;// for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	default_images_.error_checker_board_image = CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(device_, &samplerCreateInfo, nullptr, &default_samplers_.nearest);

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

	vkCreateSampler(device_, &samplerCreateInfo, nullptr, &default_samplers_.linear);

	main_deletion_queue_.PushFunction([=]() {
		vkDestroySampler(device_, default_samplers_.nearest, nullptr);
		vkDestroySampler(device_, default_samplers_.linear, nullptr);

		DestroyImage(default_images_.white_image);
		DestroyImage(default_images_.grey_image);
		DestroyImage(default_images_.black_image);
		DestroyImage(default_images_.error_checker_board_image);

		});

	GLTFMetallic_Roughness::MaterialResources materialResources;
	materialResources.color_image = default_images_.white_image;
	materialResources.color_sampler = default_samplers_.linear;
	materialResources.metal_rough_image = default_images_.white_image;
	materialResources.metal_rought_sampler = default_samplers_.linear;

	// set the uniform buffer for the material data
	AllocatedBuffer materialConstants = CreateBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	// write the buffer
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
	sceneUniformData->color_factors = glm::vec4(1, 1, 1, 1);
	sceneUniformData->metal_rough_factors = glm::vec4(1, 0.5, 0, 0);

	main_deletion_queue_.PushFunction([=, this]() {
		DestroyBuffer(materialConstants);
		});

	materialResources.data_buffer = materialConstants.buffer;
	materialResources.data_buffer_offset = 0;

	dafault_data_ = metal_rough_material_.WriteMaterial(device_, MaterialPass::kMainColor, materialResources, global_descriptor_allocator_);


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
	vkb::SwapchainBuilder swapchainBuilder{ chosen_gpu_, device_, surface_ };

	swapchain_image_format_ = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.set_desired_format(VkSurfaceFormatKHR{ .format = swapchain_image_format_ ,.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
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

	for (int i = 0; i < swapchain_image_views_.size(); i++) {
		vkDestroyImageView(device_, swapchain_image_views_[i], nullptr);
	}
}

void VulkanEngine::DrawBackground(VkCommandBuffer cmd)
{
	vkutils::VulkanScopeTimer timer(cmd, profiler_, "Background");
	vkutils::VulkanPipelineStatRecorder timers(cmd, profiler_, "Background Primitives");

	// make a clear-color from frame number, this will flash with a 120 frame period
	VkClearColorValue clearValue;
	float flush = std::abs(std::sin(frame_number / 120.0f));
	clearValue = { { 0.0f, 0.0f, flush, 1.0f } };

	VkImageSubresourceRange subresourceRange = vkinit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	ComputeEffect& effect = background_effects_[current_background_effect_];

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	/*DescriptorInfo drawImageDesc(draw_image_.view, VK_IMAGE_LAYOUT_GENERAL);
	DescriptorInfo descs[] = { drawImageDesc };
	vkCmdPushDescriptorSetWithTemplateKHR(cmd, effect.program.updateTemplate, effect.program.layout,0, descs);*/
	effect.descriptor_binder.ApplyBinds(cmd, effect.layout);

	vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(draw_extent_.width / 16.0)), static_cast<uint32_t>(std::ceil(draw_extent_.height / 16.0)), 1);
}

void VulkanEngine::DrawImGui(VkCommandBuffer cmd, VkImageView target_image_view)
{
	vkutils::VulkanScopeTimer timer(cmd, profiler_, "ImGui");
	vkutils::VulkanPipelineStatRecorder timers(cmd, profiler_, "ImGui Primitives");
	VkRenderingAttachmentInfo colorAttachment = vkinit::AttachmentInfo(target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::RenderingInfo(swapchain_extent_, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VulkanEngine::DrawGeometry(VkCommandBuffer cmd)
{
	vkutils::VulkanScopeTimer timer(cmd, profiler_, "Geometry");
	vkutils::VulkanPipelineStatRecorder timers(cmd, profiler_, "Geometry Primitives");
	// reset counters
	engine_stats_.drawcall_count = 0;
	engine_stats_.triangle_count = 0;

	// begin clock
	auto start = std::chrono::system_clock::now();

	std::vector<uint32_t> opaqueDraws;
	opaqueDraws.reserve(main_draw_context_.opaque_surfaces.size());

	Frustum frustum(scene_data_.viewproj);

	for (uint32_t i = 0; i < main_draw_context_.opaque_surfaces.size(); ++i) {
		RenderObject& obj = main_draw_context_.opaque_surfaces[i];
		//if (frustum.isSphereVisible(obj.bounds.origin, obj.bounds.sphereRadius)) {
		if (is_visible(obj, scene_data_.viewproj)) {
			opaqueDraws.push_back(i);
		}
		//}
	}

	// sort the opaque surfaces by material and mesh
	std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = main_draw_context_.opaque_surfaces[iA];
		const RenderObject& B = main_draw_context_.opaque_surfaces[iB];
		if (A.material == B.material) {
			return A.index_buffer < B.index_buffer;
		}
		else {
			return A.material < B.material;
		}
		});

	// begin a render pass connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::AttachmentInfo(draw_image_.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::DepthAttachmentInfo(depth_image_.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::RenderingInfo(draw_extent_, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	// vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	GetCurrentFrame().deletion_queue.PushFunction([=, this]() {
		DestroyBuffer(gpuSceneDataBuffer);
		});

	//write the buffer
	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = scene_data_;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = GetCurrentFrame().frame_descriptors.Allocate(device_, gpu_scene_data_descriptor_layout_);

	{
		lc::DescriptorWriter writer;
		writer.WriteBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.UpdateSet(device_, globalDescriptor);
	}

	// this is the state we will try to skip
	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout_, 2, 1, &bindless_texture_set_, 0, nullptr);
	auto Draw = [&](const RenderObject& r) {
		if (r.material != lastMaterial) {
			lastMaterial = r.material;

			// rebind pipeline and descriptors if the material changed
			if (r.material->pipeline != lastPipeline) {
				lastPipeline = r.material->pipeline;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);
				

				// set dynamic viewport and scissor
				VkViewport viewport = {};
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = static_cast<float>(draw_extent_.width);
				viewport.height = static_cast<float>(draw_extent_.height);
				viewport.minDepth = 0.f;
				viewport.maxDepth = 1.f;
				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = draw_extent_.width;
				scissor.extent.height = draw_extent_.height;
				vkCmdSetScissor(cmd, 0, 1, &scissor);
			}

			// bind the material descriptor set
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, &r.material->set, 0, nullptr);
		}
		// rebind index buffer if needed
		if (r.index_buffer != lastIndexBuffer) {
			lastIndexBuffer = r.index_buffer;
			vkCmdBindIndexBuffer(cmd, r.index_buffer, 0, VK_INDEX_TYPE_UINT32);
		}
		// calculate final mesh matrix
		GPUDrawPushConstants pushConstants;
		pushConstants.vertex_buffer_address = r.vertex_buffer_address;
		pushConstants.world_matrix = r.transform;
		vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);

		engine_stats_.drawcall_count++;
		engine_stats_.triangle_count += r.index_count / 3;
		};

	for (auto& r : opaqueDraws) {
		Draw(main_draw_context_.opaque_surfaces[r]);
	}

	for (auto& r : main_draw_context_.transparent_surfaces) {
		Draw(r);
	}

	vkCmdEndRendering(cmd);

	auto end = std::chrono::system_clock::now();

	//convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	engine_stats_.mesh_draw_time = elapsed.count() / 1000.f;

	main_draw_context_.opaque_surfaces.clear();
	main_draw_context_.transparent_surfaces.clear();
}

const std::string VulkanEngine::GetAssetPath(const std::string& path) const
{
	return std::string("../../" + path);
}

void VulkanEngine::UpdateScene()
{
	engine_stats_.scene_update_time = 0;

	auto start = std::chrono::system_clock::now();

	main_draw_context_.opaque_surfaces.clear();

	main_camera_.Update();

	scene_data_.view = main_camera_.GetViewMatrix();
	// camera projection
	scene_data_.proj = glm::perspective(glm::radians(70.f), (float)window_extent_.width / (float)window_extent_.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	scene_data_.proj[1][1] *= -1;
	scene_data_.viewproj = scene_data_.proj * scene_data_.view;

	//some default lighting parameters
	scene_data_.ambient_color = glm::vec4(.1f);
	scene_data_.sunlight_color = glm::vec4(1.f);
	scene_data_.sunlight_direction = glm::vec4(0, 1, 0.5, 1.f);

	loaded_scenes_["structure"]->Draw(glm::mat4{ 1.f }, main_draw_context_);

	auto end = std::chrono::system_clock::now();

	//convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	engine_stats_.scene_update_time = elapsed.count() / 1000.f;
}

void GLTFMetallic_Roughness::BuildPipelines(VulkanEngine* engine)
{
	lc::ShaderEffect* meshEffect = engine->shader_cache_.GetShaderEffect();
	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.frag.spv"), VK_SHADER_STAGE_FRAGMENT_BIT);

	lc::DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	material_layout = layoutBuilder.Build(engine->device_, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

	VkDescriptorSetLayout layouts[] = {
		engine->gpu_scene_data_descriptor_layout_,
		material_layout,
		engine->bindless_texture_layout_
	};

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
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

	// use the triangle layout we created
	//pipelineBuilder._pipelineLayout = meshEffect->built_layout_;
	opaque_pipeline.layout = engine->mesh_pipeline_layout_;
	opaque_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->device_,engine->global_pipeline_cache_->GetCache());

	// create the transparent variant
	pipelineBuilder.EnableBlendingAdditive();

	pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparent_pipeline.layout = engine->mesh_pipeline_layout_;
	transparent_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->device_,engine->global_pipeline_cache_->GetCache());

	
}

void GLTFMetallic_Roughness::ClearResources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, material_layout, nullptr);
	vkDestroyPipelineLayout(device, transparent_pipeline.layout, nullptr);

	vkDestroyPipeline(device, transparent_pipeline.pipeline, nullptr);
	vkDestroyPipeline(device, opaque_pipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, lc::DescriptorAllocatorGrowable& descriptor_allocator)
{
	MaterialInstance matData;
	matData.pass_type = pass;
	if (pass == MaterialPass::kTransparent) {
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

void MeshNode::Draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
	glm::mat4 node_matrix = top_matrix * world_transform;

	if (mesh.get())
	{
		for (auto& s : mesh->surfaces) {
			RenderObject def;
			def.index_count = s.count;
			def.first_index = s.start_index;
			def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
			def.material = &s.material->data;
			def.bounds = s.bounds;
			def.transform = node_matrix;
			def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;

			if (s.material->data.pass_type == MaterialPass::kTransparent)
				ctx.transparent_surfaces.push_back(def);
			else
				ctx.opaque_surfaces.push_back(def);
		}
	}


	Node::Draw(top_matrix, ctx);
}
