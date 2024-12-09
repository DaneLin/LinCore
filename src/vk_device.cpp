#include "vk_device.h"
#include <iostream>
#include "logging.h"
#include <set>
#include <vector>
#include <algorithm>

namespace lc
{


bool GPUDevice::InitVulkanFeatures() {
    // vulkan 1.3 features
    features_13_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features_13_.dynamicRendering = true;
    features_13_.synchronization2 = true;

    // vulkan 1.2 features
    features_12_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features_12_.bufferDeviceAddress = true;
    features_12_.descriptorIndexing = true;
    features_12_.descriptorBindingPartiallyBound = true;
    features_12_.descriptorBindingSampledImageUpdateAfterBind = true;
    features_12_.descriptorBindingUniformBufferUpdateAfterBind = true;
    features_12_.runtimeDescriptorArray = true;
    features_12_.descriptorBindingVariableDescriptorCount = true;
    features_12_.hostQueryReset = true;
    features_12_.drawIndirectCount = true;

    // vulkan 1.0 features
    features_10_.pipelineStatisticsQuery = true;
    features_10_.multiDrawIndirect = true;
    features_10_.geometryShader = true;
    features_10_.inheritedQueries = true;

    return true;
}

bool GPUDevice::InitVulkanDevice(VkSurfaceKHR surface) {
    // Chain the feature structs
    features_12_.pNext = &features_13_;
    
    // Create device info
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &features_12_;
    device_info.pEnabledFeatures = &features_10_;

    // Setup queue create infos
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = { queue_indices_.graphics_family, queue_indices_.transfer_family };

    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_info.pQueueCreateInfos = queue_create_infos.data();

    // Add device extensions
    device_info.enabledExtensionCount = static_cast<uint32_t>(std::size(required_extensions));
    device_info.ppEnabledExtensionNames = required_extensions;

    // Create logical device
    if (vkCreateDevice(physical_device_, &device_info, nullptr, &device_) != VK_SUCCESS) {
        LOGE("Failed to create logical device");
        return false;
    }

    // Get queue handles
    vkGetDeviceQueue(device_, queue_indices_.graphics_family, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, queue_indices_.transfer_family, 0, &transfer_queue_);
    
    return true;
}

bool GPUDevice::InitVulkanAllocator() {
    VmaVulkanFunctions vulkan_functions = {};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device_;
    allocator_info.device = device_;
    allocator_info.instance = instance_;
    allocator_info.pVulkanFunctions = &vulkan_functions;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    if (vmaCreateAllocator(&allocator_info, &allocator_) != VK_SUCCESS) {
        LOGE("Failed to create VMA allocator");
        return false;
    }

    return true;
}

bool GPUDevice::SelectPhysicalDevice(VkSurfaceKHR surface) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    
    if (device_count == 0) {
        LOGE("Failed to find GPUs with Vulkan support!");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    LOGI("Found {} GPU(s) with Vulkan support:", device_count);

    // Score and select the best device
    int highest_score = -1;
    for (const auto& device : devices) {
        LogDeviceProperties(device);
        
        // Check if device meets minimum requirements
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        
        // Check for Vulkan 1.3 support
        if (VK_API_VERSION_MAJOR(device_properties.apiVersion) < 1 ||
            (VK_API_VERSION_MAJOR(device_properties.apiVersion) == 1 && 
             VK_API_VERSION_MINOR(device_properties.apiVersion) < 3)) {
            continue;
        }

        // Check extension support
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        bool extensions_supported = true;
        for (const char* required_extension : required_extensions) {
            bool extension_found = false;
            for (const auto& extension : available_extensions) {
                if (strcmp(required_extension, extension.extensionName) == 0) {
                    extension_found = true;
                    break;
                }
            }
            if (!extension_found) {
                extensions_supported = false;
                break;
            }
        }

        if (!extensions_supported) {
            continue;
        }

        // Score the device
        int score = 0;
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }
        score += device_properties.limits.maxImageDimension2D;

        // Select this device if it has the highest score
        if (score > highest_score) {
            physical_device_ = device;
            properties_ = device_properties;
            highest_score = score;
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        LOGE("Failed to find a suitable GPU!");
        return false;
    }

    // Get the final device properties
    vkGetPhysicalDeviceProperties(physical_device_, &properties_);
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);
    
    return true;
}

void GPUDevice::LogDeviceProperties(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    LOGI("GPU: {}", device_properties.deviceName);
    LOGI("    - Type: {}", device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated" : 
                          device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete" :
                          device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "Virtual" :
                          device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU" : "Other");
    LOGI("    - API Version: {}.{}.{}", 
         VK_VERSION_MAJOR(device_properties.apiVersion),
         VK_VERSION_MINOR(device_properties.apiVersion),
         VK_VERSION_PATCH(device_properties.apiVersion));
}

bool GPUDevice::CreateSurface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, instance_, &surface_)) {
        LOGE("Failed to create window surface!");
        return false;
    }
    return true;
}

