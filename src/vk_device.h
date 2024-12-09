#pragma once

#include <vk_types.h>
#include "vk_initializers.h"
#include "VkBootstrap.h"
#include "vk_resources.h"
#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <SDL.h>
#include <SDL_vulkan.h>
#include "command_buffer.h"
#include <vk_descriptors.h>

namespace lc
{
    class GPUDevice
    {
    public:
        struct QueueFamilyIndices
        {
            uint32_t graphics_family;
            uint32_t transfer_family;
            bool has_dedicated_transfer;
        };

        struct CreateInfo
        {
            const char *app_name;
            const char *engine_name;
            uint32_t api_version;
            SDL_Window *window;
        };

        bool Init(const CreateInfo &create_info);
        void Cleanup();

        // 设备功能访问
        VkDevice GetDevice() const { return device_; }
        VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }
        const VkPhysicalDeviceProperties &GetDeviceProperties() const { return properties_; }
        const VkPhysicalDeviceFeatures &GetDeviceFeatures() const { return features_; }

        // 队列访问
        VkQueue GetGraphicsQueue() const { return graphics_queue_; }
        VkQueue GetTransferQueue() const { return transfer_queue_; }
        uint32_t GetGraphicsQueueFamily() const { return queue_indices_.graphics_family; }
        uint32_t GetTransferQueueFamily() const { return queue_indices_.transfer_family; }
        bool HasDedicatedTransferQueue() const { return queue_indices_.has_dedicated_transfer; }

        // 内存分配器访问
        VmaAllocator GetAllocator() const { return allocator_; }

        // 同步原语管理
        VkFence CreateFence(bool signaled = false, const char *debug_name = nullptr);
        VkSemaphore CreateSemaphore(const char *debug_name = nullptr);
        void DestroyFence(VkFence fence);
        void DestroySemaphore(VkSemaphore semaphore);

        // 队列操作
        struct QueueSubmitInfo
        {
            const VkSubmitInfo *submit_info;
            uint32_t submit_count;
            VkFence fence;
        };

        struct QueuePresentInfo
        {
            const VkPresentInfoKHR *present_info;
        };

        // Generic queue operations
        bool Submit(VkQueue queue, const QueueSubmitInfo &submit_info);
        bool Present(VkQueue queue, const QueuePresentInfo &present_info);
        bool WaitQueueIdle(VkQueue queue);

        // Queue family operations
        uint32_t GetQueueFamilyIndex(VkQueueFlags required_flags) const;
        VkQueue GetQueue(uint32_t family_index, uint32_t queue_index = 0) const;

        // 命令缓冲区管理
        bool InitCommandBuffers(uint32_t num_threads);
        void ShutdownCommandBuffers();
        CommandBufferManager &GetCommandBufferManager() { return command_buffer_manager_; }

        // 辅助功能
        void SetDebugName(VkObjectType type, uint64_t handle, const char *name);

        // 资源管理
        BufferHandle CreateBuffer(const BufferCreationInfo &info);
        void DestroyBuffer(BufferHandle handle);
        AllocatedBufferUntyped &GetBuffer(BufferHandle handle);

        TextureHandle CreateTexture(const TextureCreationInfo &info);
        void DestroyTexture(TextureHandle handle);
        AllocatedImage &GetTexture(TextureHandle handle);

        // 获取缓冲区创建信息
        BufferCreationInfo GetBufferCreationInfo(BufferHandle handle) const;
        // 获取纹理创建信息
        TextureCreationInfo GetTextureCreationInfo(TextureHandle handle) const;

        // Surface management
        bool CreateSurface(SDL_Window *window);
        void DestroySurface();
        VkSurfaceKHR GetSurface() const { return surface_; }

        // Descriptor management
        struct DescriptorLayoutInfo
        {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            VkDescriptorSetLayoutCreateFlags flags = 0;
            VkShaderStageFlags shader_stages = VK_SHADER_STAGE_ALL;
            void *pNext = nullptr;
        };

        struct DescriptorAllocatorCreateInfo
        {
            std::span<lc::DescriptorAllocatorGrowable::PoolSizeRatio> pool_sizes;
            uint32_t max_sets = 1000;
        };

        // Descriptor layout management
        VkDescriptorSetLayout CreateDescriptorSetLayout(const DescriptorLayoutInfo &layout_info);
        void DestroyDescriptorSetLayout(VkDescriptorSetLayout layout);

        // Descriptor allocator management
        bool InitDescriptorAllocator(const DescriptorAllocatorCreateInfo &create_info);
        void DestroyDescriptorAllocator();
        VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout, void *pNext = nullptr);

        // Descriptor writing
        void UpdateDescriptorSet(VkDescriptorSet set, const std::vector<VkWriteDescriptorSet> &writes);

        // Vulkan 1.2 and 1.3 feature support
        VkPhysicalDeviceVulkan13Features features_13_{};
        VkPhysicalDeviceVulkan12Features features_12_{};
        VkPhysicalDeviceFeatures features_10_{};

        // Device properties and features
        VkPhysicalDeviceProperties2 properties_2_{};
        VkPhysicalDeviceMemoryProperties memory_properties_{};

        // Required device extensions
        static constexpr const char *required_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};

    private:
        VkInstance CreateInstance(const char *app_name, const char *engine_name, uint32_t api_version);
        bool InitVulkanFeatures();
        bool InitVulkanDevice(VkSurfaceKHR surface);
        bool InitVulkanAllocator();
        bool SelectPhysicalDevice(VkSurfaceKHR surface);
        void LogDeviceProperties(VkPhysicalDevice device);
        bool CreateDevice(VkSurfaceKHR surface);
        bool SetupDebugMessenger();
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphics_queue_ = VK_NULL_HANDLE;
        VkQueue transfer_queue_ = VK_NULL_HANDLE;
        QueueFamilyIndices queue_indices_{};
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;

        // 资源池
        ResourcePool<BufferHandle, AllocatedBufferUntyped> buffer_pool_;
        ResourcePool<TextureHandle, AllocatedImage> texture_pool_;
        std::unordered_map<BufferHandle, BufferCreationInfo> buffer_creation_infos_;
        std::unordered_map<TextureHandle, TextureCreationInfo> texture_creation_infos_;
        mutable std::shared_mutex creation_info_mutex_;
        CommandBufferManager command_buffer_manager_;

        // Descriptor allocator
        lc::DescriptorAllocatorGrowable descriptor_allocator_;
    };

} // namespace lc
