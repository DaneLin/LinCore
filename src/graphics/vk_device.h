#pragma once
// std
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
// external
#include "VkBootstrap.h"
#include <volk.h>
#include <SDL_vulkan.h>
// lincore
#include "foundation/data_structure.h"
#include "foundation/resources.h"
#include "vk_command_buffer.h"
#include "vk_initializers.h"
#include "vk_resources.h"
#include "vk_types.h"
#include "vk_profiler.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

// 前向声明
namespace vkb
{
	struct Instance;
}

namespace lincore
{
	class ShaderManager;
	class TextureCache;
	class PipelineCache;

	/**
	 * @brief 帧数据
	 * 包含交换链信号量、渲染信号量、渲染栅栏、删除队列和帧描述符
	 */
	struct FrameData
	{
		VkSemaphore swapchain_semaphore, render_semaphore;
		VkFence render_fence;
		DeletionQueue deletion_queue;
		DescriptorAllocatorGrowable frame_descriptors;

		uint32_t visible_count{0};

		// 渲染上下文
		uint32_t swapchain_index{0};
		VkExtent2D draw_extent{};
		CommandBuffer *cmd{nullptr};
		VkResult result{VK_SUCCESS};
	}; // struct FrameData

	/**
	 * @brief GPU描述符池创建信息
	 * 包含不同类型的描述符数量
	 */
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

	/**
	 * @brief GPU资源池创建信息
	 * 包含不同类型的资源数量
	 */
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

	/**
	 * @brief GPU设备创建信息
	 * 包含描述符池创建信息、资源池创建信息和窗口信息
	 */
	struct GpuDeviceCreation
	{
		GPUDescriptorPoolCreation descriptor_pool_creation;
		GPUResourcePoolCreation resource_pool_creation;

		void *window = nullptr; // pointer to API-specific window
		uint16_t width = 1;
		uint16_t height = 1;

		uint16_t gpu_time_queries_per_frame = 32;
		uint16_t num_threads = 1;
		bool enable_gpu_time_queries = false;
		bool enable_pipeline_statistics = true;
		bool debug = false;
		bool force_disable_dynamic_rendering = false;

		GpuDeviceCreation &SetWindow(uint32_t width, uint32_t height, void *handle);
	};

	/**
	 * @brief GPU设备类
	 * 包含队列族索引、创建信息、交换链信息、默认资源、调试信息、窗口指针等成员
	 */
	class GpuDevice
	{
	public:
		struct QueueFamilyIndices
		{
			uint32_t graphics_family;
			uint32_t transfer_family;
			bool has_dedicated_transfer;
		} queue_indices_;

		/**
		 * @brief 创建信息
		 * 包含应用程序名称、引擎名称、API版本、窗口尺寸和窗口指针
		 */	
		struct CreateInfo
		{
			const char *app_name;
			const char *engine_name;
			uint32_t api_version;
			VkExtent2D window_extent{1700, 900};
			struct SDL_Window *window{nullptr};
		};

		/**
		 * @brief 交换链信息
		 * 包含图像格式、尺寸、呈现模式、使用标志
		 */
		struct SwapchainInfo
		{
			VkFormat image_format{VK_FORMAT_B8G8R8A8_UNORM};
			VkExtent2D extent{1700, 900};
			VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
			VkImageUsageFlags usage{VK_IMAGE_USAGE_TRANSFER_DST_BIT};
		} default_swapchain_info_;

		/**
		 * @brief 默认资源
		 * 包含图像和采样器
		 */
		struct DefaultResources
		{
			struct
			{
				TextureHandle white_image;
				TextureHandle black_image;
				TextureHandle grey_image;
				TextureHandle error_checker_board_image;
			} images;
			struct
			{
				SamplerHandle linear;
				SamplerHandle nearest;
			} samplers;
		} default_resources_;

		/**
		 * @brief 调试信息
		 * 包含句柄、类型和名称
		 */
		struct DebugInfo
		{
			uint64_t handle;
			VkObjectType type;
			std::string name;
		};
		/**
		 * @brief 调试名称映射
		 * 包含句柄和调试信息
		 */
		std::unordered_map<uint64_t, DebugInfo> debug_names_;
		std::mutex debug_mutex_;

		SDL_Window *window_;

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

		uint32_t current_frame_{0};
		uint32_t previous_frame_{ 1 };
		FrameData frames_[kFRAME_OVERLAP];

		/**
		 * @brief 绘制资源
		 * 包含绘制图像句柄和深度图像句柄
		 */
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

		/**
		 * @brief 描述符分配器
		 * 包含描述符分配器
		 */
		DescriptorAllocatorGrowable descriptor_allocator_;

		/**
		 * @brief 全局描述符集布局
		 * 包含绑定纹理布局
		 */
		VkDescriptorSetLayout bindless_texture_layout_{VK_NULL_HANDLE};
		VkDescriptorSet bindless_texture_set_{VK_NULL_HANDLE};
		// 全局单个更新数组
   		BindlessUpdateArray bindless_updates;

		VkDescriptorSetLayout gpu_scene_data_descriptor_layout_;

		GPUSceneData scene_data_;

		ResourceManager resource_manager_;
		CommandBufferManager command_buffer_manager_;
		ShaderManager shader_manager_;

		VulkanProfiler profiler_;
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
		bool Init(const CreateInfo &create_info);
		void Shutdown();

		FrameData &BeginFrame();
		void EndFrame();

