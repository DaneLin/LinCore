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

#include "pool_structure.h"
#include <resources.h>

// 前向声明
namespace vkb { struct Instance; }


namespace lincore
{
	class ShaderCache;
	class TextureCache;
	class PipelineCache;

struct FrameData {
	VkSemaphore swapchain_semaphore, render_semaphore;
	VkFence render_fence;
	DeletionQueue deletion_queue;
	class DescriptorAllocatorGrowable frame_descriptors;
}; // struct FrameData

struct GPUDescriptorPoolCreation
{
	uint16_t samplers = 256;
	uint16_t combined_image_samplers = 256;
	uint16_t sampled_image = 256;
	uint16_t storage_image = 256;
	uint16_t uniform_texel_buffers = 256;
	uint16_t storage_texel_buffers = 256;
	uint16_t uniform_buffer = 256;
	uint16_t storage_buffer = 256;
	uint16_t uniform_buffer_dynamic = 256;
	uint16_t storage_buffer_dynamic = 256;
	uint16_t input_attachments = 256;
}; // struct GPUDescriptorPoolCreation

struct GPUResourcePoolCreation
{
	uint16_t buffers = 256;
	uint16_t textures = 256;
	uint16_t pipelines = 256;
	uint16_t samplers = 256;
	uint16_t descriptor_set_layouts = 256;
	uint16_t descriptor_sets = 256;
	uint16_t render_passes = 256;
	uint16_t framebuffers = 256;
	uint16_t command_buffers = 256;
	uint16_t shaders = 256;
	uint16_t page_pools = 64;
}; // struct GPUResourcePoolCreation

struct GpuDeviceCreation
{
	GPUDescriptorPoolCreation descriptor_pool_creation;
	GPUResourcePoolCreation resource_pool_creation;

	void* window = nullptr; // pointer to API-specific window
	uint16_t width = 1;
	uint16_t height = 1;

	uint16_t gpu_time_queries_per_frame = 32;
	uint16_t num_threads = 1;
	bool enable_gpu_time_queries = false;
	bool enable_pipeline_statistics = true;
	bool debug = false;
	bool force_disable_dynamic_rendering = false;

	GpuDeviceCreation& SetWindow(uint32_t width, uint32_t height, void* handle);
	GpuDeviceCreation& SetNumThreads(uint32_t value);
};


class GpuDevice {
public:
	struct QueueFamilyIndices {
		uint32_t graphics_family;
		uint32_t transfer_family;
		bool has_dedicated_transfer;
	}queue_indices_;

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
		VkPresentModeKHR present_mode{ VK_PRESENT_MODE_FIFO_KHR };
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

	// Debug utilities
	struct DebugInfo {
		uint64_t handle;
		VkObjectType type;
		std::string name;
	};
	// Debug name mapping
	std::unordered_map<uint64_t, DebugInfo> debug_names_;
	std::mutex debug_mutex_;

	SDL_Window* window_;

	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
	VkSurfaceKHR surface_;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	VkQueue transfer_queue_ = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties properties_{};
	VkPhysicalDeviceFeatures features_{};

	DeletionQueue main_deletion_queue_;
	VmaAllocator vma_allocator_ = VK_NULL_HANDLE;

	uint32_t current_frame_{ 0 };
	FrameData frames_[kFRAME_OVERLAP];

	// draw resources
	TextureHandle draw_image_handle_;
	TextureHandle depth_image_handle_;
	VkExtent2D draw_extent_;
	float render_scale_ = 1.f;

	// Swapchain相关成员
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat swapchain_image_format_;
	std::vector<VkImage> swapchain_images_;
	std::vector<VkImageView> swapchain_image_views_;
	VkExtent2D swapchain_extent_{};

	// Descriptor allocator
	DescriptorAllocatorGrowable descriptor_allocator_;

	// global descriptor set layouts
	// [] bindless texture layout
	VkDescriptorSetLayout bindless_texture_layout_{ VK_NULL_HANDLE };
	VkDescriptorSet bindless_texture_set_{ VK_NULL_HANDLE };
	std::vector<ResourceUpdate> bindless_texture_updates_;

	VkDescriptorSetLayout gpu_scene_data_descriptor_layout_;
	
	GPUSceneData scene_data_;

	ResourceManager resource_manager_;
	CommandBufferManager command_buffer_manager_;

	VulkanProfiler profiler_;
	TextureCache texture_cache_;
	PipelineCache pipeline_cache_;

	bool bindless_supported_ = false;
	bool dynamic_rendering_extension_present_ = false;
	bool timeline_semaphore_extension_present_ = false;
	bool synchronization2_extension_present_ = false;
	bool mesh_shaders_extension_present_ = false;
	bool multiview_extension_present_ = false;
	bool fragment_shading_rate_present_ = false;
	bool ray_tracing_present_ = false;
	bool ray_query_present_ = false;

public:
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
	//FrameData& GetCurrentFrame() { return frames_[current_frame_ % kFRAME_OVERLAP]; }
	void DestroySwapchain();
	bool ResizeSwapchain(uint32_t width, uint32_t height);
	bool CreateSwapchain();

	// Draw resources
	Texture* GetDrawImage() { return resource_manager_.GetTexture(draw_image_handle_); }
	Texture* GetDepthImage() { return resource_manager_.GetTexture(depth_image_handle_); }

	FrameData& GetCurrentFrame() { return frames_[current_frame_ % kFRAME_OVERLAP]; }

	// resource management
	void UploadBuffer(BufferHandle& buffer_handle, void* buffer_data, size_t size, VkBufferUsageFlags usage, bool transfer = false);

	// Command buffer management
	CommandBuffer* GetCommandBuffer(bool begin = true) {
		return command_buffer_manager_.GetCommandBuffer(current_frame_, 0, begin);
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

private:

	struct DeviceFeatures {
		VkPhysicalDeviceVulkan13Features features_13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		VkPhysicalDeviceVulkan12Features features_12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceFeatures features_10{};
	};

	// GPU 资源初始化
	bool InitVulkan(const CreateInfo& create_info);
	bool InitSwapchain();
	bool InitDefaultResources();
	void InitDescriptors();
	void InitFrameDatas();

	void SetupDebugMessenger();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
	void LogAvailableDevices();
};

}