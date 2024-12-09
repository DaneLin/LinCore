#pragma once

#include <mutex>
#include <shared_mutex>
#include <queue>
#include <unordered_map>
#include <gpu_enums.h>
#include <vk_mem_alloc.h>
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

struct AllocatedImage
{
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

struct AllocatedBufferUntyped
{
    VkBuffer buffer{ VK_NULL_HANDLE };
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
    VkDeviceSize size{ 0 };
    VkDescriptorBufferInfo GetInfo(VkDeviceSize offset = 0) const{
        return VkDescriptorBufferInfo{ .buffer=buffer,.offset = offset, .range = size };
    }
};


template<typename T>
struct AllocatedBuffer : public AllocatedBufferUntyped {
    void operator=(const AllocatedBufferUntyped& other) {
        buffer = other.buffer;
        allocation = other.allocation;
        info = other.info;
        size = other.size;
    }
    AllocatedBuffer(AllocatedBufferUntyped& other) {
        buffer = other.buffer;
        allocation = other.allocation;
        info = other.info;
        size = other.size;
    }
    AllocatedBuffer() = default;
};


// Resource creation info base class
struct ResourceCreationInfo {
    virtual ~ResourceCreationInfo() = default;
};

// Buffer creation info
struct BufferCreationInfo : ResourceCreationInfo {
    VkBufferUsageFlags usage;
    VkDeviceSize size;
    VmaMemoryUsage memory_usage;
    void* initial_data{ nullptr };
    const char* name{ nullptr };

    BufferCreationInfo& Reset();
    BufferCreationInfo& Set(VkBufferUsageFlags flags, VmaMemoryUsage mem_usage, VkDeviceSize buffer_size) ;
    BufferCreationInfo& SetData(void* data) ;
    BufferCreationInfo& SetName(const char* buffer_name) ;
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
    void* initial_data{ nullptr };
    const char* name{ nullptr };

    // Builder style methods
    TextureCreationInfo& SetSize(uint32_t width, uint32_t height, uint32_t depth = 1);
	TextureCreationInfo& SetSize(VkExtent3D size);
    TextureCreationInfo& SetFormatType(VkFormat fmt, TextureType::Enum tex_type);
    TextureCreationInfo& SetFlags(uint32_t mips, VkImageCreateFlags create_flags) ;
    TextureCreationInfo& SetLayout(VkImageLayout layout) ;
    TextureCreationInfo& SetUsage(VkImageUsageFlags usage_flags) ;
    TextureCreationInfo& SetData(void* data) ;
    TextureCreationInfo& SetName(const char* buffer_name);
    // Helper function to set array layers based on texture type
    void SetArrayLayers(uint32_t layers) ;
    // Helper function to validate creation info based on texture type
    bool Validate() const ;
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
    void CleanUp();

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

namespace TextureFormat {

    inline bool IsDepthStencil( VkFormat value ) {
        return value >= VK_FORMAT_D16_UNORM_S8_UINT && value < VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    }
    inline bool IsDepthOnly( VkFormat value ) {
        return value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_S8_UINT;
    }
    inline bool IsStencilOnly( VkFormat value ) {
        return value == VK_FORMAT_S8_UINT;
    }

    inline bool HasDepth( VkFormat value ) {
        return IsDepthOnly(value) || IsDepthStencil( value );
    }
    inline bool HasStencil( VkFormat value ) {
        return value >= VK_FORMAT_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    inline bool HasDepthOrStencil( VkFormat value ) {
        return value >= VK_FORMAT_D16_UNORM && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

} // namespace TextureFormat

// Synchronization //////////////////////////////////////////////////////////////

//
//
struct ImageBarrier {

    TextureHandle                   texture             = kInvalidTextureHandle;
    ResourceState                   destination_state   = RESOURCE_STATE_UNDEFINED; // Source state is saved in the texture.

    uint16_t                             array_base_layer    = 0;
    uint16_t                             array_layer_count   = 1;
    uint16_t                             mip_base_level      = 0;
    uint16_t                             mip_level_count     = 1;

}; // struct ImageBarrier

//
//
struct BufferBarrier {

    BufferHandle                    buffer              = kInvalidBufferHandle;
    ResourceState                   source_state        = RESOURCE_STATE_UNDEFINED;
    ResourceState                   destination_state   = RESOURCE_STATE_UNDEFINED;
    uint32_t                             offset              = 0;
    uint32_t                             size                = 0;

}; // struct MemoryBarrier

//
//
struct ExecutionBarrier {

    static constexpr uint32_t            k_max_barriers = 8;

    uint32_t                             num_image_barriers      = 0;
    uint32_t                             num_buffer_barriers     = 0;

    ImageBarrier                    image_barriers[ k_max_barriers ];
    BufferBarrier                   buffer_barriers[ k_max_barriers ];

    ExecutionBarrier&               Reset();
    ExecutionBarrier&               AddImageBarrier( const ImageBarrier& barrier );
    ExecutionBarrier&               AddBufferBarrier( const BufferBarrier& barrier );

}; // struct ExecutionBarrier

//
//
struct ResourceUpdate {

    ResourceUpdateType::Enum        type;
    ResourceHandle                  handle;
    uint32_t                             current_frame;
    uint32_t                             deleting;
}; // struct ResourceUpdate
