#include "graphics/vk_descriptors.h"
// lincore
#include "graphics/vk_loader.h"
#include "fundation/logging.h"

namespace lincore
{
	void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, uint32_t descriptor_count)
	{
		VkDescriptorSetLayoutBinding bind{};
		bind.binding = binding;
		bind.descriptorCount = descriptor_count;
		bind.descriptorType = type;

		bindings.push_back(bind);
	}

	void DescriptorLayoutBuilder::Clear()
	{
		bindings.clear();
	}

	VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device, VkShaderStageFlags shader_stages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
	{

		bool is_bindless = (flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
		std::vector<VkDescriptorBindingFlags> bindFlags;
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};

		if (is_bindless)
		{
			bindFlags.resize(bindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
			// 最后一个binding设置为variable descriptor count
			if (!bindings.empty())
			{
				// 修改:确保最后一个 binding(纹理数组)有正确的描述符数量
				for (auto& bind : bindings)
				{
					if (bind.binding == kBINDLESS_TEXTURE_BINDING)
					{
						bind.descriptorCount = kMAX_BINDLESS_RESOURCES;
					}
				}
				bindFlags.back() |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
					VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
			}

			bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			bindingFlags.bindingCount = static_cast<uint32_t>(bindings.size());
			bindingFlags.pBindingFlags = bindFlags.data();
			bindingFlags.pNext = pNext;
		}

		for (auto& bind : bindings)
		{
			bind.stageFlags |= shader_stages;
		}

		VkDescriptorSetLayoutCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = is_bindless ? &bindingFlags : pNext,
			.flags = flags,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data() };

		VkDescriptorSetLayout set;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

		return set;
	}

	void DescriptorAllocatorGrowable::Init(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios)
	{
		ratios_.clear();

		for (auto r : pool_ratios)
		{
			ratios_.push_back(r);
		}

		VkDescriptorPool new_pool = CreatePool(device, initial_sets, pool_ratios);

		sets_per_pool_ = static_cast<uint32_t>(initial_sets * 1.5); // grow it next allocation

		ready_pools_.push_back(new_pool);
	}

	void DescriptorAllocatorGrowable::ClearPools(VkDevice device)
	{
		for (auto p : ready_pools_)
		{
			vkResetDescriptorPool(device, p, 0);
		}

		for (auto p : full_pools_)
		{
			vkResetDescriptorPool(device, p, 0);
			ready_pools_.push_back(p);
		}

		full_pools_.clear();
	}

	void DescriptorAllocatorGrowable::DestroyPools(VkDevice device)
	{
		for (auto p : ready_pools_)
		{
			vkDestroyDescriptorPool(device, p, nullptr);
		}
		for (auto p : full_pools_)
		{
			vkDestroyDescriptorPool(device, p, nullptr);
		}
		ready_pools_.clear();
		full_pools_.clear();
	}

	VkDescriptorSet DescriptorAllocatorGrowable::Allocate(VkDevice device, VkDescriptorSetLayout layout, void* ptr_next)
	{
		VkDescriptorPool pool_to_use = GetPool(device);

		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.pNext = ptr_next;
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = pool_to_use;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &layout;

		VkDescriptorSet ds;
		VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &ds);

		// allocation failed, try again
		if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
		{
			full_pools_.push_back(pool_to_use);
			pool_to_use = GetPool(device);
			alloc_info.descriptorPool = pool_to_use;

			VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &ds));
		}

		ready_pools_.push_back(pool_to_use);

		return ds;
	}

	VkDescriptorPool DescriptorAllocatorGrowable::GetPool(VkDevice device)
	{
		VkDescriptorPool new_pool;
		if (ready_pools_.size() != 0)
		{
			new_pool = ready_pools_.back();
			ready_pools_.pop_back();
		}
		else
		{
			new_pool = CreatePool(device, sets_per_pool_, ratios_);

			sets_per_pool_ = static_cast<uint32_t>(sets_per_pool_ * 1.5);
			if (sets_per_pool_ > 4092)
			{
				sets_per_pool_ = 4092;
			}
		}
		return new_pool;
	}
	VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios)
	{
		std::vector<VkDescriptorPoolSize> pool_sizes;
		for (PoolSizeRatio ratio : pool_ratios)
		{
			pool_sizes.push_back(VkDescriptorPoolSize{
				.type = ratio.type,
				.descriptorCount = static_cast<uint32_t>(ratio.ratio * set_count) });
		}

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		// Add flag for bindless support
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		pool_info.maxSets = set_count;
		pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
		pool_info.pPoolSizes = pool_sizes.data();

		VkDescriptorPool new_pool;
		vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool);
		return new_pool;
	}

	void DescriptorWriter::WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
	{
		VkDescriptorImageInfo& info = image_infos_.emplace_back(VkDescriptorImageInfo{
			.sampler = sampler,
			.imageView = image,
			.imageLayout = layout });

		VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

		write.dstBinding = binding;
		write.dstSet = VK_NULL_HANDLE;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pImageInfo = &info;

		writes_.push_back(write);
	}

	void DescriptorWriter::WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
	{
		VkDescriptorBufferInfo& info = buffer_infos_.emplace_back(VkDescriptorBufferInfo{
			.buffer = buffer,
			.offset = offset,
			.range = size });

		VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

		write.dstBinding = binding;
		write.dstSet = VK_NULL_HANDLE;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pBufferInfo = &info;

		writes_.push_back(write);
	}

	void DescriptorWriter::WriteImageArray(int binding, uint32_t dst_array_element, const std::vector<VkDescriptorImageInfo>& image_info_array, VkDescriptorType type)
	{
		size_t start_idx = image_infos_.size();
		for (const auto& info : image_info_array)
		{
			image_infos_.push_back(info);
		}

		VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstBinding = binding;
		write.dstArrayElement = dst_array_element;
		write.dstSet = VK_NULL_HANDLE;
		write.descriptorCount = static_cast<uint32_t>(image_info_array.size());
		write.descriptorType = type;
		write.pImageInfo = &image_infos_[start_idx];

		writes_.push_back(write);
	}

	void DescriptorWriter::WriteBufferArray(int binding, uint32_t dst_array_element, const std::vector<VkDescriptorBufferInfo>& bufferInfoArray, VkDescriptorType type)
	{
		size_t start_idx = buffer_infos_.size();
		for (const auto& info : bufferInfoArray)
		{
			buffer_infos_.push_back(info);
		}

		VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstBinding = binding;
		write.dstArrayElement = dst_array_element;
		write.dstSet = VK_NULL_HANDLE;
		write.descriptorCount = static_cast<uint32_t>(bufferInfoArray.size());
		write.descriptorType = type;
		write.pBufferInfo = &buffer_infos_[start_idx];

		writes_.push_back(write);
	}

	void DescriptorWriter::Clear()
	{
		image_infos_.clear();
		buffer_infos_.clear();
		writes_.clear();
	}

	void DescriptorWriter::UpdateSet(VkDevice device, VkDescriptorSet set)
	{
		for (auto& write : writes_)
		{
			write.dstSet = set;
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes_.size()), writes_.data(), 0, nullptr);
	}
} // namespace lincore
