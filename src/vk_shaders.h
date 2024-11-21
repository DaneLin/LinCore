#pragma once

#include <vk_types.h>
#include "vk_descriptors.h"

class VulkanEngine;

namespace lc {

    // Represents a compiled shader module with its SPIR-V code.
    struct ShaderModule {
        std::vector<uint32_t> code;
        VkShaderModule module;
    };

    namespace vkutil {
        // Loads a shader module from a SPIR-V file.
        // Returns false if loading fails.
        bool LoadShader(VkDevice device, const char* file_path,
            ShaderModule* out_shader_module);

        // Computes hash for descriptor layout information.
        uint32_t HashDescriptorLayoutInfo(VkDescriptorSetLayoutCreateInfo* info);
    }  // namespace vkutil

    // Contains all information needed for a shader effect pipeline.
    class ShaderEffect {
    public:
        // Defines type overrides for shader reflection.
        struct ReflectionOverrides {
            const char* name;
            VkDescriptorType overriden_type;
        };

        // Stores information about reflected bindings.
        struct ReflectedBinding {
            uint32_t set;
            uint32_t binding;
            VkDescriptorType type;
        };

        ~ShaderEffect();

        void AddStage(ShaderModule* shader_module, VkShaderStageFlagBits stage);
        void ReflectLayout(VkDevice device, ReflectionOverrides* overrides,
            int override_count, uint32_t override_constant_size = -1);
        void FillStage(std::vector<VkPipelineShaderStageCreateInfo>& pipeline_stages);
        VkPipelineBindPoint GetBindPoint() const;

        VkPipelineLayout built_layout_;
        std::unordered_map<std::string, ReflectedBinding> bindings_;
        std::array<VkDescriptorSetLayout, 4> set_layouts_;
        std::array<uint32_t, 4> set_hashes_;

    private:
        struct ShaderStage {
            ShaderModule* module;
            VkShaderStageFlagBits stage;
        };

        std::vector<ShaderStage> stages_;
    };

    class ShaderDescriptorBinder {
    public:
        struct BufferWriteDescriptor {
            int dst_set;
            int dst_binding;
            VkDescriptorType descriptor_type;
            VkDescriptorBufferInfo BufferInfo;
            uint32_t dynamic_offset;
        };

        struct ImageWriteDescriptor {
            int dst_set;
            int dst_binding;
            VkDescriptorType descriptor_type;
            VkDescriptorImageInfo image_info;
        };

        void BindBuffer(const char* name,
            const VkDescriptorBufferInfo& BufferInfo);
        void BindImage(const char* name,
            const VkDescriptorImageInfo& image_info);
        void BindDynamicBuffer(const char* name, uint32_t offset,
            const VkDescriptorBufferInfo& BufferInfo);
        void ApplyBinds(VkCommandBuffer cmd, VkPipelineLayout layout);
        void BuildSets(VkDevice device, DescriptorAllocatorGrowable& allocator);
        void SetShader(ShaderEffect* new_shader);

        std::array<VkDescriptorSet, 4> cached_descriptor_sets_;

    private:
        struct DynOffset {
            std::array<uint32_t, 16> offset;
            uint32_t count{ 0 };
        };

        std::array<DynOffset, 4> set_offsets_;
        ShaderEffect* shaders_{ nullptr };
        std::vector<BufferWriteDescriptor> buffer_writes_;
        std::vector<ImageWriteDescriptor> image_writes_;
    };

    class ShaderCache {
    public:
        ShaderEffect* GetShaderEffect();
        ShaderEffect* GetShaderEffect(const std::string& path,
            VkShaderStageFlagBits stage);
        ShaderModule* GetShader(const std::string& path);
        void Clear();

    private:
        std::unordered_map<std::string, ShaderModule> module_cache_;
        std::vector<ShaderEffect*> shader_effect_cache_;
    };

}  // namespace lc