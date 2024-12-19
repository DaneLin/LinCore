
#include "vk_device.h"
#include <iostream>
#include "logging.h"
#include <set>
#include <vector>
#include <algorithm>
#include <SDL_vulkan.h>
#include <volk/volk.h>
#include <SDL.h>
#include <glm/packing.hpp>

#include "vk_pipelines.h"

namespace lincore
{
	bool GpuDevice::Init(const CreateInfo& create_info) {

		if (!InitVulkan(create_info)) {
			return false;
		}

		resource_manager_.Init(this);

		if (!InitSwapchain()) {
			return false;
		}

		
		command_buffer_manager_.Init(this, kNUM_RENDER_THREADS);
		pipeline_cache_.Init(device_, cache_file_path);
		profiler_.Init(device_, properties_.limits.timestampPeriod);

		InitDefaultResources();
		InitDescriptors();
		InitFrameDatas();

		return true;
	}

	void GpuDevice::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = this;  // Pass the GpuDevice instance to the callback
	}

	void GpuDevice::SetupDebugMessenger() {
		if (!bUseValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debug_messenger_));
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL GpuDevice::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		auto* device = static_cast<GpuDevice*>(pUserData);
		std::string message = pCallbackData->pMessage;
		std::string objectNames;

		// 查找相关对象的名称
		if (pCallbackData->objectCount > 0 && device) {
			std::lock_guard<std::mutex> lock(device->debug_mutex_);
			for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
				const auto& obj = pCallbackData->pObjects[i];
				auto it = device->debug_names_.find(obj.objectHandle);
				if (it != device->debug_names_.end()) {
					objectNames += "\n  - Object: " + it->second.name;
					if (obj.pObjectName) {
						objectNames += " (" + std::string(obj.pObjectName) + ")";
					}
				}
			}
		}

		// 根据消息严重程度选择输出方式
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			LOGE("Validation Error: {}{}", message, objectNames);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			LOGW("Validation Warning: {}{}", message, objectNames);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			LOGI("Validation Info: {}{}", message, objectNames);
		}
		else {
			LOGD("Validation Debug: {}{}", message, objectNames);
		}

		return VK_FALSE;
	}

	void GpuDevice::SetDebugName(VkObjectType type, uint64_t handle, const char* name) {
		if (!bUseValidationLayers || !name) return;

		// 保存对象名称到映射中
		{
			std::lock_guard<std::mutex> lock(debug_mutex_);
			debug_names_[handle] = DebugInfo{
				.handle = handle,
				.type = type,
				.name = name
			};
		}

		// 设置Vulkan对象的调试名称
		VkDebugUtilsObjectNameInfoEXT nameInfo{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = type,
			.objectHandle = handle,
			.pObjectName = name
		};

		vkSetDebugUtilsObjectNameEXT(device_, &nameInfo);
	}


	void GpuDevice::InitDescriptors()
	{
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> default_ratios = {
		   {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0.1f},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0.1f},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.1f},
		   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.7f},
		};
		descriptor_allocator_.Init(device_, kINITIAL_DESCRIPTOR_POOL_SIZE, default_ratios);
		{
			DescriptorLayoutBuilder builder;
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
				.pDescriptorCounts = &kMAX_BINDLESS_RESOURCES };

			bindless_texture_set_ = descriptor_allocator_.Allocate(device_, bindless_texture_layout_, &count_allocate_info);
			texture_cache_.SetDescriptorSet(bindless_texture_set_);
		}

		// make sure both the descriptor allocator and the new layout get cleaned up properly
		main_deletion_queue_.PushFunction([&]()
			{
				descriptor_allocator_.DestroyPools(device_);

				vkDestroyDescriptorSetLayout(device_, bindless_texture_layout_, nullptr); });

		{
			DescriptorLayoutBuilder builder;
			builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
#if LC_DRAW_INDIRECT
			builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
#endif
			gpu_scene_data_descriptor_layout_ = builder.Build(device_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

			main_deletion_queue_.PushFunction([&]()
				{ vkDestroyDescriptorSetLayout(device_, gpu_scene_data_descriptor_layout_, nullptr); });
		}
	}

	void GpuDevice::InitFrameDatas()
	{
		VkFenceCreateInfo fence_create_info = vkinit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
		VkSemaphoreCreateInfo semaphore_create_info = vkinit::SemaphoreCreateInfo();

		for (int i = 0; i < kFRAME_OVERLAP; ++i)
		{
			VK_CHECK(vkCreateFence(device_, &fence_create_info, nullptr, &frames_[i].render_fence));
			VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &frames_[i].render_semaphore));
			VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &frames_[i].swapchain_semaphore));
		}

		for (int i = 0; i < kFRAME_OVERLAP; i++)
		{
			// create a descriptor pool
			std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
				{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0.1f},
				{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0.1f},
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.1f},
				{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.7f},
			};

			frames_[i].frame_descriptors = DescriptorAllocatorGrowable{};
			frames_[i].frame_descriptors.Init(device_, 1000, frameSizes);

			main_deletion_queue_.PushFunction([&, i]()
				{ frames_[i].frame_descriptors.DestroyPools(device_); });
		}
	}

	void GpuDevice::LogAvailableDevices() {
		uint32_t device_count = 0;
		vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

		if (device_count == 0) {
			LOGE("No GPUs found with Vulkan support!");
			return;
		}

		std::vector<VkPhysicalDevice> devices(device_count);
		vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

		LOGI("Available GPUs with Vulkan support:");
		for (const auto& device : devices) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);
			LOGI("  - {} (Type: {})", props.deviceName,
				props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete" :
				props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated" :
				props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "Virtual" :
				props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU" : "Other");
		}
	}

	void GpuDevice::Shutdown() {
		// 等待设备完成所有操作
		if (device_) {
			vkDeviceWaitIdle(device_);
		}

		DestroySwapchain();
		resource_manager_.Shutdown();
		command_buffer_manager_.Shutdown();
		pipeline_cache_.CleanUp();
		main_deletion_queue_.Flush();
		profiler_.CleanUp();

		for (int i = 0; i < kFRAME_OVERLAP; i++)
		{
			vkDestroyFence(device_, frames_[i].render_fence, nullptr);
			vkDestroySemaphore(device_, frames_[i].render_semaphore, nullptr);
			vkDestroySemaphore(device_, frames_[i].swapchain_semaphore, nullptr);

			frames_[i].deletion_queue.Flush();
		}

		// 清理VMA分配器
		if (vma_allocator_) {
			vmaDestroyAllocator(vma_allocator_);
			vma_allocator_ = VK_NULL_HANDLE;
		}

		// 清理设备
		if (device_) {
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}

		// 清理调试信使
		if (debug_messenger_) {
			vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
			debug_messenger_ = VK_NULL_HANDLE;
		}
	}

	GpuDevice::QueueFamilyIndices GpuDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
		QueueFamilyIndices indices{};

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

		// 查找支持图形的队列族
		for (uint32_t i = 0; i < queue_family_count; i++) {
			if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphics_family = i;
				break;
			}
		}

		// 查找专用传输队列族
		for (uint32_t i = 0; i < queue_family_count; i++) {
			if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
				!(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				indices.transfer_family = i;
				indices.has_dedicated_transfer = true;
				break;
			}
		}

		return indices;
	}

	VkFence GpuDevice::CreateFence(bool signaled, const char* debug_name) {
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		VkFence fence;
		if (vkCreateFence(device_, &fence_info, nullptr, &fence) != VK_SUCCESS) {
			LOGE("Failed to create fence");
			return VK_NULL_HANDLE;
		}

		if (debug_name) {
			SetDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, debug_name);
		}

		return fence;
	}

	VkSemaphore GpuDevice::CreateSemaphore(const char* debug_name) {
		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkSemaphore semaphore;
		if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
			LOGE("Failed to create semaphore");
			return VK_NULL_HANDLE;
		}

		if (debug_name) {
			SetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, debug_name);
		}

		return semaphore;
	}

	void GpuDevice::DestroyFence(VkFence fence) {
		if (fence) {
			vkDestroyFence(device_, fence, nullptr);
		}
	}

	void GpuDevice::DestroySemaphore(VkSemaphore semaphore) {
		if (semaphore) {
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
	}


	void GpuDevice::UploadBuffer(BufferHandle& buffer_handle, void* buffer_data, size_t size, VkBufferUsageFlags usage, bool transfer)
	{
	}

	bool GpuDevice::InitVulkan(const CreateInfo& create_info)
	{
		if (volkInitialize() != VK_SUCCESS) {
			LOGE("Failed to initialize volk");
			return false;
		}

		vkb::InstanceBuilder builder;
		auto inst_ret = builder.set_app_name(create_info.app_name)
			.set_engine_name(create_info.engine_name)
			.require_api_version(create_info.api_version)
			.request_validation_layers(bUseValidationLayers)
			.enable_validation_layers(bUseValidationLayers)
			.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
			.build();

		if (!inst_ret.has_value()) {
			LOGE("Failed to create Vulkan instance: {}", inst_ret.error().message());
			return false;
		}

		auto vkb_inst = inst_ret.value();
		instance_ = vkb_inst.instance;
		volkLoadInstance(instance_);

		window_ = create_info.window;
		if (!SDL_Vulkan_CreateSurface(window_, instance_, &surface_)) {
			LOGE("Failed to create window surface");
			return false;
		}

		// 配置所需的设备特性
		DeviceFeatures required_features{};
		required_features.features_13.dynamicRendering = true;
		required_features.features_13.synchronization2 = true;
		required_features.features_12.bufferDeviceAddress = true;
		required_features.features_12.descriptorIndexing = true;
		required_features.features_12.descriptorBindingPartiallyBound = true;
		required_features.features_12.descriptorBindingSampledImageUpdateAfterBind = true;
		required_features.features_12.descriptorBindingUniformBufferUpdateAfterBind = true;
		required_features.features_12.descriptorBindingSampledImageUpdateAfterBind = true;
		required_features.features_12.descriptorBindingStorageBufferUpdateAfterBind = true;
		required_features.features_12.descriptorBindingStorageImageUpdateAfterBind = true;
		required_features.features_12.runtimeDescriptorArray = true;
		required_features.features_12.descriptorBindingVariableDescriptorCount = true;
		required_features.features_12.hostQueryReset = true;
		required_features.features_12.drawIndirectCount = true;
		required_features.features_10.pipelineStatisticsQuery = true;
		required_features.features_10.multiDrawIndirect = true;
		required_features.features_10.geometryShader = true;
		required_features.features_10.inheritedQueries = true;

		// 选择物理设备
		vkb::PhysicalDeviceSelector selector{ vkb_inst };
		auto phys_ret = selector
			.set_minimum_version(1, 3)
			.set_required_features_13(required_features.features_13)
			.set_required_features_12(required_features.features_12)
			.set_required_features(required_features.features_10)
			.set_surface(surface_)
			.add_required_extensions(required_extensions)
			.select();

		if (!phys_ret.has_value()) {
			// 仅在设备选择失败时打印可用设备信息，用于调试
			LogAvailableDevices();
			LOGE("Failed to select physical device: {}", phys_ret.error().message());
			return false;
		}

		vkb::PhysicalDevice physical_device = phys_ret.value();

		// 创建逻辑设备
		vkb::DeviceBuilder device_builder{ physical_device };
		auto dev_ret = device_builder.build();
		if (!dev_ret.has_value()) {
			LOGE("Failed to create logical device: {}", dev_ret.error().message());
			return false;
		}

		// 保存设备信息
		auto vkb_device = dev_ret.value();
		device_ = vkb_device.device;
		volkLoadDevice(device_);
		physical_device_ = physical_device.physical_device;
		properties_ = physical_device.properties;
		features_ = physical_device.features;

		// 获取队列
		auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
		auto transfer_queue_ret = vkb_device.get_queue(vkb::QueueType::transfer);
		if (!graphics_queue_ret.has_value()) {
			LOGE("Failed to get graphics queue");
			return false;
		}
		if (!transfer_queue_ret.has_value()) {
			LOGE("Failed to get transfer queue");
			return false;
		}

		graphics_queue_ = graphics_queue_ret.value();
		transfer_queue_ = transfer_queue_ret.value();
		queue_indices_ = FindQueueFamilies(physical_device.physical_device, surface_);

		// 创建VMA分配器
		VmaVulkanFunctions vulkan_functions = {};
		vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocator_info = {};
		allocator_info.physicalDevice = physical_device_;
		allocator_info.device = device_;
		allocator_info.instance = instance_;
		allocator_info.pVulkanFunctions = &vulkan_functions;
		allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		if (vmaCreateAllocator(&allocator_info, &vma_allocator_) != VK_SUCCESS) {
			LOGE("Failed to create VMA allocator");
			return false;
		}

		LOGI("GPU Device initialized successfully");
		LOGI("  Device: {} (API Version: {}.{}.{})",
			properties_.deviceName,
			VK_VERSION_MAJOR(properties_.apiVersion),
			VK_VERSION_MINOR(properties_.apiVersion),
			VK_VERSION_PATCH(properties_.apiVersion));
		// 检查扩展支持情况
		// 获取物理设备支持的扩展
		uint32_t extension_count;
		vkEnumerateDeviceExtensionProperties(physical_device.physical_device, nullptr, &extension_count, nullptr);
		std::vector<VkExtensionProperties> available_extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(physical_device.physical_device, nullptr, &extension_count, available_extensions.data());

		auto has_extension = [&available_extensions](const char* ext_name) -> bool {
			return std::find_if(
				available_extensions.begin(),
				available_extensions.end(),
				[ext_name](const VkExtensionProperties& ext) {
					return strcmp(ext.extensionName, ext_name) == 0;
				}
			) != available_extensions.end();
			};

		// 检查各个扩展的支持情况
		bindless_supported_ = has_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		dynamic_rendering_extension_present_ = has_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		timeline_semaphore_extension_present_ = has_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
		synchronization2_extension_present_ = has_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
		mesh_shaders_extension_present_ = has_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
		multiview_extension_present_ = has_extension(VK_KHR_MULTIVIEW_EXTENSION_NAME);
		fragment_shading_rate_present_ = has_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
		ray_tracing_present_ = has_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) && has_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
			has_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		ray_query_present_ = has_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

		// 输出日志，方便调试
		LOGI("  Extension support status:");
		LOGI("    Bindless: {}", bindless_supported_);
		LOGI("    Dynamic Rendering: {}", dynamic_rendering_extension_present_);
		LOGI("    Timeline Semaphore: {}", timeline_semaphore_extension_present_);
		LOGI("    Synchronization2: {}", synchronization2_extension_present_);
		LOGI("    Mesh Shaders: {}", mesh_shaders_extension_present_);
		LOGI("    Multiview: {}", multiview_extension_present_);
		LOGI("    Fragment Shading Rate: {}", fragment_shading_rate_present_);
		LOGI("    Ray Tracing Pipeline: {}", ray_tracing_present_);
		LOGI("    Ray Query: {}", ray_query_present_);


		// 设置debug messenger
		if (bUseValidationLayers) {
			SetupDebugMessenger();
		}

		return true;
	}

	bool GpuDevice::InitSwapchain()
	{
		if (!CreateSwapchain()) {
			return false;
		}

		// draw image size will match the window
		VkExtent3D draw_image_extent = { default_swapchain_info_.extent.width, default_swapchain_info_.extent.height, 1 };

		TextureCreation image_info{};
		image_info.Reset()
			.SetFormatType(VK_FORMAT_R16G16B16A16_SFLOAT, TextureType::Texture2D)
			.SetSize(draw_image_extent.width, draw_image_extent.height, draw_image_extent.depth)
			.SetFlags(TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask);
		
		// hardcoding the draw format to 32 bit float
		// draw_image_.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		// draw_image_.extent = draw_image_extent;
		// VkImageUsageFlags draw_image_usages{};
		// draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		// draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		// draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		// draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;

		draw_image_handle_ =  resource_manager_.CreateTexture(image_info);

		// VkImageCreateInfo rimg_info = vkinit::ImageCreateInfo(draw_image_.format, draw_image_usages, draw_image_extent);

		// // for the draw image, we want to allocate it from gpu local memory
		// VmaAllocationCreateInfo rimg_allocinfo{};
		// rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		// rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// // allocate and create the image
		// vmaCreateImage(vma_allocator_, &rimg_info, &rimg_allocinfo, &draw_image_.image, &draw_image_.allocation, nullptr);

		// // build a image-view for the draw image to use for rendering
		// VkImageViewCreateInfo rview_info = vkinit::ImageViewCreateInfo(draw_image_.format, draw_image_.image, VK_IMAGE_ASPECT_COLOR_BIT);

		// VK_CHECK(vkCreateImageView(device_, &rview_info, nullptr, &draw_image_.view));

		image_info.Reset()
			.SetFormatType(VK_FORMAT_D32_SFLOAT, TextureType::Texture2D)
			.SetSize(draw_image_extent.width, draw_image_extent.height, draw_image_extent.depth)
			.SetFlags(TextureFlags::Compute_mask);

		depth_image_handle_ = resource_manager_.CreateTexture(image_info);

		// depth_image_.format = VK_FORMAT_D32_SFLOAT;
		// depth_image_.extent = draw_image_extent;
		// VkImageUsageFlags depth_image_usages{};
		// depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		// VkImageCreateInfo dimg_info = vkinit::ImageCreateInfo(depth_image_.format, depth_image_usages, draw_image_extent);

		// // allocate and create the image
		// vmaCreateImage(vma_allocator_, &dimg_info, &rimg_allocinfo, &depth_image_.image, &depth_image_.allocation, nullptr);

		// VkImageViewCreateInfo dview_info = vkinit::ImageViewCreateInfo(depth_image_.format, depth_image_.image, VK_IMAGE_ASPECT_DEPTH_BIT);

		// VK_CHECK(vkCreateImageView(device_, &dview_info, nullptr, &depth_image_.view));

		// add the image to the deletion queue
		// main_deletion_queue_.PushFunction([=]()
		// 	{
		// 		vkDestroyImageView(device_, draw_image_.view, nullptr);
		// 		vmaDestroyImage(vma_allocator_, draw_image_.image, draw_image_.allocation);

		// 		vkDestroyImageView(device_, depth_image_.view, nullptr);
		// 		vmaDestroyImage(vma_allocator_, depth_image_.image, depth_image_.allocation); });

		return true;
	}

	bool GpuDevice::InitDefaultResources()
	{
		uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));

		TextureCreation image_info{};
		image_info.Reset()
			.SetName("default white image")
			.SetData((void*)&white)
			.SetSize(1, 1, 1)
			.SetFormatType(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Enum::Texture2D)
			.SetFlags(TextureFlags::Default);

		default_resources_.images.white_image = resource_manager_.CreateTexture(image_info);// CreateImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

		uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66, 0.66, 0.66, 1));
		image_info.SetName("default grey image").SetData((void*)&grey);
		default_resources_.images.grey_image = resource_manager_.CreateTexture(image_info);// CreateImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

		uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
		image_info.SetName("default black image").SetData((void*)&black);
		default_resources_.images.black_image = resource_manager_.CreateTexture(image_info);// CreateImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

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
		image_info.SetName("default checkerboard image")
			.SetData(pixels.data())
			.SetSize(16, 16, 1);
		default_resources_.images.error_checker_board_image = resource_manager_.CreateTexture(image_info);// CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

		VkSamplerCreateInfo sampler_create_info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

		sampler_create_info.magFilter = VK_FILTER_NEAREST;
		sampler_create_info.minFilter = VK_FILTER_NEAREST;

		vkCreateSampler(device_, &sampler_create_info, nullptr, &default_resources_.samplers.nearest);

		sampler_create_info.magFilter = VK_FILTER_LINEAR;
		sampler_create_info.minFilter = VK_FILTER_LINEAR;

		vkCreateSampler(device_, &sampler_create_info, nullptr, &default_resources_.samplers.linear);

		main_deletion_queue_.PushFunction([&]()
			{
				vkDestroySampler(device_, default_resources_.samplers.nearest, nullptr);
				vkDestroySampler(device_, default_resources_.samplers.linear, nullptr);
			});

		return true;
	}


	bool GpuDevice::CreateSwapchain()
	{
		vkb::SwapchainBuilder swapchain_builder{ physical_device_, device_, surface_ };

		swapchain_image_format_ = default_swapchain_info_.image_format;

		vkb::Swapchain vkb_swapchain = swapchain_builder
			.set_desired_format(VkSurfaceFormatKHR{ .format = swapchain_image_format_, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
			.set_desired_present_mode(default_swapchain_info_.present_mode)
			.set_desired_extent(default_swapchain_info_.extent.width, default_swapchain_info_.extent.height)
			.add_image_usage_flags(default_swapchain_info_.usage)
			.build()
			.value();

		swapchain_extent_ = vkb_swapchain.extent;
		// store swapchain and images
		swapchain_ = vkb_swapchain.swapchain;
		swapchain_images_ = vkb_swapchain.get_images().value();
		swapchain_image_views_ = vkb_swapchain.get_image_views().value();
		return true;
	}

	void GpuDevice::DestroySwapchain()
	{
		if (swapchain_ != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device_, swapchain_, nullptr);
			swapchain_ = VK_NULL_HANDLE;
		}

		for (auto view : swapchain_image_views_)
		{
			vkDestroyImageView(device_, view, nullptr);
		}
		swapchain_image_views_.clear();
		swapchain_images_.clear();
	}

	bool GpuDevice::ResizeSwapchain(uint32_t width, uint32_t height)
	{
		vkDeviceWaitIdle(device_);
		DestroySwapchain();

		default_swapchain_info_.extent.width = width;
		default_swapchain_info_.extent.height = height;

		return CreateSwapchain();
	}

	GpuDeviceCreation& GpuDeviceCreation::SetWindow(uint32_t width, uint32_t height, void* handle)
	{
		this->width = static_cast<uint16_t>(width);
		this->height = static_cast<uint16_t>(height);
		window = handle;
		return *this;
	}

	GpuDeviceCreation& GpuDeviceCreation::SetNumThreads(uint32_t value)
	{
		num_threads = value;
		return *this;
	}
}