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

// 前向声明
namespace vkb { struct Instance; }

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
        VkSurfaceKHR surface;
    };

    bool Init(const CreateInfo& create_info);
    void Cleanup();

    // 设备功能访问
    VkDevice GetDevice() const { return device_; }
    VkPhysicalDevice GetPhysicalDevice() const { return physical_device_; }
    const VkPhysicalDeviceProperties& GetDeviceProperties() const { return properties_; }
    const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return features_; }
    
    // 队列访问
    VkQueue GetGraphicsQueue() const { return graphics_queue_; }
    VkQueue GetTransferQueue() const { return transfer_queue_; }
    uint32_t GetGraphicsQueueFamily() const { return queue_indices_.graphics_family; }
    uint32_t GetTransferQueueFamily() const { return queue_indices_.transfer_family; }
    bool HasDedicatedTransferQueue() const { return queue_indices_.has_dedicated_transfer; }

    // 内存分配器访问
    VmaAllocator GetAllocator() const { return allocator_; }

    // 同步原语管理
    VkFence CreateFence(bool signaled = false, const char* debug_name = nullptr);
    VkSemaphore CreateSemaphore(const char* debug_name = nullptr);
    void DestroyFence(VkFence fence);
    void DestroySemaphore(VkSemaphore semaphore);

    // 队列操作
    void SubmitToGraphicsQueue(const VkSubmitInfo& submit_info, VkFence fence = VK_NULL_HANDLE);
    void SubmitToTransferQueue(const VkSubmitInfo& submit_info, VkFence fence = VK_NULL_HANDLE);
    void WaitGraphicsQueueIdle();
    void WaitTransferQueueIdle();

    // 命令池管理
    struct CommandPoolConfig {
        uint32_t queue_family_index;
        VkCommandPoolCreateFlags flags = 0;
        const char* debug_name = nullptr;
    };

    struct CommandBufferConfig {
        VkCommandPool command_pool;
        uint32_t count;
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        const char* debug_name = nullptr;
    };

    VkCommandPool CreateCommandPool(const CommandPoolConfig& config);
    void DestroyCommandPool(VkCommandPool pool);
    std::vector<VkCommandBuffer> AllocateCommandBuffers(const CommandBufferConfig& config);
    void FreeCommandBuffers(VkCommandPool pool, const std::vector<VkCommandBuffer>& command_buffers);

    // 辅助功能
    VkCommandBuffer BeginSingleTimeCommands(VkCommandPool pool);
    void EndSingleTimeCommands(VkCommandBuffer command_buffer, VkCommandPool pool);
    void SetDebugName(VkObjectType type, uint64_t handle, const char* name);

    // 资源管理
    BufferHandle CreateBuffer(const BufferCreationInfo& info);
    void DestroyBuffer(BufferHandle handle);
    AllocatedBufferUntyped& GetBuffer(BufferHandle handle);

    TextureHandle CreateTexture(const TextureCreationInfo& info);
    void DestroyTexture(TextureHandle handle);
    AllocatedImage& GetTexture(TextureHandle handle);

    // 获取缓冲区创建信息
    BufferCreationInfo GetBufferCreationInfo(BufferHandle handle) const;
    // 获取纹理创建信息
    TextureCreationInfo GetTextureCreationInfo(TextureHandle handle) const;

private:
    VkInstance CreateInstance(const char* app_name, const char* engine_name, uint32_t api_version);
    bool CreateDevice(VkSurfaceKHR surface);
    bool CreateAllocator();
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

    // 资源池
    ResourcePool<BufferHandle, AllocatedBufferUntyped> buffer_pool_;
    ResourcePool<TextureHandle, AllocatedImage> texture_pool_;
    std::unordered_map<BufferHandle, BufferCreationInfo> buffer_creation_infos_;
    std::unordered_map<TextureHandle, TextureCreationInfo> texture_creation_infos_;
    mutable std::shared_mutex creation_info_mutex_;
};