		// 获取当前帧数据
		FrameData &GetCurrentFrame() { return frames_[current_frame_]; }
		const FrameData &GetCurrentFrame() const { return frames_[current_frame_]; }
		uint32_t AddBindlessSampledImage(TextureHandle texture_handle, SamplerHandle sampler_handle);

		// 同步原语管理
		VkFence CreateFence(bool signaled = false, const char *debug_name = nullptr);
		VkSemaphore CreateSemaphore(const char *debug_name = nullptr);
		void DestroyFence(VkFence fence);
		void DestroySemaphore(VkSemaphore semaphore);

		// 辅助功能
		void SetDebugName(VkObjectType type, uint64_t handle, const char *name);

		// Swapchain管理
		void DestroySwapchain();
		bool ResizeSwapchain(uint32_t width, uint32_t height);
		bool CreateSwapchain();

		// Draw resources
		Texture *GetDrawImage() { return resource_manager_.GetTexture(draw_image_handle_); }
		Texture *GetDepthImage() { return resource_manager_.GetTexture(depth_image_handle_); }

		// resource management
		void UploadBuffer(BufferHandle &buffer_handle, void *buffer_data, size_t size, VkBufferUsageFlags usage, bool transfer = false);
		void CopyBuffer(CommandBuffer *cmd, BufferHandle &src_buffer_handle, BufferHandle &dst_buffer_handle);

		ShaderEffect* CreateShaderEffect(std::initializer_list<std::string> file_names, const std::string& name = "");

		// 创建渲染附件
		std::vector<VkRenderingAttachmentInfo> CreateRenderingAttachmentsColor(std::vector<TextureHandle>& color_targets);
		VkRenderingAttachmentInfo CreateRenderingAttachmentsDepth(TextureHandle& depth_target);

		/**
		 * @brief 通用资源访问模板函数
		 * 包含资源类型和句柄
		 */
		template <typename T>
		T *GetResource(ResourceHandle handle)
		{
			if constexpr (std::is_same_v<T, Buffer>)
			{
				return resource_manager_.GetBuffer({handle});
			}
			else if constexpr (std::is_same_v<T, Texture>)
			{
				return resource_manager_.GetTexture({handle});
			}
			else if constexpr (std::is_same_v<T, Sampler>)
			{
				return resource_manager_.GetSampler({handle});
			}
			else
			{
				static_assert(always_false<T>, "Unsupported resource type");
				return nullptr;
			}
		}

		/**
		 * @brief 常量版本的资源访问函数
		 * 包含资源类型和句柄
		 */
		template <typename T>
		const T *GetResource(ResourceHandle handle) const
		{
			if constexpr (std::is_same_v<T, Buffer>)
			{
				return resource_manager_.GetBuffer({handle});
			}
			else if constexpr (std::is_same_v<T, Texture>)
			{
				return resource_manager_.GetTexture({handle});
			}
			else if constexpr (std::is_same_v<T, Sampler>)
			{
				return resource_manager_.GetSampler({handle});
			}
			else
			{
				static_assert(always_false<T>, "Unsupported resource type");
				return nullptr;
			}
		}

		/**
		 * @brief 通用资源创建模板函数
		 * 包含资源类型和创建信息
		 */
		template <typename CreationType>
		auto CreateResource(const CreationType &creation)
		{
			if constexpr (std::is_same_v<CreationType, BufferCreation>)
			{
				return resource_manager_.CreateBuffer(creation);
			}
			else if constexpr (std::is_same_v<CreationType, TextureCreation>)
			{
				return resource_manager_.CreateTexture(creation);
			}
			else if constexpr (std::is_same_v<CreationType, SamplerCreation>)
			{
				return resource_manager_.CreateSampler(creation);
			}
			else
			{
				static_assert(always_false<CreationType>, "Unsupported resource creation type");
				return ResourceHandle{};
			}
		}

		/**
		 * @brief 通用资源销毁模板函数
		 * 包含资源类型和句柄
		 */
		template <typename HandleType>
		void DestroyResource(HandleType handle)
		{
			if constexpr (std::is_same_v<HandleType, BufferHandle>)
			{
				resource_manager_.DestroyBuffer(handle);
			}
			else if constexpr (std::is_same_v<HandleType, TextureHandle>)
			{
				resource_manager_.DestroyTexture(handle);
			}
			else if constexpr (std::is_same_v<HandleType, SamplerHandle>)
			{
				resource_manager_.DestroySampler(handle);
			}
			else
			{
				static_assert(always_false<HandleType>, "Unsupported resource handle type");
			}
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
			void *pUserData);

	private:
		/**
		 * @brief 设备特性
		 * 包含Vulkan 1.3、1.2和1.0特性
		 */
		struct DeviceFeatures
		{
			VkPhysicalDeviceVulkan13Features features_13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
			VkPhysicalDeviceVulkan12Features features_12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
			VkPhysicalDeviceVulkan11Features features_11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
			VkPhysicalDeviceFeatures features_10{};
		};

		/**
		 * @brief GPU 资源初始化
		 * 包含Vulkan初始化、交换链初始化、默认资源初始化、描述符初始化和帧数据初始化
		 */
		bool InitVulkan(const CreateInfo &create_info);
		bool InitSwapchain();
		bool InitDefaultResources();
		void InitDescriptors();
		void InitFrameDatas();

		void SetupDebugMessenger();
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

		/**
		 * @brief 每帧调用的更新方法
		 * 包含更新绑定纹理描述符
		 */
   		void UpdateBindlessDescriptors();

		void LogAvailableDevices();

		/**
		 * @brief 用于编译期断言的辅助模板
		 * 包含类型T
		 */
		template <typename T>
		static constexpr bool always_false = false;
	};
}