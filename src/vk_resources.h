#pragma once

#include "vk_types.h"
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <unordered_map>
#include <gpu_enums.h>

// Forward declaration
class VulkanEngine;

static const uint32_t kInvalidIndex = UINT32_MAX;

typedef uint32_t ResourceHandle;

// Resource handle types
struct BufferHandle {
    ResourceHandle index;
    bool operator==(const BufferHandle& other) const { return index == other.index; }
};

struct TextureHandle {
    ResourceHandle index;
    bool operator==(const TextureHandle& other) const { return index == other.index; }
};

struct ShaderStateHandle {
    ResourceHandle index;
    bool operator==(const ShaderStateHandle& other) const { return index == other.index; }
};

struct DescriptorSetHandle {
    ResourceHandle index;
    bool operator==(const DescriptorSetHandle& other) const { return index == other.index; }
};

struct PipelineHandle {
    ResourceHandle index;
    bool operator==(const PipelineHandle& other) const { return index == other.index; }
};

struct RenderPassHandle {
    ResourceHandle index;
    bool operator==(const RenderPassHandle& other) const { return index == other.index; }
};

// Hash functions for handles
namespace std {
    template<>
    struct hash<BufferHandle> {
        size_t operator()(const BufferHandle& h) const { return h.index; }
    };
    
    template<>
    struct hash<TextureHandle> {
        size_t operator()(const TextureHandle& h) const { return h.index; }
    };

    template<>
    struct hash<ShaderStateHandle> {
        size_t operator()(const ShaderStateHandle& h) const { return h.index; }
    };

    template<>
    struct hash<DescriptorSetHandle> {
        size_t operator()(const DescriptorSetHandle& h) const { return h.index; }
    };

    template<>
    struct hash<PipelineHandle> {
        size_t operator()(const PipelineHandle& h) const { return h.index; }
    };

    template<>
    struct hash<RenderPassHandle> {
        size_t operator()(const RenderPassHandle& h) const { return h.index; }
    };
}

// Invaild handles
static BufferHandle kInvalidBufferHandle = {kInvalidIndex};
static TextureHandle kInvalidTextureHandle = {kInvalidIndex};
static ShaderStateHandle kInvalidShaderStateHandle = { kInvalidIndex };
static DescriptorSetHandle kInvalidDescriptorSetHandle = { kInvalidIndex };
static PipelineHandle kInvalidPipelineHandle = { kInvalidIndex };
static RenderPassHandle kInvalidRenderPassHandle = { kInvalidIndex };


// Resource creation info base class
struct ResourceCreationInfo {
    virtual ~ResourceCreationInfo() = default;
};

// Buffer creation info
struct BufferCreationInfo : ResourceCreationInfo {
    VkBufferUsageFlags usage;
    VkDeviceSize size;
    VmaMemoryUsage memory_usage;
};

// Texture creation info
struct TextureCreationInfo : ResourceCreationInfo {
    TextureType::Enum type;
    VkFormat format;
    VkExtent3D extent;
    VkImageUsageFlags usage;
    VkImageLayout initial_layout;
    uint32_t mip_levels{ 1 };
    uint32_t array_layers{ 1 };
    VkSampleCountFlagBits samples{ VK_SAMPLE_COUNT_1_BIT };
    VkImageTiling tiling{ VK_IMAGE_TILING_OPTIMAL };
    VkImageCreateFlags flags{ 0 };

    // Helper function to set array layers based on texture type
    void SetArrayLayers(uint32_t layers) {
        array_layers = layers;
        if (type == TextureType::Texture_Cube_Array) {
            array_layers *= 6; // Cube maps need 6 faces per array layer
        }
    }

    // Helper function to validate creation info based on texture type
    bool Validate() const {
        switch (type) {
            case TextureType::Texture1D:
            case TextureType::Texture_1D_Array:
                return extent.width > 0 && extent.height == 1 && extent.depth == 1;
            case TextureType::Texture2D:
            case TextureType::Texture_2D_Array:
                return extent.width > 0 && extent.height > 0 && extent.depth == 1;
            case TextureType::Texture3D:
                return extent.width > 0 && extent.height > 0 && extent.depth > 0;
            case TextureType::Texture_Cube_Array:
                return extent.width > 0 && extent.height > 0 && extent.depth == 1 && 
                       (array_layers % 6) == 0 && extent.width == extent.height;
            default:
                return false;
        }
    }
};

// Resource pool template class
template<typename HandleType, typename ResourceType>
class ResourcePool {
public:
    ResourcePool() = default;
    virtual ~ResourcePool() = default;

    HandleType Allocate() {
        std::unique_lock lock(mutex_);
        
        if (!free_list_.empty()) {
            HandleType handle = {free_list_.front()};
            free_list_.pop();
            return handle;
        }

        HandleType handle = {static_cast<uint32_t>(resources_.size())};
        resources_.push_back({});
        return handle;
    }

    void Free(HandleType handle) {
        std::unique_lock lock(mutex_);
        free_list_.push(handle.index);
    }

    ResourceType& Get(HandleType handle) {
        std::shared_lock lock(mutex_);
        return resources_[handle.index];
    }

    void Set(HandleType handle, ResourceType&& resource) {
        std::unique_lock lock(mutex_);
        resources_[handle.index] = std::move(resource);
    }

protected:
    std::vector<ResourceType> resources_;
    std::queue<uint32_t> free_list_;
    mutable std::shared_mutex mutex_;
};

// Resource manager class
class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;

    void Init(VulkanEngine* engine);
    void Cleanup();

    // Buffer resource creation and management
    BufferHandle CreateBuffer(const BufferCreationInfo& info);
    AllocatedBufferUntyped& GetBuffer(BufferHandle handle);
    void DestroyBuffer(BufferHandle handle);

    // Texture resource creation and management
    TextureHandle CreateTexture(const TextureCreationInfo& info);
    AllocatedImage& GetTexture(TextureHandle handle);
    void DestroyTexture(TextureHandle handle);

private:
    void CreateBufferResource(BufferHandle handle);
    void CreateTextureResource(TextureHandle handle);

    VulkanEngine* engine_{ nullptr };
    
    ResourcePool<BufferHandle, AllocatedBufferUntyped> buffer_pool_;
    ResourcePool<TextureHandle, AllocatedImage> texture_pool_;
    
    std::unordered_map<BufferHandle, BufferCreationInfo> buffer_creation_infos_;
    std::unordered_map<TextureHandle, TextureCreationInfo> texture_creation_infos_;
    
    mutable std::shared_mutex creation_info_mutex_;
};
