#include "vk_resources.h"
#include "vk_engine.h"

TextureCreationInfo &TextureCreationInfo::SetSize(uint32_t width, uint32_t height, uint32_t depth)
{
    extent.width = width;
    extent.height = height;
    extent.depth = depth;
    return *this;
}

TextureCreationInfo& TextureCreationInfo::SetSize(VkExtent3D size)
{
	extent = size;
	return *this;
}

TextureCreationInfo &TextureCreationInfo::SetFormatType(VkFormat fmt, TextureType::Enum tex_type)
{
    format = fmt;
    type = tex_type;
    return *this;
}

TextureCreationInfo &TextureCreationInfo::SetFlags(uint32_t mips, VkImageCreateFlags create_flags)
{
    mip_levels = mips;
    flags = create_flags;
    return *this;
}

TextureCreationInfo &TextureCreationInfo::SetLayout(VkImageLayout layout)
{
    initial_layout = layout;
    return *this;
}

TextureCreationInfo &TextureCreationInfo::SetUsage(VkImageUsageFlags usage_flags)
{
    usage = usage_flags;
    return *this;
}

TextureCreationInfo &TextureCreationInfo::SetData(void *data)
{
    initial_data = data;
    return *this;
}

// Helper function to set array layers based on texture type
void TextureCreationInfo::SetArrayLayers(uint32_t layers)
{
    array_layers = layers;
    if (type == TextureType::Texture_Cube_Array)
    {
        array_layers *= 6; // Cube maps need 6 faces per array layer
    }
}

TextureCreationInfo& TextureCreationInfo::SetName(const char* buffer_name)
{
    name = buffer_name;
    return *this;
}

