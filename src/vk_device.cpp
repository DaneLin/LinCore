#include "vk_device.h"
#include <iostream>
#include "logging.h"
#include <set>
#include <vector>
#include <algorithm>

bool GPUDevice::Init(const CreateInfo& create_info) {
    instance_ = CreateInstance(create_info.app_name, create_info.engine_name, create_info.api_version);
    if (instance_ == VK_NULL_HANDLE) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }
    
    if (!SetupDebugMessenger()) {
        LOGE("Failed to setup debug messenger");
        return false;
    }

    if (!CreateDevice(create_info.surface)) {
        LOGE("Failed to create logical device");
        return false;
    }

    return true;
}

VkInstance GPUDevice::CreateInstance(const char* app_name, const char* engine_name, uint32_t api_version) {
    vkb::InstanceBuilder builder;
    
    auto inst_ret = builder.set_app_name(app_name)
        .set_engine_name(engine_name)
        .request_validation_layers(true)
        .use_default_debug_messenger()
        .require_api_version(api_version)
        .build();

    if (!inst_ret) {
        LOGE("Failed to create Vulkan instance: {}", inst_ret.error().message());
        return VK_NULL_HANDLE;
    }

    vkb::Instance vkb_inst = inst_ret.value();
    return vkb_inst.instance;
}

void GPUDevice::Cleanup() {
    // 等待设备完成所有操作
    if (device_) {
        vkDeviceWaitIdle(device_);
    }

    // 清理资源池中的资源
   
    // 清理VMA分配器
    if (allocator_) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
    
    // 清理设备
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    // 清理调试信使
    if (debug_messenger_) {
        vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
        debug_messenger_ = VK_NULL_HANDLE;
    }
}

bool GPUDevice::CreateDevice(VkSurfaceKHR surface) {
    try {
        // Use VkBootstrap's device selector directly with the physical device
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
        if (device_count == 0) {
            LOGE("Failed to find GPUs with Vulkan support!");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

        // Find a suitable device
        for (const auto& device : devices) {
            physical_device_ = device;
            queue_indices_ = FindQueueFamilies(device, surface);
            
            if (queue_indices_.graphics_family != UINT32_MAX) {
                // Found a suitable device
                break;
            }
        }

        if (physical_device_ == VK_NULL_HANDLE) {
            LOGE("Failed to find a suitable GPU!");
            return false;
        }

        // Get device properties
        vkGetPhysicalDeviceProperties(physical_device_, &properties_);
        vkGetPhysicalDeviceFeatures(physical_device_, &features_);

        // Create logical device
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<uint32_t> unique_queue_families = { queue_indices_.graphics_family };
        if (queue_indices_.transfer_family != queue_indices_.graphics_family) {
            unique_queue_families.insert(queue_indices_.transfer_family);
        }

        float queue_priority = 1.0f;
        for (uint32_t queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.pEnabledFeatures = &features_;

        if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
            LOGE("Failed to create logical device!");
            return false;
        }

        // Get queue handles
        vkGetDeviceQueue(device_, queue_indices_.graphics_family, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, queue_indices_.transfer_family, 0, &transfer_queue_);

        return true;
    }
    catch (const std::exception& e) {
        LOGE("Device creation failed: {}", e.what());
        return false;
    }
}

bool GPUDevice::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                   void* pUserData) -> VKAPI_ATTR VkBool32 {
        switch (messageSeverity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                LOGD("[Vulkan] {}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                LOGI("[Vulkan] {}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                LOGW("[Vulkan] {}", pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                LOGE("[Vulkan] {}", pCallbackData->pMessage);
                break;
            default:
                LOGI("[Vulkan] {}", pCallbackData->pMessage);
                break;
        }
        return VK_FALSE;
    };

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT == nullptr) {
        LOGE("Failed to get vkCreateDebugUtilsMessengerEXT function pointer");
        return false;
    }

    if (vkCreateDebugUtilsMessengerEXT(instance_, &create_info, nullptr, &debug_messenger_) != VK_SUCCESS) {
        LOGE("Failed to create debug utils messenger");
        return false;
    }

    return true;
}

