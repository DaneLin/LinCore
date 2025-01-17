#pragma once
// lincore
#include "graphics/backend/vk_types.h"

namespace vkinit
{
	//> init_cmd
	VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queue_family_index, VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1);
	//< init_cmd

	VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

	VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);

	VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

	VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signal_semaphore_info, VkSemaphoreSubmitInfo* wait_semaphore_info);

	VkPresentInfoKHR PresentInfo();

	VkRenderingAttachmentInfo AttachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

	VkRenderingAttachmentInfo DepthAttachmentInfo(VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

	VkRenderingInfo RenderingInfo(VkExtent2D render_extent, VkRenderingAttachmentInfo* color_attachment, VkRenderingAttachmentInfo* depth_attachment);

	VkRenderingInfo RenderingInfo(VkExtent2D render_extent, std::vector<VkRenderingAttachmentInfo>& color_attachments, VkRenderingAttachmentInfo* depth_attachment);

	VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspect_mask);

	VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore);

	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stage_flags, uint32_t binding);

	VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding* bindings, uint32_t binding_count);

	VkWriteDescriptorSet WriteDescriptorImage(VkDescriptorType type, VkDescriptorSet dst_set, VkDescriptorImageInfo* image_info, uint32_t binding);

	VkWriteDescriptorSet WriteDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dst_set, VkDescriptorBufferInfo* buffer_info, uint32_t binding);

	VkDescriptorBufferInfo BufferInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

	VkImageCreateInfo ImageCreateInfo(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);

	VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);

	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();

	VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shader_module, const char* entry = "main");

	VkSamplerCreateInfo SamplerCreateInfo(VkFilter magFilter = VK_FILTER_LINEAR, VkFilter minFilter = VK_FILTER_LINEAR);
} // namespace vkinit