// Helper function to validate creation info based on texture type
bool TextureCreationInfo::Validate() const
{
    switch (type)
    {
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

BufferCreationInfo &BufferCreationInfo::Reset()
{
    usage = 0;
    size = 0;
    memory_usage = VMA_MEMORY_USAGE_UNKNOWN;
    initial_data = nullptr;
    name = nullptr;
    return *this;
}

BufferCreationInfo &BufferCreationInfo::Set(VkBufferUsageFlags flags, VmaMemoryUsage mem_usage, VkDeviceSize buffer_size)
{
    usage = flags;
    memory_usage = mem_usage;
    size = buffer_size;
    return *this;
}

BufferCreationInfo &BufferCreationInfo::SetData(void *data)
{
    initial_data = data;
    return *this;
}

BufferCreationInfo &BufferCreationInfo::SetName(const char *buffer_name)
{
    name = buffer_name;
    return *this;
}

BufferHandle ResourceManager::CreateBuffer(const BufferCreationInfo &info)
{
    BufferHandle handle = buffer_pool_.Allocate();

    {
        std::unique_lock lock(creation_info_mutex_);
        buffer_creation_infos_[handle] = info;
    }

    // Create the actual buffer resource
    CreateBufferResource(handle);

    return handle;
}

void ResourceManager::CreateBufferResource(BufferHandle handle)
{
    BufferCreationInfo info;
    {
        std::shared_lock lock(creation_info_mutex_);
        info = buffer_creation_infos_[handle];
    }

    // Create buffer using VMA
    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = info.size;
    buffer_info.usage = info.usage;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = info.memory_usage;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    AllocatedBufferUntyped buffer;
    vmaCreateBuffer(engine_->allocator_, &buffer_info, &alloc_info,
                    &buffer.buffer, &buffer.allocation, &buffer.info);

    // Store the created buffer
    buffer_pool_.Set(handle, std::move(buffer));
}

AllocatedBufferUntyped &ResourceManager::GetBuffer(BufferHandle handle)
{
    return buffer_pool_.Get(handle);
}

void ResourceManager::DestroyBuffer(BufferHandle handle)
{
    AllocatedBufferUntyped &buffer = GetBuffer(handle);
    vmaDestroyBuffer(engine_->allocator_, buffer.buffer, buffer.allocation);

    {
        std::unique_lock lock(creation_info_mutex_);
        buffer_creation_infos_.erase(handle);
    }

    buffer_pool_.Free(handle);
}

TextureHandle ResourceManager::CreateTexture(const TextureCreationInfo &info)
{
    TextureHandle handle = texture_pool_.Allocate();

    {
        std::unique_lock lock(creation_info_mutex_);
        texture_creation_infos_[handle] = info;
    }

    // Create the actual texture resource
    CreateTextureResource(handle);

    return handle;
}

void ResourceManager::CreateTextureResource(TextureHandle handle)
{
    TextureCreationInfo info;
    {
        std::shared_lock lock(creation_info_mutex_);
        info = texture_creation_infos_[handle];
    }

    // Validate creation info
    if (!info.Validate())
    {
        throw std::runtime_error("Invalid texture creation info");
    }

    // Create image using VMA
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

    // Set image type based on texture type
    switch (info.type)
    {
    case TextureType::Texture1D:
    case TextureType::Texture_1D_Array:
        imageInfo.imageType = VK_IMAGE_TYPE_1D;
        break;
    case TextureType::Texture2D:
    case TextureType::Texture_2D_Array:
    case TextureType::Texture_Cube_Array:
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    case TextureType::Texture3D:
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        break;
    default:
        throw std::runtime_error("Unsupported texture type");
    }

    // Set image creation flags
    if (info.type == TextureType::Texture_Cube_Array)
    {
        imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    imageInfo.format = info.format;
    imageInfo.extent = info.extent;
    imageInfo.mipLevels = info.mip_levels;
    imageInfo.arrayLayers = info.array_layers;
    imageInfo.samples = info.samples;
    imageInfo.tiling = info.tiling;
    imageInfo.usage = info.usage;
    imageInfo.initialLayout = info.initial_layout;
    imageInfo.flags |= info.flags;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    AllocatedImage image;
    image.format = info.format;
    image.extent = info.extent;

    vmaCreateImage(engine_->allocator_, &imageInfo, &alloc_info,
                   &image.image, &image.allocation, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image.image;
    viewInfo.format = info.format;

    // Set view type based on texture type
    switch (info.type)
    {
    case TextureType::Texture1D:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
        break;
    case TextureType::Texture2D:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case TextureType::Texture3D:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    case TextureType::Texture_1D_Array:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        break;
    case TextureType::Texture_2D_Array:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        break;
    case TextureType::Texture_Cube_Array:
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        break;
    default:
        throw std::runtime_error("Unsupported texture type");
    }

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = info.mip_levels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = info.array_layers;

    VkResult result = vkCreateImageView(engine_->device_, &viewInfo, nullptr, &image.view);
    if (result != VK_SUCCESS)
    {
        vmaDestroyImage(engine_->allocator_, image.image, image.allocation);
        throw std::runtime_error("Failed to create image view");
    }

    // Store the created image
    texture_pool_.Set(handle, std::move(image));
}

AllocatedImage &ResourceManager::GetTexture(TextureHandle handle)
{
    return texture_pool_.Get(handle);
}

void ResourceManager::DestroyTexture(TextureHandle handle)
{
    AllocatedImage &image = GetTexture(handle);
    vkDestroyImageView(engine_->device_, image.view, nullptr);
    vmaDestroyImage(engine_->allocator_, image.image, image.allocation);

    {
        std::unique_lock lock(creation_info_mutex_);
        texture_creation_infos_.erase(handle);
    }

    texture_pool_.Free(handle);
}

void ResourceManager::Init(VulkanEngine *engine)
{
    engine_ = engine;
}

void ResourceManager::CleanUp()
{
    // Destroy all remaining resources
    std::vector<BufferHandle> buffers;
    std::vector<TextureHandle> textures;

    {
        std::shared_lock lock(creation_info_mutex_);
        for (const auto &pair : buffer_creation_infos_)
        {
            buffers.push_back(pair.first);
        }
        for (const auto &pair : texture_creation_infos_)
        {
            textures.push_back(pair.first);
        }
    }

    for (auto handle : buffers)
    {
        DestroyBuffer(handle);
    }

    for (auto handle : textures)
    {
        DestroyTexture(handle);
    }
}

ExecutionBarrier& ExecutionBarrier::Reset()
{
    num_image_barriers = num_buffer_barriers = 0;
    return *this;
}

ExecutionBarrier& ExecutionBarrier::AddImageBarrier(const ImageBarrier& barrier)
{
    image_barriers[num_image_barriers++] = barrier;

    return *this;
}

ExecutionBarrier& ExecutionBarrier::AddBufferBarrier(const BufferBarrier& barrier)
{
    buffer_barriers[num_buffer_barriers++] = barrier;

    return *this;
}
