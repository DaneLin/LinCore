#include "vk_device.h"
// std
#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
// external
#include <SDL_vulkan.h>
#include <volk/volk.h>
#include <SDL.h>
#include <glm/packing.hpp>
// lincore
#include "foundation/logging.h"
#include "graphics/backend/vk_pipelines.h"

namespace lincore
{
	bool GpuDevice::Init(const CreateInfo &create_info)
	{

		if (!InitVulkan(create_info))
		{
			return false;
		}

		resource_manager_.Init(this);

		if (!InitSwapchain())
		{
			return false;
		}

		command_buffer_manager_.Init(this, kNUM_RENDER_THREADS);
		pipeline_cache_.Init(device_, cache_file_path);
		profiler_.Init(device_, properties_.limits.timestampPeriod);
		shader_manager_.Init(this);

		InitDefaultResources();
		InitDescriptors();
		InitFrameDatas();

		return true;
	}

	void GpuDevice::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
									 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
									 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
								 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
								 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = this; // Pass the GpuDevice instance to the callback
	}

	void GpuDevice::SetupDebugMessenger()
	{
		if (!bUseValidationLayers)
			return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debug_messenger_));
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL GpuDevice::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData)
	{

		auto *device = static_cast<GpuDevice *>(pUserData);
		std::string message = pCallbackData->pMessage;
		std::string objectNames;

		// 查找相关对象的名称
		if (pCallbackData->objectCount > 0 && device)
		{
			std::lock_guard<std::mutex> lock(device->debug_mutex_);
			for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
			{
				const auto &obj = pCallbackData->pObjects[i];
				auto it = device->debug_names_.find(obj.objectHandle);
				if (it != device->debug_names_.end())
				{
					objectNames += "\n  - Object: " + it->second.name;
					if (obj.pObjectName)
					{
						objectNames += " (" + std::string(obj.pObjectName) + ")";
					}
				}
			}
		}

		// 根据消息严重程度选择输出方式
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			LOGE("Validation Error: {}{}", message, objectNames);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			LOGW("Validation Warning: {}{}", message, objectNames);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			LOGI("Validation Info: {}{}", message, objectNames);
		}
		else
		{
			LOGD("Validation Debug: {}{}", message, objectNames);
		}

		return VK_FALSE;
	}

	void GpuDevice::SetDebugName(VkObjectType type, uint64_t handle, const char *name)
	{
		if (!bUseValidationLayers || !name)
			return;

		// 保存对象名称到映射中
		{
			std::lock_guard<std::mutex> lock(debug_mutex_);
			debug_names_[handle] = DebugInfo{
				.handle = handle,
				.type = type,
				.name = name};
		}

		// 设置Vulkan对象的调试名称
		VkDebugUtilsObjectNameInfoEXT nameInfo{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = type,
			.objectHandle = handle,
			.pObjectName = name};

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
													 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
													 nullptr,
													 VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
			SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)bindless_texture_layout_, "Global Bindless Texture Layout");

			// allocate a descriptor set for our bindless textures
			VkDescriptorSetVariableDescriptorCountAllocateInfo count_allocate_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
				.descriptorSetCount = 1,
				.pDescriptorCounts = &kMAX_BINDLESS_RESOURCES};

			bindless_texture_set_ = descriptor_allocator_.Allocate(device_, bindless_texture_layout_, &count_allocate_info);
			SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)bindless_texture_set_, "Global Bindless Texture Set");

			bindless_updates.descriptor_set = bindless_texture_set_;
		}

		// make sure both the descriptor allocator and the new layout get cleaned up properly
		main_deletion_queue_.PushFunction(
			[&]()
			{
				descriptor_allocator_.DestroyPools(device_);

				vkDestroyDescriptorSetLayout(device_, bindless_texture_layout_, nullptr); });
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

	void GpuDevice::InitSynchronization()
	{
		VkSemaphoreCreateInfo semaphore_create_info = vkinit::SemaphoreCreateInfo();
		// extra structure for timeline semaphore
		VkSemaphoreTypeCreateInfo timeline_semaphore_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = 0};
		semaphore_create_info.pNext = &timeline_semaphore_info;
		VK_CHECK(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &timeline_semaphore_));
	}

	void GpuDevice::UpdateBindlessDescriptors()
	{
		if (bindless_updates.updates.empty())
		{
			return;
		}

		std::vector<VkWriteDescriptorSet> writes;
		writes.reserve(bindless_updates.updates.size());

		for (const auto &update : bindless_updates.updates)
		{
			VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			write.dstSet = bindless_updates.descriptor_set;
			write.dstBinding = update.binding;
			write.dstArrayElement = update.array_element;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.pImageInfo = &update.image_info;
			writes.push_back(write);
		}

		vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()),
							   writes.data(), 0, nullptr);

		bindless_updates.Reset();
	}

	void GpuDevice::LogAvailableDevices()
	{
		uint32_t device_count = 0;
		vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

		if (device_count == 0)
		{
			LOGE("No GPUs found with Vulkan support!");
			return;
		}

		std::vector<VkPhysicalDevice> devices(device_count);
		vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

		LOGI("Available GPUs with Vulkan support:");
		for (const auto &device : devices)
		{
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);
			LOGI("  - {} (Type: {})", props.deviceName,
				 props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU	  ? "Discrete"
				 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated"
				 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU	  ? "Virtual"
				 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU			  ? "CPU"
																			  : "Other");
		}
	}

	void GpuDevice::Shutdown()
	{
		// 等待设备完成所有操作
		if (device_)
		{
			vkDeviceWaitIdle(device_);
		}

		DestroySwapchain();

		resource_manager_.Shutdown();
		command_buffer_manager_.Shutdown();
		pipeline_cache_.CleanUp();
		shader_manager_.Shutdown();
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
		if (vma_allocator_)
		{
			vmaDestroyAllocator(vma_allocator_);
			vma_allocator_ = VK_NULL_HANDLE;
		}

		// 清理设备
		if (device_)
		{
			vkDestroyDevice(device_, nullptr);
			device_ = VK_NULL_HANDLE;
		}

		// 清理调试信使
		if (debug_messenger_)
		{
			vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
			debug_messenger_ = VK_NULL_HANDLE;
		}
	}

	FrameData &GpuDevice::BeginFrame()
	{
		UpdateBindlessDescriptors();

		FrameData &frame = GetCurrentFrame();

		VK_CHECK(vkWaitForFences(device_, 1, &frame.render_fence, VK_TRUE, UINT64_MAX));

		// 清理当前帧的资源
		frame.deletion_queue.Flush();
		frame.frame_descriptors.ClearPools(device_);

		// 重置fence
		VK_CHECK(vkResetFences(device_, 1, &frame.render_fence));

		// 获取新的交换链图像
		frame.result = vkAcquireNextImageKHR(
			device_,
			swapchain_,
			UINT64_MAX,
			frame.swapchain_semaphore,
			VK_NULL_HANDLE,
			&frame.swapchain_index);
		if (frame.result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return frame;
		}

		draw_extent_.width = static_cast<uint32_t>(std::min(swapchain_extent_.width, GetDrawImage()->vk_extent.width) * render_scale_);
		draw_extent_.height = static_cast<uint32_t>(std::min(swapchain_extent_.height, GetDrawImage()->vk_extent.height) * render_scale_);

		// 设置当前帧的渲染上下文
		frame.draw_extent = draw_extent_;
		command_buffer_manager_.ResetPools(current_frame_);
		frame.cmd = command_buffer_manager_.GetCommandBuffer(current_frame_, 0, true);

		frame.cmd->Begin();

		return frame;
	}

	void GpuDevice::EndFrame()
	{
		FrameData &frame = GetCurrentFrame();

		// 结束命令缓冲区记录
		frame.cmd->End();
		// 准备提交信息
		VkCommandBufferSubmitInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
		cmd_info.commandBuffer = frame.cmd->GetVkCommandBuffer();

		VkSemaphoreSubmitInfo wait_info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
		wait_info.semaphore = frame.swapchain_semaphore;
		wait_info.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR; // 确保等待所有可能的操作;

		VkSemaphoreSubmitInfo signal_info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
		signal_info.semaphore = frame.render_semaphore;
		signal_info.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
		VkSubmitInfo2 submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
		submit_info.commandBufferInfoCount = 1;
		submit_info.pCommandBufferInfos = &cmd_info;
		submit_info.waitSemaphoreInfoCount = 1;
		submit_info.pWaitSemaphoreInfos = &wait_info;
		submit_info.signalSemaphoreInfoCount = 1;
		submit_info.pSignalSemaphoreInfos = &signal_info;
		// 提交命令缓冲区
		VK_CHECK(vkQueueSubmit2(graphics_queue_, 1, &submit_info, frame.render_fence));
		// 准备呈现信息
		VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swapchain_;
		present_info.pImageIndices = &frame.swapchain_index;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &frame.render_semaphore;
		// 呈现
		frame.result = vkQueuePresentKHR(graphics_queue_, &present_info);
		// 更新帧索引
		previous_frame_ = current_frame_;
		current_frame_ = (current_frame_ + 1) % kFRAME_OVERLAP;
	}

	uint32_t GpuDevice::AddBindlessSampledImage(TextureHandle texture_handle, SamplerHandle sampler_handle)
	{
		Texture *tex = GetResource<Texture>(texture_handle.index);
		Sampler *sampler = GetResource<Sampler>(sampler_handle.index);
		tex->sampler = sampler;
		return bindless_updates.AddTextureUpdate(tex->vk_image_view, sampler->vk_sampler);
	}

	VkFence GpuDevice::CreateFence(bool signaled, const char *debug_name)
	{
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

		VkFence fence;
		if (vkCreateFence(device_, &fence_info, nullptr, &fence) != VK_SUCCESS)
		{
			LOGE("Failed to create fence");
			return VK_NULL_HANDLE;
		}

		if (debug_name)
		{
			SetDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, debug_name);
		}

		return fence;
	}

	VkSemaphore GpuDevice::CreateSemaphore(const char *debug_name)
	{
		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkSemaphore semaphore;
		if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS)
		{
			LOGE("Failed to create semaphore");
			return VK_NULL_HANDLE;
		}

		if (debug_name)
		{
			SetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, debug_name);
		}

		return semaphore;
	}

	void GpuDevice::DestroyFence(VkFence fence)
	{
		if (fence)
		{
			vkDestroyFence(device_, fence, nullptr);
		}
	}

	void GpuDevice::DestroySemaphore(VkSemaphore semaphore)
	{
		if (semaphore)
		{
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
	}

	void GpuDevice::CopyBuffer(CommandBuffer *cmd, BufferHandle &src_buffer_handle, BufferHandle &dst_buffer_handle)
	{
		Buffer *src = GetResource<Buffer>(src_buffer_handle.index);
		Buffer *dst = GetResource<Buffer>(dst_buffer_handle.index);
		VkBufferCopy copy_region{0};
		copy_region.dstOffset = 0;
		copy_region.srcOffset = 0;
		copy_region.size = src->size;
		vkCmdCopyBuffer(cmd->GetVkCommandBuffer(), src->vk_buffer, dst->vk_buffer, 1, &copy_region);

		cmd->AddBufferBarrier(dst, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
	}

	ShaderEffect *GpuDevice::CreateShaderEffect(std::initializer_list<std::string> file_names, const std::string &name)
	{
		return shader_manager_.GetShaderEffect(file_names, name);
	}

	std::vector<VkRenderingAttachmentInfo> GpuDevice::CreateRenderingAttachmentsColor(std::vector<TextureHandle> &color_targets, VkClearValue *clear_color)
	{
		std::vector<VkRenderingAttachmentInfo> attachments;
		for (auto &color_target : color_targets)
		{
			attachments.push_back(vkinit::AttachmentInfo(GetResource<Texture>(color_target.index)->vk_image_view, clear_color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
		}
		return attachments;
	}

	VkRenderingAttachmentInfo GpuDevice::CreateRenderingAttachmentsDepth(TextureHandle &depth_target)
	{
		return vkinit::DepthAttachmentInfo(GetResource<Texture>(depth_target.index)->vk_image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	}

	bool GpuDevice::InitVulkan(const CreateInfo &create_info)
	{
		if (volkInitialize() != VK_SUCCESS)
		{
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

		if (!inst_ret.has_value())
		{
			LOGE("Failed to create Vulkan instance: {}", inst_ret.error().message());
			return false;
		}

		auto vkb_inst = inst_ret.value();
		instance_ = vkb_inst.instance;
		volkLoadInstance(instance_);

		window_ = create_info.window;
		if (!SDL_Vulkan_CreateSurface(window_, instance_, &surface_))
		{
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
		required_features.features_12.timelineSemaphore = true;
		required_features.features_11.shaderDrawParameters = true;
		required_features.features_10.pipelineStatisticsQuery = true;
		required_features.features_10.multiDrawIndirect = true;
		required_features.features_10.geometryShader = true;
		required_features.features_10.inheritedQueries = true;
		required_features.features_10.samplerAnisotropy = true;

		// 选择物理设备
		vkb::PhysicalDeviceSelector selector{vkb_inst};
		auto phys_ret = selector
							.set_minimum_version(1, 3)
							.set_required_features_13(required_features.features_13)
							.set_required_features_12(required_features.features_12)
							.set_required_features_11(required_features.features_11)
							.set_required_features(required_features.features_10)
							.set_surface(surface_)
							.add_required_extensions(required_extensions)
							.select();

		if (!phys_ret.has_value())
		{
			// 仅在设备选择失败时打印可用设备信息，用于调试
			LogAvailableDevices();
			LOGE("Failed to select physical device: {}", phys_ret.error().message());
			return false;
		}

		vkb::PhysicalDevice physical_device = phys_ret.value();

		// 创建逻辑设备
		vkb::DeviceBuilder device_builder{physical_device};
		auto dev_ret = device_builder.build();
		if (!dev_ret.has_value())
		{
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
		if (!graphics_queue_ret.has_value())
		{
			LOGE("Failed to get graphics queue");
			return false;
		}
		if (!transfer_queue_ret.has_value())
		{
			LOGE("Failed to get transfer queue");
			return false;
		}

		graphics_queue_ = graphics_queue_ret.value();
		transfer_queue_ = transfer_queue_ret.value();
		SetDebugName(VK_OBJECT_TYPE_QUEUE, (uint64_t)graphics_queue_, "Graphics Queue");
		SetDebugName(VK_OBJECT_TYPE_QUEUE, (uint64_t)transfer_queue_, "Transfer Queue");
		queue_indices_.graphics_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
		queue_indices_.transfer_family = vkb_device.get_queue_index(vkb::QueueType::transfer).value();

		if (queue_indices_.graphics_family != queue_indices_.transfer_family)
		{
			queue_indices_.has_dedicated_transfer = true;
		}

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

		if (vmaCreateAllocator(&allocator_info, &vma_allocator_) != VK_SUCCESS)
		{
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

		auto has_extension = [&available_extensions](const char *ext_name) -> bool
		{
			return std::find_if(
					   available_extensions.begin(),
					   available_extensions.end(),
					   [ext_name](const VkExtensionProperties &ext)
					   {
						   return strcmp(ext.extensionName, ext_name) == 0;
					   }) != available_extensions.end();
		};

		// 检查各个扩展的支持情况
		enabled_features_.bindless_supported_ = has_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		enabled_features_.dynamic_rendering_extension_present_ = has_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		enabled_features_.timeline_semaphore_extension_present_ = has_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
		enabled_features_.synchronization2_extension_present_ = has_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
		enabled_features_.mesh_shaders_extension_present_ = has_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
		enabled_features_.multiview_extension_present_ = has_extension(VK_KHR_MULTIVIEW_EXTENSION_NAME);
		enabled_features_.fragment_shading_rate_present_ = has_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
		enabled_features_.ray_tracing_present_ = has_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) && has_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
												 has_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		enabled_features_.ray_query_present_ = has_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

		LOGI("  Extension support status:");
		LOGI("    Bindless: {}", enabled_features_.bindless_supported_);
		LOGI("    Dynamic Rendering: {}", enabled_features_.dynamic_rendering_extension_present_);
		LOGI("    Timeline Semaphore: {}", enabled_features_.timeline_semaphore_extension_present_);
		LOGI("    Synchronization2: {}", enabled_features_.synchronization2_extension_present_);
		LOGI("    Mesh Shaders: {}", enabled_features_.mesh_shaders_extension_present_);
		LOGI("    Multiview: {}", enabled_features_.multiview_extension_present_);
		LOGI("    Fragment Shading Rate: {}", enabled_features_.fragment_shading_rate_present_);
		LOGI("    Ray Tracing Pipeline: {}", enabled_features_.ray_tracing_present_);
		LOGI("    Ray Query: {}", enabled_features_.ray_query_present_);

		// 设置debug messenger
		if (bUseValidationLayers)
		{
			SetupDebugMessenger();
		}

		return true;
	}

	bool GpuDevice::InitSwapchain()
	{
		if (!CreateSwapchain())
		{
			return false;
		}

		// draw image size will match the window
		VkExtent3D draw_image_extent = {default_swapchain_info_.extent.width, default_swapchain_info_.extent.height, 1};

		TextureCreation image_info{};
		image_info.Reset()
			.SetImmediate()
			.SetName("draw image")
			.SetFormatType(VK_FORMAT_R16G16B16A16_SFLOAT, TextureType::Texture2D)
			.SetSize(draw_image_extent.width, draw_image_extent.height, draw_image_extent.depth, false)
			.SetFlags(TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask | TextureFlags::Default_mask);
		draw_image_handle_ = CreateResource(image_info);

		image_info.Reset()
			.SetImmediate()
			.SetName("depth image")
			.SetFormatType(VK_FORMAT_D32_SFLOAT, TextureType::Texture2D)
			.SetSize(draw_image_extent.width, draw_image_extent.height, draw_image_extent.depth, false)
			.SetFlags(TextureFlags::Compute_mask | TextureFlags::RenderTarget_mask | TextureFlags::Default_mask);
		depth_image_handle_ = CreateResource(image_info);
		
		return true;
	}

	bool GpuDevice::InitDefaultResources()
	{
		uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));

		TextureCreation image_info{};
		image_info.Reset()
			.SetImmediate()
			.SetName("default white image")
			.SetData((void *)&white, sizeof(uint32_t))
			.SetSize(1, 1, 1)
			.SetFormatType(VK_FORMAT_R8G8B8A8_UNORM, TextureType::Enum::Texture2D)
			.SetFlags(TextureFlags::Default | TextureFlags::RenderTarget);
		default_resources_.images.white_image = CreateResource(image_info);

		uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66, 0.66, 0.66, 1));
		image_info.SetName("default grey image")
			.SetData((void *)&grey, sizeof(uint32_t));
		default_resources_.images.grey_image = CreateResource(image_info);

		uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
		image_info.SetName("default black image")
			.SetData((void *)&black, sizeof(uint32_t));
		default_resources_.images.black_image = CreateResource(image_info);

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
			.SetData(pixels.data(), 16 * 16 * sizeof(uint32_t))
			.SetSize(16, 16, 1);
		default_resources_.images.error_checker_board_image = CreateResource(image_info);

		SamplerCreation sampler_info{};
		sampler_info.SetName("default nearest sampler")
			.SetMinMag(VK_FILTER_NEAREST, VK_FILTER_NEAREST)
			.SetMip(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		default_resources_.samplers.nearest = CreateResource(sampler_info);

		sampler_info.SetName("default linear sampler")
			.SetMinMag(VK_FILTER_LINEAR, VK_FILTER_LINEAR)
			.SetMip(VK_SAMPLER_MIPMAP_MODE_LINEAR);
		default_resources_.samplers.linear = CreateResource(sampler_info);

		AddBindlessSampledImage(default_resources_.images.black_image, default_resources_.samplers.linear);

		Texture *depth_image = GetResource<Texture>(depth_image_handle_.index);
		depth_image->sampler = GetResource<Sampler>(default_resources_.samplers.linear.index);

		BufferCreation buffer_info{};
		buffer_info.Reset()
			.Set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ResourceUsageType::Immutable)
			.SetData(nullptr, sizeof(scene::GPUSceneData))
			.SetPersistent();
		global_scene_data_buffer_ = CreateResource(buffer_info);

		return true;
	}

	bool GpuDevice::CreateSwapchain()
	{
		vkb::SwapchainBuilder swapchain_builder{physical_device_, device_, surface_};

		swapchain_image_format_ = default_swapchain_info_.image_format;

		vkb::Swapchain vkb_swapchain = swapchain_builder
										   .set_desired_format(VkSurfaceFormatKHR{.format = swapchain_image_format_, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
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

		swapchain_textures_.resize(swapchain_images_.size());
		for (size_t i = 0; i < swapchain_images_.size(); ++i)
		{
			swapchain_textures_[i].vk_image = swapchain_images_[i];
			swapchain_textures_[i].vk_image_view = swapchain_image_views_[i];
			swapchain_textures_[i].vk_format = swapchain_image_format_;
			swapchain_textures_[i].vk_usage = default_swapchain_info_.usage;
			swapchain_textures_[i].vk_extent = {swapchain_extent_.width, swapchain_extent_.height, 1};
			swapchain_textures_[i].array_layer_count = 1;
			swapchain_textures_[i].mip_level_count = 1;
			swapchain_textures_[i].flags = 0;
			swapchain_textures_[i].state = RESOURCE_STATE_UNDEFINED;
			swapchain_textures_[i].vma_allocation = VK_NULL_HANDLE;
			swapchain_textures_[i].queue_type = QueueType::Enum::Graphics;
			swapchain_textures_[i].queue_family = queue_indices_.graphics_family;
			SetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)swapchain_textures_[i].vk_image, ("swapchain image" + std::to_string(i)).c_str());
		}

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

	GpuDeviceCreation &GpuDeviceCreation::SetWindow(uint32_t width, uint32_t height, void *handle)
	{
		this->width = static_cast<uint16_t>(width);
		this->height = static_cast<uint16_t>(height);
		window = handle;
		return *this;
	}
}