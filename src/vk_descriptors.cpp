#include <vk_descriptors.h>
#include <config.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, uint32_t descriptorCount)
{
	VkDescriptorSetLayoutBinding newbind{};
	newbind.binding = binding;
	newbind.descriptorCount = descriptorCount;
	newbind.descriptorType = type;

	bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{

	bool is_bindless = (flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
	std::vector<VkDescriptorBindingFlags> bindFlags;
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};

	if (is_bindless) {
		bindFlags.resize(bindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
		// 最后一个binding设置为variable descriptor count
		if (!bindings.empty()) {
			// 修改:确保最后一个 binding(纹理数组)有正确的描述符数量
			for (auto& bind : bindings) {
				if (bind.binding == BINDLESS_TEXTURE_BINDING) {
					bind.descriptorCount = MAX_BINDLESS_RESOURCES;
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

	for (auto& bind : bindings) {
		bind.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info{
		 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		 .pNext = is_bindless ? &bindingFlags : pNext,
		 .flags = flags,
		 .bindingCount = static_cast<uint32_t>(bindings.size()),
		 .pBindings = bindings.data()
	};

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
{
	_ratios.clear();

	for (auto r : poolRatios) {
		_ratios.push_back(r);
	}

	VkDescriptorPool newPool = create_pool(device, initialSets, poolRatios);

	_sets_per_pool = static_cast<uint32_t>(initialSets * 1.5); // grow it next allocation

	_ready_pools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
	for (auto p : _ready_pools) {
		vkResetDescriptorPool(device, p, 0);
	}

	for (auto p : _full_pools) {
		vkResetDescriptorPool(device, p, 0);
		_ready_pools.push_back(p);
	}

	_full_pools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
	for (auto p : _ready_pools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	for (auto p : _full_pools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	_ready_pools.clear();
	_full_pools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
	VkDescriptorPool poolToUse = get_pool(device);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

	// allocation failed, try again
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
		_full_pools.push_back(poolToUse);
		poolToUse = get_pool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	_ready_pools.push_back(poolToUse);

	return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
	VkDescriptorPool newPool;
	if (_ready_pools.size() != 0) {
		newPool = _ready_pools.back();
		_ready_pools.pop_back();
	}
	else {
		newPool = create_pool(device, _sets_per_pool, _ratios);

		_sets_per_pool = static_cast<uint32_t>(_sets_per_pool * 1.5);
		if (_sets_per_pool > 4092) {
			_sets_per_pool = 4092;
		}
	}
	return newPool;
}
VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount)
			});
	}

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	// Add flag for bindless support
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = setCount;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
	return newPool;
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
		});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
		});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_image_array(int binding, uint32_t dstArrayElement, const std::vector<VkDescriptorImageInfo>& imageInfoArray, VkDescriptorType type)
{
	size_t startIdx = imageInfos.size();
	for (const auto& info : imageInfoArray) {
		imageInfos.push_back(info);
	}

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = binding;
	write.dstArrayElement = dstArrayElement;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = static_cast<uint32_t>(imageInfoArray.size());
	write.descriptorType = type;
	write.pImageInfo = &imageInfos[startIdx];

	writes.push_back(write);
}

void DescriptorWriter::write_buffer_array(int binding, uint32_t dstArrayElement, const std::vector<VkDescriptorBufferInfo>& bufferInfoArray, VkDescriptorType type)
{
	size_t startIdx = bufferInfos.size();
	for (const auto& info : bufferInfoArray) {
		bufferInfos.push_back(info);
	}

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = binding;
	write.dstArrayElement = dstArrayElement;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = static_cast<uint32_t>(bufferInfoArray.size());
	write.descriptorType = type;
	write.pBufferInfo = &bufferInfos[startIdx];

	writes.push_back(write);
}

void DescriptorWriter::clear()
{
	imageInfos.clear();
	bufferInfos.clear();
	writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
	for (auto& write : writes) {
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
