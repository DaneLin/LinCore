#include "command_buffer.h"
#include "vk_engine.h"
#include "logging.h"

void CommandBuffer::Init(CommandBufferLevel level)
{
	level_ = level;
}

void CommandBuffer::Shutdowon()
{
	command_buffer_ = VK_NULL_HANDLE;
	is_recording_ = false;
	level_ = CommandBufferLevel::kPrimary;
	state_ = {};
}

void CommandBuffer::Begin(VkCommandBufferUsageFlags flags)
{
	if (!is_recording_)
	{
		VkCommandBufferBeginInfo begin_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = flags;

		VK_CHECK(vkBeginCommandBuffer(command_buffer_, &begin_info));
		is_recording_ = true;
	}
}

void CommandBuffer::BeginSecondary(const CommandBufferInheritanceInfo& inheritance_info)
{
	if (!is_recording_ && level_ == CommandBufferLevel::kSecondary)
	{
		VkCommandBufferInheritanceRenderingInfo inheritance_rendering_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO };
		inheritance_rendering_info.rasterizationSamples = VkSampleCountFlagBits(inheritance_info.samples);
		inheritance_rendering_info.colorAttachmentCount = inheritance_info.color_attachment_count;
		inheritance_rendering_info.pColorAttachmentFormats = inheritance_info.color_formats;
		
		if (inheritance_info.enable_depth)
		{
			inheritance_rendering_info.depthAttachmentFormat = inheritance_info.depth_format;
		}
		if (inheritance_info.enable_stencil)
		{
			inheritance_rendering_info.stencilAttachmentFormat = inheritance_info.depth_format;
		}
		

		VkCommandBufferInheritanceInfo inheritance{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
		inheritance.pNext = &inheritance_rendering_info;
		inheritance.renderPass = VK_NULL_HANDLE;
		inheritance.subpass = 0;
		inheritance.framebuffer = VK_NULL_HANDLE;
		inheritance.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;

		VkCommandBufferBeginInfo begin_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT };
		begin_info.pInheritanceInfo = &inheritance;


		VK_CHECK(vkBeginCommandBuffer(command_buffer_, &begin_info));
		is_recording_ = true;
	}
}

void CommandBuffer::End()
{
	if (is_recording_)
	{
		VK_CHECK(vkEndCommandBuffer(command_buffer_));
		is_recording_ = false;
	}
}

void CommandBuffer::Reset()
{
	if (is_recording_)
	{
		End();
	}

	VK_CHECK(vkResetCommandBuffer(command_buffer_, 0));
	is_recording_ = false;
	state_ = {};
}

void CommandBuffer::BeginRendering(const VkRenderingInfo& render_info)
{
	// 检查是否已处于渲染范围内，避免重复调用
	if (state_.is_rendering)
	{
		LOGE("CommandBuffer::BeginRendering: Already in rendering scope.");
		return;
	}

	// 主要针对次级命令缓冲区的逻辑
	if (level_ == CommandBufferLevel::kSecondary)
	{
		LOGE("CommandBuffer::BeginRendering should not be called for secondary command buffers.");
		return; // 防止次级命令缓冲区中错误调用
	}

	// 确保在主命令缓冲区中正确调用 BeginRendering
	if (level_ == CommandBufferLevel::kPrimary)
	{
		vkCmdBeginRendering(command_buffer_, &render_info);
		state_.is_rendering = true;
	}
}

void CommandBuffer::EndRendering()
{
	if (level_ == CommandBufferLevel::kPrimary && state_.is_rendering)
	{
		vkCmdEndRendering(command_buffer_);
		state_.is_rendering = false;
	}
}

void CommandBuffer::BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point)
{
	if (state_.pipeline != pipeline || state_.bind_point != bind_point)
	{
		vkCmdBindPipeline(command_buffer_, bind_point, pipeline);
		state_.pipeline = pipeline;
		state_.bind_point = bind_point;
	}
}

void CommandBuffer::BindVertexBuffer(VkBuffer buffer, uint32_t binding, uint32_t offset)
{
	VkDeviceSize offsets[] = { offset };
	vkCmdBindVertexBuffers(command_buffer_, binding, 1, &buffer, offsets);
}

void CommandBuffer::BindIndexBuffer(VkBuffer buffer, uint32_t offset, VkIndexType index_type)
{
	vkCmdBindIndexBuffer(command_buffer_, buffer, offset, index_type);
}