GPUDevice::QueueFamilyIndices GPUDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices{};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    // 查找支持图形的队列族
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            break;
        }
    }

    // 查找专用传输队列族
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.transfer_family = i;
            indices.has_dedicated_transfer = true;
            break;
        }
    }

    return indices;
}

void GPUDevice::SetDebugName(VkObjectType type, uint64_t handle, const char* name) {
    if (!name) return;

    VkDebugUtilsObjectNameInfoEXT name_info{};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name;

    auto vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance_, "vkSetDebugUtilsObjectNameEXT");
    if (vkSetDebugUtilsObjectNameEXT) {
        vkSetDebugUtilsObjectNameEXT(device_, &name_info);
    }
}

VkCommandPool GPUDevice::CreateCommandPool(const CommandPoolConfig& config) {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = config.queue_family_index;
    pool_info.flags = config.flags;

    VkCommandPool pool;
    if (vkCreateCommandPool(device_, &pool_info, nullptr, &pool) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
        return VK_NULL_HANDLE;
    }

    if (config.debug_name) {
        SetDebugName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)pool, config.debug_name);
    }

    return pool;
}

void GPUDevice::DestroyCommandPool(VkCommandPool pool) {
    if (pool) {
        vkDestroyCommandPool(device_, pool, nullptr);
    }
}

std::vector<VkCommandBuffer> GPUDevice::AllocateCommandBuffers(const CommandBufferConfig& config) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = config.command_pool;
    alloc_info.level = config.level;
    alloc_info.commandBufferCount = config.count;

    std::vector<VkCommandBuffer> command_buffers(config.count);
    if (vkAllocateCommandBuffers(device_, &alloc_info, command_buffers.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers");
        return {};
    }

    if (config.debug_name) {
        for (uint32_t i = 0; i < config.count; i++) {
            SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)command_buffers[i], config.debug_name);
        }
    }

    return command_buffers;
}

void GPUDevice::FreeCommandBuffers(VkCommandPool pool, const std::vector<VkCommandBuffer>& command_buffers) {
    if (!command_buffers.empty()) {
        vkFreeCommandBuffers(device_, pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
    }
}

VkFence GPUDevice::CreateFence(bool signaled, const char* debug_name) {
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VkFence fence;
    if (vkCreateFence(device_, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        LOGE("Failed to create fence");
        return VK_NULL_HANDLE;
    }

    if (debug_name) {
        SetDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, debug_name);
    }

    return fence;
}

VkSemaphore GPUDevice::CreateSemaphore(const char* debug_name) {
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore;
    if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
        LOGE("Failed to create semaphore");
        return VK_NULL_HANDLE;
    }

    if (debug_name) {
        SetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)semaphore, debug_name);
    }

    return semaphore;
}

void GPUDevice::DestroyFence(VkFence fence) {
    if (fence) {
        vkDestroyFence(device_, fence, nullptr);
    }
}

void GPUDevice::DestroySemaphore(VkSemaphore semaphore) {
    if (semaphore) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
}

VkCommandBuffer GPUDevice::BeginSingleTimeCommands(VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void GPUDevice::EndSingleTimeCommands(VkCommandBuffer command_buffer, VkCommandPool pool) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);

    vkFreeCommandBuffers(device_, pool, 1, &command_buffer);
}

void GPUDevice::SubmitToGraphicsQueue(const VkSubmitInfo& submit_info, VkFence fence) {
    if (vkQueueSubmit(graphics_queue_, 1, &submit_info, fence) != VK_SUCCESS) {
        LOGE("Failed to submit to graphics queue");
    }
}

void GPUDevice::SubmitToTransferQueue(const VkSubmitInfo& submit_info, VkFence fence) {
    if (vkQueueSubmit(transfer_queue_, 1, &submit_info, fence) != VK_SUCCESS) {
        LOGE("Failed to submit to transfer queue");
    }
}

