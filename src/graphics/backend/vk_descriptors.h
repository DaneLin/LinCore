#pragma once
// lincore
#include "graphics/backend/vk_types.h"
#include "foundation/config.h"

namespace lincore
{
	/**
	 * @brief 描述符布局构建器
	 * 用于构建描述符布局
	 */
	struct DescriptorLayoutBuilder
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptor_count = 1);
		void Clear();
		VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shader_stages, void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
	};

	/**
	 * @brief 描述符分配器
	 * 用于分配描述符
	 */
	class DescriptorAllocatorGrowable
	{
	public:
		struct PoolSizeRatio
		{
			VkDescriptorType type;
			float ratio;
		};

		void Init(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios);
		void ClearPools(VkDevice device);
		void DestroyPools(VkDevice device);

		VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, void *ptr_next = nullptr);

	private:
		VkDescriptorPool GetPool(VkDevice device);
		VkDescriptorPool CreatePool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios);

		std::vector<PoolSizeRatio> ratios_;
		std::vector<VkDescriptorPool> full_pools_;
		std::vector<VkDescriptorPool> ready_pools_;
		uint32_t sets_per_pool_;
	};

	/**
	 * @brief 描述符写入器
	 * 用于写入描述符
	 */
	struct DescriptorWriter
	{
		std::deque<VkDescriptorImageInfo> image_infos_;
		std::deque<VkDescriptorBufferInfo> buffer_infos_;
		std::vector<VkWriteDescriptorSet> writes_;

		void WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
		void WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

		void WriteImageArray(int binding, uint32_t dst_array_element, const std::vector<VkDescriptorImageInfo> &image_info_array, VkDescriptorType type);
		void WriteBufferArray(int binding, uint32_t dst_array_element, const std::vector<VkDescriptorBufferInfo> &buffer_info_array, VkDescriptorType type);

		void Clear();
		void UpdateSet(VkDevice device, VkDescriptorSet set);
	};

	/**
	 * @brief 绑定资源更新
	 * 用于更新绑定资源
	 */
	struct BindlessUpdate
	{
		uint32_t binding;
		uint32_t array_element;
		VkDescriptorImageInfo image_info;
	};

	/**
	 * @brief 绑定资源键
	 * 用于缓存绑定资源
	 */
	struct BindlessResourceKey
	{
		uint32_t binding;
		VkImageView image_view;
		VkSampler sampler;

		bool operator==(const BindlessResourceKey &other) const
		{
			return binding == other.binding &&
				   image_view == other.image_view &&
				   sampler == other.sampler;
		}
	};

	/**
	 * @brief 绑定资源键哈希
	 * 用于在哈希表中使用BindlessResourceKey
	 */
	struct BindlessResourceKeyHash
	{
		std::size_t operator()(const BindlessResourceKey &key) const
		{
			// 简单的哈希组合方法
			std::size_t h1 = std::hash<uint32_t>{}(key.binding);
			std::size_t h2 = std::hash<VkImageView>{}(key.image_view);
			std::size_t h3 = std::hash<VkSampler>{}(key.sampler);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

	/**
	 * @brief 绑定资源更新数组
	 * 用于缓存绑定资源
	 */
	struct BindlessUpdateArray
	{
		std::vector<BindlessUpdate> updates;
		VkDescriptorSet descriptor_set{VK_NULL_HANDLE};

		/**
		 * @brief 绑定资源缓存
		 * 用于缓存绑定资源
		 */
		std::unordered_map<BindlessResourceKey, uint32_t, BindlessResourceKeyHash> resource_cache;

		void Reset()
		{
			updates.clear();
		}

		uint32_t AddTextureUpdate(VkImageView image_view, VkSampler sampler)
		{
			return AddTextureUpdate(kBINDLESS_TEXTURE_BINDING, image_view, sampler);
		}

		uint32_t AddTextureUpdate(uint32_t binding, VkImageView image_view, VkSampler sampler)
		{
			BindlessResourceKey key{binding, image_view, sampler};

			// 检查缓存中是否已存在
			auto it = resource_cache.find(key);
			if (it != resource_cache.end())
			{
				return it->second;
			}

			// 如果不存在，创建新的更新
			uint32_t array_element = static_cast<uint32_t>(updates.size());
			BindlessUpdate update;
			update.binding = binding;
			update.array_element = array_element;
			update.image_info = {
				.sampler = sampler,
				.imageView = image_view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

			updates.push_back(update);
			resource_cache[key] = array_element;
			return array_element;
		}
	};

} // namespace lincore