void CommandBuffer::BindDescriptorSets(VkPipelineBindPoint bind_point, VkPipelineLayout layout, uint32_t first_set, uint32_t set_count, const VkDescriptorSet* sets, uint32_t dynamic_offset_count, const uint32_t* dynamic_offsets)
{
	vkCmdBindDescriptorSets(command_buffer_, bind_point, layout, first_set, set_count, sets, dynamic_offset_count, dynamic_offsets);
}

void CommandBuffer::Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	vkCmdDraw(command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_intance)
{
	vkCmdDrawIndexed(command_buffer_, index_count, instance_count, first_index, vertex_offset, first_intance);
}

void CommandBuffer::DrawIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride)
{
	vkCmdDrawIndirect(command_buffer_, buffer, offset, 1, stride);
}

void CommandBuffer::DrawIndexedIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride, uint32_t count)
{
	vkCmdDrawIndexedIndirect(command_buffer_, buffer, offset, count, stride);
}

void CommandBuffer::ExecuteCommands(const VkCommandBuffer* secondary_cmd_bufs, uint32_t count)
{
	if (level_ == CommandBufferLevel::kPrimary)
	{
		vkCmdExecuteCommands(command_buffer_, count, secondary_cmd_bufs);
	}
}

void CommandBuffer::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	vkCmdDispatch(command_buffer_, group_count_x, group_count_y, group_count_z);
}

void CommandBuffer::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth)
{
	VkViewport viewport{ .x = x, .y = y, .width = width, .height = height, .minDepth = min_depth, .maxDepth = max_depth };
	vkCmdSetViewport(command_buffer_, 0, 1, &viewport);
	state_.viewport = viewport;
}

void CommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	VkRect2D scissor{ .offset = {x, y}, .extent = {width, height} };
	vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
	state_.scissor = scissor;
}

void CommandBuffer::Clear(float r, float g, float b, float a, uint32_t attachment_index)
{
	clear_values_[attachment_index].color = { r,g,b,a };
}

void CommandBuffer::ClearDepthStencil(float depth, uint8_t stencil)
{
	clear_values_[kDepth_Stencil_Clear_Index].depthStencil = { depth, stencil };
}

void CommandBuffer::PipelineBarrier2(const VkDependencyInfo& dep_info)
{
	vkCmdPipelineBarrier2(command_buffer_, &dep_info);
}

void CommandBuffer::UploadTextureData(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	//TODO: replace with resource manager
	/*AllocatedImage image = VulkanEngine::Get().CreateImage(data, size, format, usage, mipmapped);
	TransitionImage(image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);*/
}

