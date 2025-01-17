#include "graphics/backend/vk_initializers.h"

//> init_cmd
VkCommandPoolCreateInfo vkinit::CommandPoolCreateInfo(uint32_t queue_family_index,
	VkCommandPoolCreateFlags flags /*= 0*/)
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queueFamilyIndex = queue_family_index;
	info.flags = flags;
	return info;
}

VkCommandBufferAllocateInfo vkinit::CommandBufferAllocateInfo(
	VkCommandPool pool, uint32_t count /*= 1*/)
{
	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;

	info.commandPool = pool;
	info.commandBufferCount = count;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	return info;
}
//< init_cmd
//
//> init_cmd_draw
VkCommandBufferBeginInfo vkinit::CommandBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/)
{
	VkCommandBufferBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;

	info.pInheritanceInfo = nullptr;
	info.flags = flags;
	return info;
}
//< init_cmd_draw

//> init_sync
VkFenceCreateInfo vkinit::FenceCreateInfo(VkFenceCreateFlags flags /*= 0*/)
{
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.pNext = nullptr;

	info.flags = flags;

	return info;
}

VkSemaphoreCreateInfo vkinit::SemaphoreCreateInfo(VkSemaphoreCreateFlags flags /*= 0*/)
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = flags;
	return info;
}
//< init_sync

//> init_submit
VkSemaphoreSubmitInfo vkinit::SemaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.semaphore = semaphore;
	submit_info.stageMask = stage_mask;
	submit_info.deviceIndex = 0;
	submit_info.value = 1;

	return submit_info;
}

VkCommandBufferSubmitInfo vkinit::CommandBufferSubmitInfo(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;

	return info;
}

VkSubmitInfo2 vkinit::SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signal_semaphore_info,
	VkSemaphoreSubmitInfo* wait_semaphore_info)
{
	VkSubmitInfo2 info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;

	info.waitSemaphoreInfoCount = wait_semaphore_info == nullptr ? 0 : 1;
	info.pWaitSemaphoreInfos = wait_semaphore_info;

	info.signalSemaphoreInfoCount = signal_semaphore_info == nullptr ? 0 : 1;
	info.pSignalSemaphoreInfos = signal_semaphore_info;

	info.commandBufferInfoCount = 1;
	info.pCommandBufferInfos = cmd;

	return info;
}
//< init_submit

VkPresentInfoKHR vkinit::PresentInfo()
{
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pNext = 0;

	info.swapchainCount = 0;
	info.pSwapchains = nullptr;
	info.pWaitSemaphores = nullptr;
	info.waitSemaphoreCount = 0;
	info.pImageIndices = nullptr;

	return info;
}

//> color_info
VkRenderingAttachmentInfo vkinit::AttachmentInfo(
	VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
	VkRenderingAttachmentInfo color_attachment{};
	color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_attachment.pNext = nullptr;

	color_attachment.imageView = view;
	color_attachment.imageLayout = layout;
	color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
	color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.resolveImageView = VK_NULL_HANDLE;
	color_attachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if (clear)
	{
		color_attachment.clearValue = *clear;
	}

	return color_attachment;
}
//< color_info
//> depth_info
VkRenderingAttachmentInfo vkinit::DepthAttachmentInfo(
	VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
	VkRenderingAttachmentInfo depth_attachment{};
	depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_attachment.pNext = nullptr;

	depth_attachment.imageView = view;
	depth_attachment.imageLayout = layout;
	depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
	depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.resolveImageView = VK_NULL_HANDLE;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.clearValue.depthStencil.depth = 0.f;

	return depth_attachment;
}
//< depth_info
//> render_info
VkRenderingInfo vkinit::RenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* color_attachment,
	VkRenderingAttachmentInfo* depth_attachment)
{
	VkRenderingInfo render_info{};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.pNext = nullptr;

	render_info.renderArea = VkRect2D{ VkOffset2D{0, 0}, renderExtent };
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = 1;
	render_info.pColorAttachments = color_attachment;
	render_info.pDepthAttachment = depth_attachment;
	render_info.pStencilAttachment = nullptr;

	return render_info;
}

