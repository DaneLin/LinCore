#pragma once

#include <vk_types.h>

namespace lc {
	struct DescriptorLayoutBuilder {
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptor_count = 1);
		void Clear();
		VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shader_stages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
	};

	class DescriptorAllocatorGrowable {
	public:
		struct PoolSizeRatio {
			VkDescriptorType type;
			float ratio;
		};

		void Init(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios);
		void ClearPools(VkDevice device);
		void DestroyPools(VkDevice device);

		VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, void* ptr_next = nullptr);
	private:
		VkDescriptorPool GetPool(VkDevice device);
		VkDescriptorPool CreatePool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios);

		std::vector<PoolSizeRatio> ratios_;
		std::vector<VkDescriptorPool> full_pools_;
		std::vector<VkDescriptorPool> ready_pools_;
		uint32_t sets_per_pool_;
	};

	struct DescriptorWriter {
		std::deque<VkDescriptorImageInfo> image_infos_;
		std::deque<VkDescriptorBufferInfo> buffer_infos_;
		std::vector<VkWriteDescriptorSet> writes_;

		void WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
		void WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

		void WriteImageArray(int binding, uint32_t dst_array_element, const std::vector<VkDescriptorImageInfo>& image_info_array, VkDescriptorType type);
		void WriteBufferArray(int binding, uint32_t dst_array_element, const std::vector<VkDescriptorBufferInfo>& buffer_info_array, VkDescriptorType type);

		void Clear();
		void UpdateSet(VkDevice device, VkDescriptorSet set);
	};

}

