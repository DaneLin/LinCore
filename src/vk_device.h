#pragma once

#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "VkBootstrap.h"
#include <volk.h>
#include <SDL_vulkan.h>

#include "command_buffer.h"

#include "vk_initializers.h"
#include "vk_loader.h"

#include "vk_resources.h"
#include "vk_types.h"
#include "vk_profiler.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

// 前向声明
namespace vkb { struct Instance; }

namespace lc
{
	class ShaderCache;
	class TextureCache;
	class PipelineCache;
}

struct FrameData {
	VkSemaphore swapchain_semaphore, render_semaphore;
	VkFence render_fence;
	DeletionQueue deletion_queue;
	class lc::DescriptorAllocatorGrowable frame_descriptors;
};


class GPUDevice {
public:
	struct QueueFamilyIndices {
		uint32_t graphics_family;
		uint32_t transfer_family;
		bool has_dedicated_transfer;
	};

	struct CreateInfo {
		const char* app_name;
		const char* engine_name;
		uint32_t api_version;
		VkExtent2D window_extent{ 1700 , 900 };
		struct SDL_Window* window{ nullptr };
	};

	// Swapchain管理
	struct SwapchainInfo {
		VkFormat image_format{ VK_FORMAT_B8G8R8A8_UNORM };
		VkExtent2D extent{ 1700 , 900 };
		VkPresentModeKHR present_mode{ VK_PRESENT_MODE_MAILBOX_KHR };
		VkImageUsageFlags usage{ VK_IMAGE_USAGE_TRANSFER_DST_BIT };
	}default_swapchain_info_;

	struct DefaultResources {
		struct
		{
			TextureHandle white_image;
			TextureHandle black_image;
			TextureHandle grey_image;
			TextureHandle error_checker_board_image;
		}images;
		struct
		{
			VkSampler linear;
			VkSampler nearest;
		}samplers;
	}default_resources_;

	float render_scale_ = 1.f;

	bool Init(const CreateInfo& create_info);
	void Shutdown();

	// 同步原语管理
	VkFence CreateFence(bool signaled = false, const char* debug_name = nullptr);
	VkSemaphore CreateSemaphore(const char* debug_name = nullptr);
	void DestroyFence(VkFence fence);
	void DestroySemaphore(VkSemaphore semaphore);

	// 辅助功能
	void SetDebugName(VkObjectType type, uint64_t handle, const char* name);

	// Swapchain管理
	//FrameData& GetCurrentFrame() { return frames_[frame_number_ % kFRAME_OVERLAP]; }
	void DestroySwapchain();
	bool ResizeSwapchain(uint32_t width, uint32_t height);
	bool CreateSwapchain();

	FrameData& GetCurrentFrame() { return frames_[frame_number_ % kFRAME_OVERLAP]; }

	// resource management
	void UploadBuffer(BufferHandle& buffer_handle, void* buffer_data, size_t size, VkBufferUsageFlags usage, bool transfer = false);

	struct DeviceFeatures {
		VkPhysicalDeviceVulkan13Features features_13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		VkPhysicalDeviceVulkan12Features features_12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceFeatures features_10{};
	};

	// Debug utilities
	struct DebugInfo {
		uint64_t handle;
		VkObjectType type;
		std::string name;
	};

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	// int frame_number_{ 0 };
	// FrameData frames_[kFRAME_OVERLAP];

	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

	// Debug name mapping
	std::unordered_map<uint64_t, DebugInfo> debug_names_;
	std::mutex debug_mutex_;

	SDL_Window* window_;

	int frame_number_{ 0 };
	FrameData frames_[kFRAME_OVERLAP];

	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
	VkSurfaceKHR surface_;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	VkQueue transfer_queue_ = VK_NULL_HANDLE;
	QueueFamilyIndices queue_indices_{};
	VkPhysicalDeviceProperties properties_{};
	VkPhysicalDeviceFeatures features_{};

	DeletionQueue main_deletion_queue_;
	VmaAllocator allocator_ = VK_NULL_HANDLE;

	// draw resources
	AllocatedImage draw_image_;
	AllocatedImage depth_image_;
	VkExtent2D draw_extent_;

	// Swapchain相关成员
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat swapchain_image_format_;
	std::vector<VkImage> swapchain_images_;
	std::vector<VkImageView> swapchain_image_views_;
	VkExtent2D swapchain_extent_{};

	// Descriptor allocator
	lc::DescriptorAllocatorGrowable descriptor_allocator_;

	// global descriptor set layouts
	// [] bindless texture layout
	VkDescriptorSetLayout bindless_texture_layout_{ VK_NULL_HANDLE };
	VkDescriptorSet bindless_texture_set_{ VK_NULL_HANDLE };

	GPUSceneData scene_data_;
	VkDescriptorSetLayout gpu_scene_data_descriptor_layout_;

	ResourceManager resource_manager_;
	CommandBufferManager command_buffer_manager_;

	vkutils::VulkanProfiler profiler_;
	lc::TextureCache texture_cache_;
	lc::PipelineCache pipeline_cache_;

private:
	// GPU 资源初始化
	bool InitVulkan(const CreateInfo& create_info);
	bool InitSwapchain();
	bool InitCommands();
	bool InitDefaultResources();
	void InitDescriptors();
	void InitFrameDatas();

	void SetupDebugMessenger();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
	void LogAvailableDevices();
};