VkRenderingInfo vkinit::RenderingInfo(VkExtent2D render_extent, std::vector<VkRenderingAttachmentInfo>& color_attachments, VkRenderingAttachmentInfo * depth_attachment)
{
	VkRenderingInfo render_info{};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.pNext = nullptr;

	render_info.renderArea = VkRect2D{ VkOffset2D{0, 0}, render_extent };
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
	render_info.pColorAttachments = color_attachments.data();
	render_info.pDepthAttachment = depth_attachment;
	return render_info;
}
//< render_info
//> subresource
VkImageSubresourceRange vkinit::ImageSubresourceRange(VkImageAspectFlags aspect_mask)
{
	VkImageSubresourceRange image_sub_range{};
	image_sub_range.aspectMask = aspect_mask;
	image_sub_range.baseMipLevel = 0;
	image_sub_range.levelCount = VK_REMAINING_MIP_LEVELS;
	image_sub_range.baseArrayLayer = 0;
	image_sub_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return image_sub_range;
}
//< subresource

VkDescriptorSetLayoutBinding vkinit::DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stage_flags,
	uint32_t binding)
{
	VkDescriptorSetLayoutBinding setbind = {};
	setbind.binding = binding;
	setbind.descriptorCount = 1;
	setbind.descriptorType = type;
	setbind.pImmutableSamplers = nullptr;
	setbind.stageFlags = stage_flags;

	return setbind;
}

VkDescriptorSetLayoutCreateInfo vkinit::DescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding* bindings,
	uint32_t binding_count)
{
	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;

	info.pBindings = bindings;
	info.bindingCount = binding_count;
	info.flags = 0;

	return info;
}

VkWriteDescriptorSet vkinit::WriteDescriptorImage(VkDescriptorType type, VkDescriptorSet dst_set,
	VkDescriptorImageInfo* image_info, uint32_t binding)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding = binding;
	write.dstSet = dst_set;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = image_info;

	return write;
}

VkWriteDescriptorSet vkinit::WriteDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dst_set,
	VkDescriptorBufferInfo* buffer_info, uint32_t binding)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding = binding;
	write.dstSet = dst_set;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = buffer_info;

	return write;
}

VkDescriptorBufferInfo vkinit::BufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	VkDescriptorBufferInfo buf_info{};
	buf_info.buffer = buffer;
	buf_info.offset = offset;
	buf_info.range = range;
	return buf_info;
}

//> image_set
VkImageCreateInfo vkinit::ImageCreateInfo(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent)
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = VK_IMAGE_TYPE_2D;

	info.format = format;
	info.extent = extent;

	info.mipLevels = 1;
	info.arrayLayers = 1;

	// for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
	info.samples = VK_SAMPLE_COUNT_1_BIT;

	// optimal tiling, which means the image is stored on the best gpu format
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage_flags;

	return info;
}

VkImageViewCreateInfo vkinit::ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags)
{
	// build a image-view for the depth image to use for rendering
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;

	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspect_flags;

	return info;
}
//< image_set
VkPipelineLayoutCreateInfo vkinit::PipelineLayoutCreateInfo()
{
	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;

	// empty defaults
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;
	return info;
}

VkPipelineShaderStageCreateInfo vkinit::PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
	VkShaderModule shader_module,
	const char* entry)
{
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;

	// shader stage
	info.stage = stage;
	// module containing the code for this shader stage
	info.module = shader_module;
	// the entry point of the shader
	info.pName = entry;
	return info;
}
VkSamplerCreateInfo vkinit::SamplerCreateInfo(VkFilter magFilter, VkFilter minFilter)
{
	VkSamplerCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.magFilter = magFilter;
	info.minFilter = minFilter;
	return info;
}