#include "vk_command_buffer.h"
// lincore
#include "graphics/vk_engine.h"
#include "foundation/logging.h"

namespace lincore
{
	void CommandBuffer::Init(GpuDevice* gpu_device, CommandBufferLevel level)
	{
		gpu_device_ = gpu_device;
		level_ = level;
	}

	void CommandBuffer::Shutdowon()
	{
		vk_command_buffer_ = VK_NULL_HANDLE;
		is_recording_ = false;
		level_ = CommandBufferLevel::kPrimary;
		state_ = {};
	}

	void CommandBuffer::Begin(VkCommandBufferUsageFlags flags)
	{
		if (!is_recording_)
		{
			VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
			begin_info.flags = flags;

			VK_CHECK(vkBeginCommandBuffer(vk_command_buffer_, &begin_info));
			is_recording_ = true;
		}
	}

	void CommandBuffer::BeginSecondary(const CommandBufferInheritanceInfo &inheritance_info)
	{
		if (!is_recording_ && level_ == CommandBufferLevel::kSecondary)
		{
			VkCommandBufferInheritanceRenderingInfo inheritance_rendering_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO};
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

			VkCommandBufferInheritanceInfo inheritance{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
			inheritance.pNext = &inheritance_rendering_info;
			inheritance.renderPass = VK_NULL_HANDLE;
			inheritance.subpass = 0;
			inheritance.framebuffer = VK_NULL_HANDLE;
			inheritance.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;

			VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT};
			begin_info.pInheritanceInfo = &inheritance;

			VK_CHECK(vkBeginCommandBuffer(vk_command_buffer_, &begin_info));
			is_recording_ = true;
		}
	}

	void CommandBuffer::End()
	{
		if (is_recording_)
		{
			VK_CHECK(vkEndCommandBuffer(vk_command_buffer_));
			is_recording_ = false;
		}
	}

	void CommandBuffer::Reset()
	{
		if (is_recording_)
		{
			End();
		}

		VK_CHECK(vkResetCommandBuffer(vk_command_buffer_, 0));
		is_recording_ = false;
		state_ = {};
	}

	void CommandBuffer::BeginRendering(const VkRenderingInfo &render_info)
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
			vkCmdBeginRendering(vk_command_buffer_, &render_info);
			state_.is_rendering = true;
		}
	}

	void CommandBuffer::EndRendering()
	{
		if (level_ == CommandBufferLevel::kPrimary && state_.is_rendering)
		{
			vkCmdEndRendering(vk_command_buffer_);
			state_.is_rendering = false;
		}
	}

	void CommandBuffer::BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point)
	{
		if (state_.pipeline != pipeline || state_.bind_point != bind_point)
		{
			vkCmdBindPipeline(vk_command_buffer_, bind_point, pipeline);
			state_.pipeline = pipeline;
			state_.bind_point = bind_point;
		}
	}

	void CommandBuffer::BindVertexBuffer(VkBuffer buffer, uint32_t binding, uint32_t offset)
	{
		VkDeviceSize offsets[] = {offset};
		vkCmdBindVertexBuffers(vk_command_buffer_, binding, 1, &buffer, offsets);
	}

	void CommandBuffer::BindIndexBuffer(VkBuffer buffer, uint32_t offset, VkIndexType index_type)
	{
		vkCmdBindIndexBuffer(vk_command_buffer_, buffer, offset, index_type);
	}

	void CommandBuffer::BindDescriptorSets(VkPipelineBindPoint bind_point, VkPipelineLayout layout, uint32_t first_set, uint32_t set_count, const VkDescriptorSet *sets, uint32_t dynamic_offset_count, const uint32_t *dynamic_offsets)
	{
		vkCmdBindDescriptorSets(vk_command_buffer_, bind_point, layout, first_set, set_count, sets, dynamic_offset_count, dynamic_offsets);
	}

	void CommandBuffer::Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
	{
		vkCmdDraw(vk_command_buffer_, vertex_count, instance_count, first_vertex, first_instance);
	}

	void CommandBuffer::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_intance)
	{
		vkCmdDrawIndexed(vk_command_buffer_, index_count, instance_count, first_index, vertex_offset, first_intance);
	}

	void CommandBuffer::DrawIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride)
	{
		vkCmdDrawIndirect(vk_command_buffer_, buffer, offset, 1, stride);
	}

	void CommandBuffer::DrawIndexedIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride, uint32_t count)
	{
		vkCmdDrawIndexedIndirect(vk_command_buffer_, buffer, offset, count, stride);
	}

	void CommandBuffer::ExecuteCommands(const VkCommandBuffer *secondary_cmd_bufs, uint32_t count)
	{
		if (level_ == CommandBufferLevel::kPrimary)
		{
			vkCmdExecuteCommands(vk_command_buffer_, count, secondary_cmd_bufs);
		}
	}

	void CommandBuffer::Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
	{
		vkCmdDispatch(vk_command_buffer_, group_count_x, group_count_y, group_count_z);
	}

	void CommandBuffer::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth)
	{
		VkViewport viewport{.x = x, .y = y, .width = width, .height = height, .minDepth = min_depth, .maxDepth = max_depth};
		vkCmdSetViewport(vk_command_buffer_, 0, 1, &viewport);
		state_.viewport = viewport;
	}

	void CommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		VkRect2D scissor{.offset = {x, y}, .extent = {width, height}};
		vkCmdSetScissor(vk_command_buffer_, 0, 1, &scissor);
		state_.scissor = scissor;
	}

	void CommandBuffer::Clear(float r, float g, float b, float a, uint32_t attachment_index)
	{
		clear_values_[attachment_index].color = {r, g, b, a};
	}

	void CommandBuffer::ClearDepthStencil(float depth, uint8_t stencil)
	{
		clear_values_[kDepth_Stencil_Clear_Index].depthStencil = {depth, stencil};
	}

	void CommandBuffer::PipelineBarrier2(const VkDependencyInfo &dep_info)
	{
		vkCmdPipelineBarrier2(vk_command_buffer_, &dep_info);
	}

	void CommandBuffer::CopyImageToImage(VkImage source, VkImage destination, VkExtent3D src_size, VkExtent3D dst_size)
	{
		VkImageBlit2 blit_region{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

		blit_region.srcOffsets[1].x = src_size.width;
		blit_region.srcOffsets[1].y = src_size.height;
		blit_region.srcOffsets[1].z = src_size.depth;

		blit_region.dstOffsets[1].x = dst_size.width;
		blit_region.dstOffsets[1].y = dst_size.height;
		blit_region.dstOffsets[1].z = dst_size.depth;

		blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit_region.srcSubresource.baseArrayLayer = 0;
		blit_region.srcSubresource.layerCount = 1;
		blit_region.srcSubresource.mipLevel = 0;

		blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit_region.dstSubresource.baseArrayLayer = 0;
		blit_region.dstSubresource.layerCount = 1;
		blit_region.dstSubresource.mipLevel = 0;

		VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
		blitInfo.dstImage = destination;
		blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		blitInfo.srcImage = source;
		blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		blitInfo.regionCount = 1;
		blitInfo.pRegions = &blit_region;
		blitInfo.filter = VK_FILTER_LINEAR;

		vkCmdBlitImage2(vk_command_buffer_, &blitInfo);
	}

    void CommandBuffer::CopyBufferToImage(VkBuffer buffer, VkImage image,VkImageLayout layout, VkBufferImageCopy copy_region)
    {
        vkCmdCopyBufferToImage(vk_command_buffer_, buffer, image, layout, 1, &copy_region);
    }

    void CommandBuffer::AddImageBarrier(Texture *texture, ResourceState new_state,
							 uint32_t base_mip_level, uint32_t mip_count,
							 uint32_t base_array_layer, uint32_t array_layer_count,
							 uint32_t destination_family,
							 QueueType::Enum destination_queue_type)
    {
		if (gpu_device_->enabled_features_.synchronization2_extension_present_)
		{
			VkImageMemoryBarrier2KHR barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR};
			barrier.srcAccessMask = UtilToVkAccessFlags2(texture->state);
			barrier.srcStageMask = UtilDeterminePipelineStageFlags2(barrier.srcAccessMask, texture->queue_type);
			barrier.dstAccessMask = UtilToVkAccessFlags2(new_state);
			barrier.dstStageMask = UtilDeterminePipelineStageFlags2(barrier.dstAccessMask, destination_queue_type);
			barrier.oldLayout = UtilToVkImageLayout2(texture->state);
			barrier.newLayout = UtilToVkImageLayout2(new_state);
			barrier.srcQueueFamilyIndex = texture->queue_family;
			barrier.dstQueueFamilyIndex = destination_family;
			barrier.image = texture->vk_image;
			barrier.subresourceRange.aspectMask =  TextureFormat::HasDepthOrStencil(texture->vk_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = base_array_layer;
			barrier.subresourceRange.layerCount = array_layer_count;
			barrier.subresourceRange.baseMipLevel = base_mip_level;
			barrier.subresourceRange.levelCount = mip_count;

			VkDependencyInfoKHR dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
			dependency_info.imageMemoryBarrierCount = 1;
			dependency_info.pImageMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2KHR(vk_command_buffer_, &dependency_info);
		}
		else
		{
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			barrier.image = texture->vk_image;
			barrier.srcQueueFamilyIndex = texture->queue_family;
			barrier.dstQueueFamilyIndex = destination_family;
			barrier.subresourceRange.aspectMask = TextureFormat::HasDepthOrStencil(texture->vk_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = base_array_layer;
			barrier.subresourceRange.layerCount = array_layer_count;
			barrier.subresourceRange.levelCount = mip_count;

			barrier.subresourceRange.baseMipLevel = base_mip_level;
			barrier.oldLayout = UtilToVkImageLayout(texture->state);
			barrier.newLayout = UtilToVkImageLayout(new_state);
			barrier.srcAccessMask = UtilToVkAccessFlags(texture->state);
			barrier.dstAccessMask = UtilToVkAccessFlags(new_state);

			const VkPipelineStageFlags source_stage_mask = UtilDeterminePipelineStageFlags(barrier.srcAccessMask, texture->queue_type);
			const VkPipelineStageFlags destination_stage_mask = UtilDeterminePipelineStageFlags(barrier.dstAccessMask, destination_queue_type);

			vkCmdPipelineBarrier(vk_command_buffer_, source_stage_mask, destination_stage_mask, 0,
								0, nullptr, 0, nullptr, 1, &barrier);
		}
		texture->state = new_state;
		texture->queue_type = destination_queue_type;
		texture->queue_family = destination_family;
    }

    void CommandBuffer::AddBufferBarrier(Buffer *buffer, ResourceState new_state,
							  uint32_t destination_family,
							  QueueType::Enum destination_queue_type)
    {
		if (gpu_device_->enabled_features_.synchronization2_extension_present_)
		{
			VkBufferMemoryBarrier2KHR barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR};
			barrier.srcAccessMask = UtilToVkAccessFlags2(buffer->state);
			barrier.srcStageMask = UtilDeterminePipelineStageFlags2(barrier.srcAccessMask, buffer->queue_type);
			barrier.dstAccessMask = UtilToVkAccessFlags2(new_state);
			barrier.dstStageMask = UtilDeterminePipelineStageFlags2(barrier.dstAccessMask, destination_queue_type);
			barrier.buffer = buffer->vk_buffer;
			barrier.offset = 0;
			barrier.size = buffer->size;

			VkDependencyInfoKHR dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
			dependency_info.bufferMemoryBarrierCount = 1;
			dependency_info.pBufferMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2KHR(vk_command_buffer_, &dependency_info);
		}
		else
		{
			VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
			barrier.buffer = buffer->vk_buffer;
			barrier.srcQueueFamilyIndex = buffer->queue_family;
			barrier.dstQueueFamilyIndex = destination_family;
			barrier.offset = 0;
			barrier.size = buffer->size;
			barrier.srcAccessMask = UtilToVkAccessFlags(buffer->state);
			barrier.dstAccessMask = UtilToVkAccessFlags(new_state);

			const VkPipelineStageFlags source_stage_mask = UtilDeterminePipelineStageFlags(barrier.srcAccessMask, buffer->queue_type);
			const VkPipelineStageFlags destination_stage_mask = UtilDeterminePipelineStageFlags(barrier.dstAccessMask, destination_queue_type);

			vkCmdPipelineBarrier(vk_command_buffer_, source_stage_mask, destination_stage_mask, 0,
								 0, nullptr, 1, &barrier, 0, nullptr);
		}
		buffer->state = new_state;
		buffer->queue_type = destination_queue_type;
		buffer->queue_family = destination_family;
    }

    void CommandBuffer::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size, const void *values)
	{
		vkCmdPushConstants(vk_command_buffer_, layout, stage_flags, offset, size, values);
	}

    void CommandBufferManager::Init(GpuDevice *gpu_device, uint32_t num_threads)
	{
		gpu_device_ = gpu_device;
		num_pools_per_frame = num_threads;

		const uint32_t total_pools = num_pools_per_frame * kFRAME_OVERLAP;

		command_pools_.resize(total_pools);
		used_buffers_.resize(total_pools, 0);
		used_secondary_buffers_.resize(total_pools, 0);

		VkCommandPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = gpu_device_->queue_indices_.graphics_family;

		for (uint32_t i = 0; i < total_pools; ++i)
		{
			VK_CHECK(vkCreateCommandPool(gpu_device_->device_, &pool_info, nullptr, &command_pools_[i]));
		}

		// Create primary command buffers
		const uint32_t total_primary_buffers = total_pools * MAX_COMMAND_BUFFERS_PER_THREAD;
		command_buffers_.resize(total_primary_buffers);

		for (uint32_t i = 0; i < total_primary_buffers; ++i)
		{
			VkCommandBufferAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};

			alloc_info.commandPool = command_pools_[i / MAX_COMMAND_BUFFERS_PER_THREAD];
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandBufferCount = 1;

			CommandBuffer &cmd = command_buffers_[i];
			vkAllocateCommandBuffers(gpu_device_->device_, &alloc_info, &cmd.vk_command_buffer_);
			cmd.Init(gpu_device_, CommandBufferLevel::kPrimary);

			gpu_device_->SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)cmd.vk_command_buffer_, "Primary Command Buffer" + i);
		}

		// Create secondary command buffers
		const uint32_t total_secondary_buffers = total_pools * MAX_SECONDARY_COMMAND_BUFFERS;
		secondary_command_buffers_.resize(total_secondary_buffers);

		for (uint32_t i = 0; i < total_secondary_buffers; ++i)
		{
			VkCommandBufferAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			alloc_info.commandPool = command_pools_[i / MAX_SECONDARY_COMMAND_BUFFERS];
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			alloc_info.commandBufferCount = 1;

			CommandBuffer &cmd = secondary_command_buffers_[i];
			VK_CHECK(vkAllocateCommandBuffers(gpu_device_->device_, &alloc_info, &cmd.vk_command_buffer_));
			cmd.Init(gpu_device_, CommandBufferLevel::kSecondary);
			gpu_device_->SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)cmd.vk_command_buffer_, "Secondary Command Buffer" + i);
		}

		// Initialize immediate submit resources
		VK_CHECK(vkCreateCommandPool(gpu_device_->device_, &pool_info, nullptr, &immediate_pool_));

		VkCommandBufferAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		alloc_info.commandPool = immediate_pool_;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VK_CHECK(vkAllocateCommandBuffers(gpu_device_->device_, &alloc_info, &immediate_buffer_.vk_command_buffer_));
		immediate_buffer_.Init(gpu_device_, CommandBufferLevel::kPrimary);
		gpu_device_->SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)immediate_buffer_.vk_command_buffer_, "Immediate Command Buffer");

		VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CHECK(vkCreateFence(gpu_device_->device_, &fence_info, nullptr, &immediate_fence_));

		// Initialize transfer queue resources
		VkCommandPoolCreateInfo transfer_pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		transfer_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		transfer_pool_info.queueFamilyIndex = gpu_device_->queue_indices_.transfer_family;

		VK_CHECK(vkCreateCommandPool(gpu_device_->device_, &transfer_pool_info, nullptr, &transfer_pool_));
		gpu_device_->SetDebugName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)transfer_pool_, "Transfer Command Pool");

		VkCommandBufferAllocateInfo transfer_alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		transfer_alloc_info.commandPool = transfer_pool_;
		transfer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		transfer_alloc_info.commandBufferCount = 1;

		VK_CHECK(vkAllocateCommandBuffers(gpu_device_->device_, &transfer_alloc_info, &transfer_buffer_.vk_command_buffer_));
		transfer_buffer_.Init(gpu_device_, CommandBufferLevel::kPrimary);
		gpu_device_->SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)transfer_buffer_.vk_command_buffer_, "Transfer Command Buffer");

		VK_CHECK(vkCreateFence(gpu_device_->device_, &fence_info, nullptr, &transfer_fence_));
	}

	void CommandBufferManager::Shutdown()
	{
		for (VkCommandPool &pool : command_pools_)
		{
			vkDestroyCommandPool(gpu_device_->device_, pool, nullptr);
		}

		vkDestroyCommandPool(gpu_device_->device_, immediate_pool_, nullptr);
		vkDestroyFence(gpu_device_->device_, immediate_fence_, nullptr);

		vkDestroyCommandPool(gpu_device_->device_, transfer_pool_, nullptr);
		vkDestroyFence(gpu_device_->device_, transfer_fence_, nullptr);
	}

	void CommandBufferManager::ResetPools(uint32_t frame_index)
	{
		for (uint32_t i = 0; i < num_pools_per_frame; ++i)
		{
			const uint32_t pool_index = GetPoolIndex(frame_index, i);
			VK_CHECK(vkResetCommandPool(gpu_device_->device_, command_pools_[pool_index], 0));
			used_buffers_[pool_index] = 0;
			used_secondary_buffers_[pool_index] = 0;
		}
	}

	CommandBuffer *CommandBufferManager::GetCommandBuffer(uint32_t frame, uint32_t thread_index, bool begin)
	{
		const uint32_t pool_index = GetPoolIndex(frame, thread_index);
		const uint32_t current_buffer = used_buffers_[pool_index]++;

		assert(current_buffer < MAX_COMMAND_BUFFERS_PER_THREAD);

		CommandBuffer *cmd = &command_buffers_[pool_index * MAX_COMMAND_BUFFERS_PER_THREAD + current_buffer];

		if (begin)
		{
			cmd->Reset();
			cmd->Begin();
		}

		return cmd;
	}

	CommandBuffer *CommandBufferManager::GetSecondaryCommandBuffer(uint32_t frame, uint32_t thread_index)
	{
		const uint32_t pool_index = GetPoolIndex(frame, thread_index);
		const uint32_t current_buffer = used_secondary_buffers_[pool_index]++;

		assert(current_buffer < MAX_SECONDARY_COMMAND_BUFFERS);

		CommandBuffer *cmd = &secondary_command_buffers_[pool_index * MAX_SECONDARY_COMMAND_BUFFERS + current_buffer];
		cmd->Reset();

		return cmd;
	}

	void CommandBufferManager::UploadBuffer(BufferHandle staging_buffer, BufferHandle dst_buffer, const void *data, size_t size, size_t dst_offset)
	{

		ImmediateSubmit([&](CommandBuffer *cmd)
			{
				Buffer *src = gpu_device_->GetResource<Buffer>(staging_buffer.index);
				Buffer *dst = gpu_device_->GetResource<Buffer>(dst_buffer.index);

				void *src_data = src->mapped_data;
				memcpy(src_data, data, size);
				VkBufferCopy copy_region{ 0 };
				copy_region.dstOffset = dst_offset;
				copy_region.srcOffset = 0;
				copy_region.size = size;
				vkCmdCopyBuffer(cmd->vk_command_buffer_, src->vk_buffer, dst->vk_buffer, 1, &copy_region);
			},
			gpu_device_->graphics_queue_);
	}

	void CommandBufferManager::ImmediateSubmit(std::function<void(CommandBuffer *cmd)> &&function, VkQueue queue)
	{
		// Wait for any pending immediate submits to complete
		VkFence fence = queue == gpu_device_->transfer_queue_ ? transfer_fence_ : immediate_fence_;
		CommandBuffer &cmd_buffer = queue == gpu_device_->transfer_queue_ ? transfer_buffer_ : immediate_buffer_;

		VK_CHECK(vkWaitForFences(gpu_device_->device_, 1, &fence, VK_TRUE, UINT64_MAX));
		VK_CHECK(vkResetFences(gpu_device_->device_, 1, &fence));

		cmd_buffer.Reset();
		cmd_buffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		// Execute the user's function
		function(&cmd_buffer);

		cmd_buffer.End();

		// Set up command buffer submission info
		VkCommandBufferSubmitInfo cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
		cmd_info.commandBuffer = cmd_buffer.GetVkCommandBuffer();

		// Submit the command buffer
		VkSubmitInfo2 submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
		submit_info.commandBufferInfoCount = 1;
		submit_info.pCommandBufferInfos = &cmd_info;

		// Submit to the queue and wait
		VK_CHECK(vkQueueSubmit2(queue, 1, &submit_info, fence));
		VK_CHECK(vkWaitForFences(gpu_device_->device_, 1, &fence, VK_TRUE, UINT64_MAX));
	}

	uint32_t CommandBufferManager::GetPoolIndex(uint32_t frame_index, uint32_t thread_index)
	{
		return frame_index * num_pools_per_frame + thread_index;
	}

}