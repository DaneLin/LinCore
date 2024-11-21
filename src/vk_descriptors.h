#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder {
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void add_binding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount = 1);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> _ratios;
	std::vector<VkDescriptorPool> _full_pools;
	std::vector<VkDescriptorPool> _ready_pools;
	uint32_t _sets_per_pool;
};

struct DescriptorWriter {
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,  VkDescriptorType type);

	void write_image_array(int binding, uint32_t dstArrayElement, const std::vector<VkDescriptorImageInfo>& imageInfoArray, VkDescriptorType type);
	void write_buffer_array(int binding, uint32_t dstArrayElement, const std::vector<VkDescriptorBufferInfo>& bufferInfoArray, VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};
