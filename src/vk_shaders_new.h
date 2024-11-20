#pragma once

#include <vk_types.h>

#include "vk_descriptors.h"

class VulkanEngine;
namespace lc {


	struct ShaderModule {
		std::vector<uint32_t> code;
		VkShaderModule module;
	};

	namespace vkutil {
		// load a shader module from a spir-v file. Return false if it errors
		bool load_shader(VkDevice device, const char* filePath, ShaderModule* outShaderModule);

		uint32_t hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info);
	}

	// holds all information for a given shader set for pipeline
	struct ShaderEffect {
		struct ReflectionOverrides {
			const char* name;
			VkDescriptorType overridenType;
		};

		void add_stage(ShaderModule* shaderModule, VkShaderStageFlagBits stage);

		void reflect_layout(VkDevice device, ReflectionOverrides* overrides, int overrideCount, uint32_t overrideConstantSize = -1);

		void fill_stage(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages);

		// 在ShaderEffect中添加函数
		VkPipelineBindPoint get_bind_point() const;

		~ShaderEffect();

		VkPipelineLayout builtLayout;

		struct ReflectedBinding {
			uint32_t set;
			uint32_t binding;
			VkDescriptorType type;
		};

		std::unordered_map<std::string, ReflectedBinding> bindings;
		std::array<VkDescriptorSetLayout, 4> setLayouts;
		std::array<uint32_t, 4> setHashes;

	private:
		struct ShaderStage {
			ShaderModule* module;
			VkShaderStageFlagBits stage;
		};

		std::vector<ShaderStage> stages;
	};

	struct ShaderDescriptorBinder {
		struct BufferWriteDescriptor {
			int dstSet;
			int dstBinding;
			VkDescriptorType descriptorType;
			VkDescriptorBufferInfo bufferInfo;

			uint32_t dynamic_offset;
		};

		struct ImageWriteDescriptor {
			int dstSet;
			int dstBinding;
			VkDescriptorType descriptorType;
			VkDescriptorImageInfo imageInfo;
		};

		void bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo);

		void bind_image(const char* name, const VkDescriptorImageInfo& imageInfo);

		void bind_dynamic_buffer(const char* name, uint32_t offset, const VkDescriptorBufferInfo& bufferInfo);

		void apply_binds(VkCommandBuffer cmd, VkPipelineLayout layout);

		void build_sets(VkDevice device, DescriptorAllocatorGrowable& allocator);

		void set_shader(ShaderEffect* newShader);;

		std::array<VkDescriptorSet, 4> cachedDescriptorSets;


	private:
		struct DynOffset {
			std::array<uint32_t, 16> offset;
			uint32_t count{ 0 };
		};
		std::array<DynOffset, 4> setOffsets;

		ShaderEffect* shaders{ nullptr };
		std::vector<BufferWriteDescriptor> bufferWrites;
		std::vector<ImageWriteDescriptor> imageWrites;
	};

	class ShaderCache {
	public:

		ShaderEffect* get_shader_effect();

		ShaderEffect* get_shader_effect(const std::string& path, VkShaderStageFlagBits stage);

		ShaderModule* get_shader(const std::string& path);

		void clear();

	private:
		std::unordered_map<std::string, ShaderModule> module_cache;
		std::vector<ShaderEffect*> shader_effect_cache_;
	};
} // namespace lc