void GPUDevice::WaitGraphicsQueueIdle() {
    vkQueueWaitIdle(graphics_queue_);
}

void GPUDevice::WaitTransferQueueIdle() {
    vkQueueWaitIdle(transfer_queue_);
}

BufferHandle GPUDevice::CreateBuffer(const BufferCreationInfo& info) {
    BufferHandle handle = buffer_pool_.Allocate();
    
    {
        std::unique_lock lock(creation_info_mutex_);
        buffer_creation_infos_[handle] = info;
    }

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = info.size;
    buffer_info.usage = info.usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = info.memory_usage;

    AllocatedBufferUntyped buffer;
    VkResult result = vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
        &buffer.buffer, &buffer.allocation, &buffer.info);

    if (result == VK_SUCCESS) {
        buffer.size = info.size;
        if (info.name) {
            SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer.buffer, info.name);
        }

        if (info.initial_data) {
            void* mapped_data;
            vmaMapMemory(allocator_, buffer.allocation, &mapped_data);
            memcpy(mapped_data, info.initial_data, info.size);
            vmaUnmapMemory(allocator_, buffer.allocation);
        }

        buffer_pool_.Set(handle, std::move(buffer));
    }

    return handle;
}

void GPUDevice::DestroyBuffer(BufferHandle handle) {
    auto& buffer = buffer_pool_.Get(handle);
    if (buffer.buffer && buffer.allocation) {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        buffer = {};
    }
    buffer_pool_.Free(handle);
    
    std::unique_lock lock(creation_info_mutex_);
    buffer_creation_infos_.erase(handle);
}

AllocatedBufferUntyped& GPUDevice::GetBuffer(BufferHandle handle) {
    return buffer_pool_.Get(handle);
}

TextureHandle GPUDevice::CreateTexture(const TextureCreationInfo& info) {
    if (!info.Validate()) {
        return kInvalidTextureHandle;
    }

    TextureHandle handle = texture_pool_.Allocate();
    
    {
        std::unique_lock lock(creation_info_mutex_);
        texture_creation_infos_[handle] = info;
    }

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = info.type == TextureType::Texture2D ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
    image_info.format = info.format;
    image_info.extent = info.extent;
    image_info.mipLevels = info.mip_levels;
    image_info.arrayLayers = info.array_layers;
    image_info.samples = info.samples;
    image_info.tiling = info.tiling;
    image_info.usage = info.usage;
    image_info.flags = info.flags;
    image_info.initialLayout = info.initial_layout;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    AllocatedImage image;
    VkResult result = vmaCreateImage(allocator_, &image_info, &alloc_info,
        &image.image, &image.allocation, nullptr);

    if (result == VK_SUCCESS) {
        image.format = info.format;
        image.extent = info.extent;

        if (info.name) {
            SetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image.image, info.name);
        }

        texture_pool_.Set(handle, std::move(image));
    }

    return handle;
}

void GPUDevice::DestroyTexture(TextureHandle handle) {
    auto& image = texture_pool_.Get(handle);
    if (image.image && image.allocation) {
        vmaDestroyImage(allocator_, image.image, image.allocation);
        image = {};
    }
    texture_pool_.Free(handle);
    
    std::unique_lock lock(creation_info_mutex_);
    texture_creation_infos_.erase(handle);
}

AllocatedImage& GPUDevice::GetTexture(TextureHandle handle) {
    return texture_pool_.Get(handle);
}

BufferCreationInfo GPUDevice::GetBufferCreationInfo(BufferHandle handle) const {
    std::shared_lock lock(creation_info_mutex_);
    return buffer_creation_infos_.at(handle);
}

TextureCreationInfo GPUDevice::GetTextureCreationInfo(TextureHandle handle) const {
    std::shared_lock lock(creation_info_mutex_);
    return texture_creation_infos_.at(handle);
}