void CommandBuffer::TransitionImage(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t src_queue_family_index, uint32_t dst_queue_family_index)
{
	VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = src_queue_family_index;
	barrier.dstQueueFamilyIndex = dst_queue_family_index;
	barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// Configure access masks and pipeline stages based on layouts
	switch (old_layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		barrier.srcAccessMask = VK_ACCESS_2_NONE;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	}

	switch (new_layout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	}

	VkDependencyInfo dependency_info{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(command_buffer_, &dependency_info);
}

void CommandBuffer::CopyImageToImage(VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size)
{
	VkImageBlit2 blit_region{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

	blit_region.srcOffsets[1].x = src_size.width;
	blit_region.srcOffsets[1].y = src_size.height;
	blit_region.srcOffsets[1].z = 1;

	blit_region.dstOffsets[1].x = dst_size.width;
	blit_region.dstOffsets[1].y = dst_size.height;
	blit_region.dstOffsets[1].z = 1;

	blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.srcSubresource.baseArrayLayer = 0;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcSubresource.mipLevel = 0;

	blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.dstSubresource.baseArrayLayer = 0;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blit_region;
	blitInfo.filter = VK_FILTER_LINEAR;

	vkCmdBlitImage2(command_buffer_, &blitInfo);
}

void CommandBuffer::GenerateMipmaps(VkImage image, VkExtent2D image_size)
{
	int mip_levels = int(std::floor(std::log2(std::max(image_size.width, image_size.height)))) + 1;
	for (int mip = 0; mip < mip_levels; ++mip)
	{
		VkExtent2D half_size = image_size;
		half_size.width /= 2;
		half_size.height /= 2;

		VkImageMemoryBarrier2 image_barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr };
		image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
		image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_barrier.subresourceRange = vkinit::ImageSubresourceRange(aspectMask);
		image_barrier.subresourceRange.levelCount = 1;
		image_barrier.subresourceRange.baseMipLevel = mip;
		image_barrier.image = image;

		VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &image_barrier;

		vkCmdPipelineBarrier2(command_buffer_, &depInfo);

		if (mip < mip_levels - 1)
		{
			VkImageBlit2 blit_region{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };
			blit_region.srcOffsets[1].x = image_size.width;
			blit_region.srcOffsets[1].y = image_size.height;
			blit_region.srcOffsets[1].z = 1;

			blit_region.dstOffsets[1].x = half_size.width;
			blit_region.dstOffsets[1].y = half_size.height;
			blit_region.dstOffsets[1].z = 1;

			blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit_region.srcSubresource.baseArrayLayer = 0;
			blit_region.srcSubresource.layerCount = 1;
			blit_region.srcSubresource.mipLevel = mip;

			blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit_region.dstSubresource.baseArrayLayer = 0;
			blit_region.dstSubresource.layerCount = 1;
			blit_region.dstSubresource.mipLevel = mip + 1;

			VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
			blitInfo.dstImage = image;
			blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			blitInfo.srcImage = image;
			blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			blitInfo.filter = VK_FILTER_LINEAR;
			blitInfo.regionCount = 1;
			blitInfo.pRegions = &blit_region;

			vkCmdBlitImage2(command_buffer_ , &blitInfo);

			image_size = half_size;
		}
	}
	// transition all mip levels into the final read_only layout
	TransitionImage(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CommandBuffer::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size, const void* values)
{
	vkCmdPushConstants(command_buffer_, layout, stage_flags, offset, size, values);
}

void CommandBufferManager::Init(uint32_t num_threads)
{
	num_pools_per_frame = num_threads;

	const uint32_t total_pools = num_pools_per_frame * kFRAME_OVERLAP;

	command_pools_.resize(total_pools);
	used_buffers_.resize(total_pools,0);
	used_secondary_buffers_.resize(total_pools,0);

	VkCommandPoolCreateInfo pool_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = VulkanEngine::Get().main_queue_family_;

	for (uint32_t i = 0; i < total_pools; ++i)
	{
		VK_CHECK(vkCreateCommandPool(VulkanEngine::Get().device_, &pool_info, nullptr, &command_pools_[i]));
	}

	// Create primary command buffers
	const uint32_t total_primary_buffers = total_pools * MAX_COMMAND_BUFFERS_PER_THREAD;
	command_buffers_.resize(total_primary_buffers);

	for (uint32_t i = 0; i < total_primary_buffers; ++i)
	{
		VkCommandBufferAllocateInfo alloc_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	
		alloc_info.commandPool = command_pools_[i / MAX_COMMAND_BUFFERS_PER_THREAD];
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		CommandBuffer& cmd = command_buffers_[i];
		vkAllocateCommandBuffers(VulkanEngine::Get().device_, &alloc_info, &cmd.command_buffer_);
		cmd.Init(CommandBufferLevel::kPrimary);
	}

	// Create secondary command buffers
	const uint32_t total_secondary_buffers = total_pools * MAX_SECONDARY_COMMAND_BUFFERS;
	secondary_command_buffers_.resize(total_secondary_buffers);

	for (uint32_t i = 0; i < total_secondary_buffers; ++i)
	{
		VkCommandBufferAllocateInfo alloc_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool = command_pools_[i / MAX_SECONDARY_COMMAND_BUFFERS];
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		alloc_info.commandBufferCount = 1;

		CommandBuffer& cmd = secondary_command_buffers_[i];
		VK_CHECK(vkAllocateCommandBuffers(VulkanEngine::Get().device_, &alloc_info, &cmd.command_buffer_));
		cmd.Init(CommandBufferLevel::kSecondary);
	}

	// Initialize immediate submit resources
	VK_CHECK(vkCreateCommandPool(VulkanEngine::Get().device_, &pool_info, nullptr, &immediate_pool_));

	VkCommandBufferAllocateInfo alloc_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = immediate_pool_;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(VulkanEngine::Get().device_, &alloc_info, &immediate_buffer_.command_buffer_));
	immediate_buffer_.Init(CommandBufferLevel::kPrimary);

	VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK(vkCreateFence(VulkanEngine::Get().device_, &fence_info, nullptr, &immediate_fence_));

	// Initialize transfer queue resources
	VkCommandPoolCreateInfo transfer_pool_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	transfer_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	transfer_pool_info.queueFamilyIndex = VulkanEngine::Get().transfer_queue_family_;

	VK_CHECK(vkCreateCommandPool(VulkanEngine::Get().device_, &transfer_pool_info, nullptr, &transfer_pool_));

	VkCommandBufferAllocateInfo transfer_alloc_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	transfer_alloc_info.commandPool = transfer_pool_;
	transfer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	transfer_alloc_info.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(VulkanEngine::Get().device_, &transfer_alloc_info, &transfer_buffer_.command_buffer_));
	transfer_buffer_.Init(CommandBufferLevel::kPrimary);

	VK_CHECK(vkCreateFence(VulkanEngine::Get().device_, &fence_info, nullptr, &transfer_fence_));
}

