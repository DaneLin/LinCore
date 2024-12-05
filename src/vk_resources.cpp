#include "vk_resources.h"
#include "vk_engine.h"

BufferHandle ResourceManager::CreateBuffer(const BufferCreationInfo& info) {
    BufferHandle handle = buffer_pool_.Allocate();
    
    {
        std::unique_lock lock(creation_info_mutex_);
        buffer_creation_infos_[handle] = info;
    }
    
    // Create the actual buffer resource
    CreateBufferResource(handle);
    
    return handle;
}

void ResourceManager::CreateBufferResource(BufferHandle handle) {
    BufferCreationInfo info;
    {
        std::shared_lock lock(creation_info_mutex_);
        info = buffer_creation_infos_[handle];
    }

    // Create buffer using VMA
    VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.size = info.size;
    buffer_info.usage = info.usage;
    
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = info.memory_usage;
    
    AllocatedBufferUntyped buffer;
    vmaCreateBuffer(engine_->allocator_, &buffer_info, &alloc_info, 
                   &buffer.buffer, &buffer.allocation, &buffer.info);

    // Store the created buffer
    buffer_pool_.Set(handle, std::move(buffer));
}

AllocatedBufferUntyped& ResourceManager::GetBuffer(BufferHandle handle) {
    return buffer_pool_.Get(handle);
}

void ResourceManager::DestroyBuffer(BufferHandle handle) {
    AllocatedBufferUntyped& buffer = GetBuffer(handle);
    vmaDestroyBuffer(engine_->allocator_, buffer.buffer, buffer.allocation);
    
    {
        std::unique_lock lock(creation_info_mutex_);
        buffer_creation_infos_.erase(handle);
    }
    
    buffer_pool_.Free(handle);
}

TextureHandle ResourceManager::CreateTexture(const TextureCreationInfo& info) {
    TextureHandle handle = texture_pool_.Allocate();
    
    {
        std::unique_lock lock(creation_info_mutex_);
        texture_creation_infos_[handle] = info;
    }
    
    // Create the actual texture resource
    CreateTextureResource(handle);
    
    return handle;
}

void ResourceManager::CreateTextureResource(TextureHandle handle) {
    TextureCreationInfo info;
    {
        std::shared_lock lock(creation_info_mutex_);
        info = texture_creation_infos_[handle];
    }

    // Validate creation info
    if (!info.Validate()) {
        throw std::runtime_error("Invalid texture creation info");
    }

    // Create image using VMA
    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    
    // Set image type based on texture type
    switch (info.type) {
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
    if (info.type == TextureType::Texture_Cube_Array) {
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
    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = image.image;
    viewInfo.format = info.format;
    
    // Set view type based on texture type
    switch (info.type) {
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
    if (result != VK_SUCCESS) {
        vmaDestroyImage(engine_->allocator_, image.image, image.allocation);
        throw std::runtime_error("Failed to create image view");
    }

    // Store the created image
    texture_pool_.Set(handle, std::move(image));
}

AllocatedImage& ResourceManager::GetTexture(TextureHandle handle) {
    return texture_pool_.Get(handle);
}

void ResourceManager::DestroyTexture(TextureHandle handle) {
    AllocatedImage& image = GetTexture(handle);
    vkDestroyImageView(engine_->device_, image.view, nullptr);
    vmaDestroyImage(engine_->allocator_, image.image, image.allocation);
    
    {
        std::unique_lock lock(creation_info_mutex_);
        texture_creation_infos_.erase(handle);
    }
    
    texture_pool_.Free(handle);
}

void ResourceManager::Init(VulkanEngine* engine) {
    engine_ = engine;
}

void ResourceManager::Cleanup() {
    // Destroy all remaining resources
    std::vector<BufferHandle> buffers;
    std::vector<TextureHandle> textures;
    
    {
        std::shared_lock lock(creation_info_mutex_);
        for (const auto& pair : buffer_creation_infos_) {
            buffers.push_back(pair.first);
        }
        for (const auto& pair : texture_creation_infos_) {
            textures.push_back(pair.first);
        }
    }
    
    for (auto handle : buffers) {
        DestroyBuffer(handle);
    }
    
    for (auto handle : textures) {
        DestroyTexture(handle);
    }
}
