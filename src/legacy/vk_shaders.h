#pragma once

#include "volk.h"
#include "vk_types.h"
#include "vk_engine.h"

struct Shader {
	VkShaderModule module;
	VkShaderStageFlagBits stage;

	VkDescriptorType resourceTypes[32];
	uint32_t resourceMask;

	uint32_t localSizeX;
	uint32_t localSizeY;
	uint32_t localSizeZ;

	bool usesPushConstants;
	bool usesDescriptorArray;

	~Shader()
	{
		vkDestroyShaderModule(VulkanEngine::Get()._device, module, nullptr);
	}
};

bool load_shader(Shader& shader, VkDevice device, const char* base, const char* path);

using Shaders = std::initializer_list<const Shader*>;
using Constants = std::initializer_list<int>;

VkPipeline create_graphics_pipeline(VkDevice device, VkPipelineCache pipelineCache, const VkPipelineRenderingCreateInfo& renderingInfo, Shaders shaders, VkPipelineLayout layout, Constants constants = {});
VkPipeline create_compute_pipeline(VkDevice device, VkPipelineCache pipelineCache, const Shader& shader, VkPipelineLayout layout, Constants constants = {});

Program create_program(VkDevice device, VkPipelineBindPoint bindPoint, Shaders shaders, size_t pushConstantSize, VkDescriptorSetLayout arrayLayout = nullptr);
std::pair<VkDescriptorPool, VkDescriptorSet> create_descriptor_array(VkDevice device, VkDescriptorSetLayout layout, uint32_t descriptorCount);

inline uint32_t get_group_count(uint32_t threadCount, uint32_t localSize) {
	return (threadCount + localSize - 1) / localSize;
}

struct DescriptorInfo {
	union {
		VkDescriptorImageInfo image;
		VkDescriptorBufferInfo buffer;
		VkAccelerationStructureKHR accelerationStructure;
	};

	DescriptorInfo() {

	}

	DescriptorInfo(VkAccelerationStructureKHR structure) {
		accelerationStructure = structure;
	}

	DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout) {
		image.sampler = VK_NULL_HANDLE;
		image.imageView = imageView;
		image.imageLayout = imageLayout;
	}

	DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
	{
		image.sampler = sampler;
		image.imageView = imageView;
		image.imageLayout = imageLayout;
	}

	DescriptorInfo(VkBuffer buffer_, VkDeviceSize offset, VkDeviceSize range)
	{
		buffer.buffer = buffer_;
		buffer.offset = offset;
		buffer.range = range;
	}

	DescriptorInfo(VkBuffer buffer_)
	{
		buffer.buffer = buffer_;
		buffer.offset = 0;
		buffer.range = VK_WHOLE_SIZE;
	}
};