void CommandBufferManager::Shutdown()
{
	for (VkCommandPool& pool : command_pools_)
	{
		vkDestroyCommandPool(VulkanEngine::Get().device_, pool, nullptr);
	}

	vkDestroyCommandPool(VulkanEngine::Get().device_, immediate_pool_, nullptr);
	vkDestroyFence(VulkanEngine::Get().device_, immediate_fence_, nullptr);

	vkDestroyCommandPool(VulkanEngine::Get().device_, transfer_pool_, nullptr);
	vkDestroyFence(VulkanEngine::Get().device_, transfer_fence_, nullptr);
}

void CommandBufferManager::ResetPools(uint32_t frame_index)
{
	for (uint32_t i = 0; i < num_pools_per_frame; ++i)
	{
		const uint32_t pool_index = GetPoolIndex(frame_index, i);
		VK_CHECK(vkResetCommandPool(VulkanEngine::Get().device_, command_pools_[pool_index], 0));
		used_buffers_[pool_index] = 0;
		used_secondary_buffers_[pool_index] = 0;
	}
}

CommandBuffer* CommandBufferManager::GetCommandBuffer(uint32_t frame, uint32_t thread_index, bool begin)
{
	const uint32_t pool_index = GetPoolIndex(frame, thread_index);
	const uint32_t current_buffer = used_buffers_[pool_index]++;

	assert(current_buffer < MAX_COMMAND_BUFFERS_PER_THREAD);

	CommandBuffer* cmd = &command_buffers_[pool_index * MAX_COMMAND_BUFFERS_PER_THREAD + current_buffer];

	if (begin)
	{
		cmd->Reset();
		cmd->Begin();
	}

	return cmd;
}

CommandBuffer* CommandBufferManager::GetSecondaryCommandBuffer(uint32_t frame, uint32_t thread_index)
{
	const uint32_t pool_index = GetPoolIndex(frame, thread_index);
	const uint32_t current_buffer = used_secondary_buffers_[pool_index]++;

	assert(current_buffer < MAX_SECONDARY_COMMAND_BUFFERS);

	CommandBuffer* cmd = &secondary_command_buffers_[pool_index * MAX_SECONDARY_COMMAND_BUFFERS + current_buffer];
	cmd->Reset();

	return cmd;
}

void CommandBufferManager::ImmediateSubmit(std::function<void(CommandBuffer* cmd)>&& function, VkQueue queue)
{
	// Wait for any pending immediate submits to complete
	VkFence fence = queue == VulkanEngine::Get().transfer_queue_ ? transfer_fence_ : immediate_fence_;
	CommandBuffer& cmd_buffer = queue == VulkanEngine::Get().transfer_queue_ ? transfer_buffer_ : immediate_buffer_;

	VK_CHECK(vkWaitForFences(VulkanEngine::Get().device_, 1, &fence, VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(VulkanEngine::Get().device_, 1, &fence));


	cmd_buffer.Reset();
	cmd_buffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// Execute the user's function
	function(&cmd_buffer);

	cmd_buffer.End();

	// Set up command buffer submission info
	VkCommandBufferSubmitInfo cmd_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	cmd_info.commandBuffer = cmd_buffer.GetCommandBuffer();

	// Submit the command buffer
	VkSubmitInfo2 submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_info;

	// Submit to the queue and wait
	VK_CHECK(vkQueueSubmit2(queue, 1, &submit_info, fence));
	VK_CHECK(vkWaitForFences(VulkanEngine::Get().device_, 1, &fence, VK_TRUE, UINT64_MAX));
}

uint32_t CommandBufferManager::GetPoolIndex(uint32_t frame_index, uint32_t thread_index)
{
	return frame_index * num_pools_per_frame + thread_index;
}