void GPUDevice::DestroySurface() {
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

bool GPUDevice::Init(const CreateInfo& create_info) {
    if (volkInitialize() != VK_SUCCESS) {
        LOGE("Failed to initialize volk");
        return false;
    }

    instance_ = CreateInstance(create_info.app_name, create_info.engine_name, create_info.api_version);
    if (instance_ == VK_NULL_HANDLE) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }

    volkLoadInstance(instance_);
    
    if (!SetupDebugMessenger()) {
        LOGE("Failed to setup debug messenger");
        return false;
    }

    if (!CreateSurface(create_info.window)) {
        LOGE("Failed to create surface");
        return false;
    }

    if (!SelectPhysicalDevice(surface_)) {
        LOGE("Failed to select physical device");
        return false;
    }

    if (!InitVulkanFeatures()) {
        LOGE("Failed to initialize Vulkan features");
        return false;
    }

    queue_indices_ = FindQueueFamilies(physical_device_, surface_);
    if (queue_indices_.graphics_family == UINT32_MAX) {
        LOGE("Failed to find suitable queue families");
        return false;
    }

    if (!InitVulkanDevice(surface_)) {
        LOGE("Failed to create logical device");
        return false;
    }

    volkLoadDevice(device_);
    
    if (!InitVulkanAllocator()) {
        LOGE("Failed to initialize VMA allocator");
        return false;
    }

    return true;
}

void GPUDevice::Cleanup() {
    // Wait for device operations to complete
    if (device_) {
        vkDeviceWaitIdle(device_);
    }

    // Clean up command buffers
    ShutdownCommandBuffers();

    // Clean up descriptor allocator
    DestroyDescriptorAllocator();

    // Clean up resources
    DestroySurface();
   
    // Clean up VMA allocator
    if (allocator_) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
    
    // Clean up device
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    // Clean up debug messenger
    if (debug_messenger_) {
        vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
        debug_messenger_ = VK_NULL_HANDLE;
    }

    // Clean up instance
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
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

bool GPUDevice::InitCommandBuffers(uint32_t num_threads) {
    command_buffer_manager_.Init(num_threads);
    return true;
}

void GPUDevice::ShutdownCommandBuffers() {
    command_buffer_manager_.Shutdown();
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
    debug_messenger_ = vkb_inst.debug_messenger;
    return vkb_inst.instance;
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

bool GPUDevice::Submit(VkQueue queue, const QueueSubmitInfo& submit_info) {
    if (vkQueueSubmit(queue, submit_info.submit_count, submit_info.submit_info, submit_info.fence) != VK_SUCCESS) {
        LOGE("Failed to submit to queue");
        return false;
    }
    return true;
}

bool GPUDevice::Present(VkQueue queue, const QueuePresentInfo& present_info) {
    VkResult result = vkQueuePresentKHR(queue, present_info.present_info);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOGE("Failed to present swap chain image");
        return false;
    }
    return true;
}

bool GPUDevice::WaitQueueIdle(VkQueue queue) {
    if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
        LOGE("Failed to wait for queue idle");
        return false;
    }
    return true;
}

uint32_t GPUDevice::GetQueueFamilyIndex(VkQueueFlags required_flags) const {
    // Get queue family properties
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());

    // First try to find a dedicated queue for the requested flags
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags == required_flags) {
            return i;
        }
    }

    // If no dedicated queue is found, try to find a queue family that supports the requested flags
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & required_flags) == required_flags) {
            return i;
        }
    }

    return VK_QUEUE_FAMILY_IGNORED;
}

VkQueue GPUDevice::GetQueue(uint32_t family_index, uint32_t queue_index) const {
    VkQueue queue;
    vkGetDeviceQueue(device_, family_index, queue_index, &queue);
    return queue;
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

VkDescriptorSetLayout GPUDevice::CreateDescriptorSetLayout(const DescriptorLayoutInfo& layout_info) {
    lc::DescriptorLayoutBuilder builder;
    
    for (const auto& binding : layout_info.bindings) {
        builder.AddBinding(binding.binding, binding.descriptorType, binding.descriptorCount);
    }

    return builder.Build(device_, layout_info.shader_stages, layout_info.pNext, layout_info.flags);
}

void GPUDevice::DestroyDescriptorSetLayout(VkDescriptorSetLayout layout) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, layout, nullptr);
    }
}

bool GPUDevice::InitDescriptorAllocator(const DescriptorAllocatorCreateInfo& create_info) {
    descriptor_allocator_.Init(device_, create_info.max_sets, create_info.pool_sizes);
    return true;
}

void GPUDevice::DestroyDescriptorAllocator() {
    descriptor_allocator_.DestroyPools(device_);
}

VkDescriptorSet GPUDevice::AllocateDescriptorSet(VkDescriptorSetLayout layout, void* pNext) {
    return descriptor_allocator_.Allocate(device_, layout, pNext);
}

void GPUDevice::UpdateDescriptorSet(VkDescriptorSet set, const std::vector<VkWriteDescriptorSet>& writes) {
    lc::DescriptorWriter writer;
    
    for (const auto& write : writes) {
        switch (write.descriptorType) {
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                for (uint32_t i = 0; i < write.descriptorCount; i++) {
                    const auto& image_info = write.pImageInfo[i];
                    writer.WriteImage(write.dstBinding + i, 
                                    image_info.imageView,
                                    image_info.sampler,
                                    image_info.imageLayout,
                                    write.descriptorType);
                }
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                for (uint32_t i = 0; i < write.descriptorCount; i++) {
                    const auto& buffer_info = write.pBufferInfo[i];
                    writer.WriteBuffer(write.dstBinding + i,
                                     buffer_info.buffer,
                                     buffer_info.offset,
                                     buffer_info.range,
                                     write.descriptorType);
                }
                break;

            default:
                LOGE("Unsupported descriptor type in UpdateDescriptorSet");
                break;
        }
    }

    writer.UpdateSet(device_, set);
}

}