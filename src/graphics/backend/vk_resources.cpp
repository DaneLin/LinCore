#include "graphics/backend/vk_resources.h"

// third party

// lincore
#include "graphics/backend/vk_device.h"

namespace lincore
{
	static void VulkanCreateTextureView(GpuDevice &gpu, const TextureViewCreation &creation, Texture *texture)
	{

		//// Create the image view
		VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		info.image = texture->vk_image;
		info.format = texture->vk_format;

		if (TextureFormat::HasDepthOrStencil(texture->vk_format))
		{

			info.subresourceRange.aspectMask = TextureFormat::HasDepth(texture->vk_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			// TODO:gs
			// info.subresourceRange.aspectMask |= TextureFormat::has_stencil( creation.format ) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
		}
		else
		{
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		info.viewType = creation.view_type;
		info.subresourceRange.baseMipLevel = creation.sub_resource.mip_base_level;
		info.subresourceRange.levelCount = creation.sub_resource.mip_level_count;
		info.subresourceRange.baseArrayLayer = creation.sub_resource.array_base_layer;
		info.subresourceRange.layerCount = creation.sub_resource.array_layer_count;
		VK_CHECK(vkCreateImageView(gpu.device_, &info, nullptr, &texture->vk_image_view));
		gpu.SetDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->vk_image_view, creation.name);
	}

	static VkImageUsageFlags VulkanGetImageUsage(const TextureCreation &creation)
	{
		const bool is_render_target = (creation.flags & TextureFlags::RenderTarget_mask) != 0;
		const bool is_compute_used = (creation.flags & TextureFlags::Compute_mask) != 0;
		const bool is_shading_rate_texture = (creation.flags & TextureFlags::ShadingRate_mask) != 0;
		const bool is_default = (creation.flags & TextureFlags::Default_mask) != 0;

		// Initialize usage flags
		VkImageUsageFlags usage = 0;

		// If it's a default texture or a render target, add SAMPLED_BIT
		// This allows both default textures and render targets to be sampled
		if (is_default || is_render_target) {
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if (TextureFormat::HasDepthOrStencil(creation.format))
		{
			// Depth/Stencil textures are normally textures you render into.
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		else
		{
			usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			if (is_render_target) {
				usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
			// 只有非深度/模板纹理才能用作存储图像
			usage |= is_compute_used ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
		}

		usage |= is_shading_rate_texture ? VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR : 0;

		return usage;
	}

	static void VulkanCreateTexture(GpuDevice &gpu, const TextureCreation &creation, TextureHandle handle, Texture *texture)
	{

		bool is_cubemap = false;
		uint32_t layer_count = creation.array_layer_count;
		if (creation.type == TextureType::TextureCube || creation.type == TextureType::Texture_Cube_Array)
		{
			is_cubemap = true;
			layer_count = 6;
		}

		const bool is_sparse_texture = (creation.flags & TextureFlags::Sparse_mask) == TextureFlags::Sparse_mask;

		texture->vk_extent = {creation.width, creation.height, creation.depth};
		texture->mip_base_level = 0;   // For new textures, we have a view that is for all mips and layers.
		texture->array_base_layer = 0; // For new textures, we have a view that is for all mips and layers.
		texture->array_layer_count = layer_count;
		texture->mip_level_count = creation.mip_level_count;
		texture->type = creation.type;
		texture->name = creation.name;
		texture->vk_format = creation.format;
		texture->vk_usage = VulkanGetImageUsage(creation);
		texture->sampler = nullptr;
		texture->flags = creation.flags;
		texture->parent_texture = k_invalid_texture;
		texture->alias_texture = k_invalid_texture;

		//// Create the image
		VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		image_info.format = texture->vk_format;
		image_info.flags = (is_cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0) | (is_sparse_texture ? (VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT) : 0);
		image_info.imageType = ToVkImageType(texture->type);
		image_info.extent.width = creation.width;
		image_info.extent.height = creation.height;
		image_info.extent.depth = creation.depth;
		image_info.mipLevels = creation.mip_level_count;
		image_info.arrayLayers = layer_count;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = texture->vk_usage;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo memory_info{};
		memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		LOGI("Creating tex %s", creation.name);

		if (creation.alias.index == k_invalid_texture.index)
		{
			if (is_sparse_texture)
			{
				VK_CHECK(vkCreateImage(gpu.device_, &image_info, nullptr, &texture->vk_image));
			}
			else
			{
				VK_CHECK(vmaCreateImage(gpu.vma_allocator_, &image_info, &memory_info,
										&texture->vk_image, &texture->vma_allocation, nullptr));

#if defined(_DEBUG)
				vmaSetAllocationName(gpu.vma_allocator_, texture->vma_allocation, creation.name);
#endif // _DEBUG
			}
		}
		else
		{
			Texture *alias_texture = gpu.GetResource<Texture>(creation.alias.index);
			assert(alias_texture != nullptr);
			assert(!is_sparse_texture);

			texture->vma_allocation = 0;
			VK_CHECK(vmaCreateAliasingImage(gpu.vma_allocator_, alias_texture->vma_allocation, &image_info, &texture->vk_image));
			texture->alias_texture = creation.alias;
		}

		gpu.SetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)texture->vk_image, creation.name);

		// Create default texture view.
		TextureViewCreation tvc;
		tvc.SetMips(0, creation.mip_level_count).SetArray(0, layer_count).SetName(creation.name).SetViewType(ToVkImageViewType(creation.type));

		VulkanCreateTextureView(gpu, tvc, texture);
		texture->state = RESOURCE_STATE_UNDEFINED;
	}

	void ResourceManager::ProcessPendingDeletions()
	{
		std::lock_guard<std::mutex> lock(deletion_queue_mutex_);

		const uint32_t current_frame = gpu_device_->current_frame_;
		const uint32_t previous_frame = gpu_device_->previous_frame_;

		// 找到第一个不是上一帧的资源的位置
		auto delete_end = resource_deletion_queue_.begin();
		while (delete_end != resource_deletion_queue_.end())
		{
			const ResourceUpdate &update = *delete_end;

			// 只删除上一帧的资源
			if (update.current_frame != previous_frame)
			{
				break;
			}

			++delete_end;
		}

		// 处理所有可以删除的资源
		for (auto it = resource_deletion_queue_.begin(); it != delete_end; ++it)
		{
			const ResourceUpdate &update = *it;
			switch (update.type)
			{
			case ResourceUpdateType::Texture:
			{
				Texture *texture = texture_pool_.Get(update.handle);
				if (texture)
				{
					if (texture->parent_texture.index == k_invalid_texture.index)
					{
						if (texture->vk_image && texture->vma_allocation)
						{
							vmaDestroyImage(gpu_device_->vma_allocator_,
											texture->vk_image, texture->vma_allocation);
							texture->vk_image = VK_NULL_HANDLE;
							texture->vma_allocation = nullptr;
						}
					}
					if (texture->vk_image_view)
					{
						vkDestroyImageView(gpu_device_->device_,
										   texture->vk_image_view, nullptr);
						texture->vk_image_view = VK_NULL_HANDLE;
					}
					texture_pool_.Release(texture);
				}
				break;
			}
			case ResourceUpdateType::Buffer:
			{
				Buffer *buffer = buffer_pool_.Get(update.handle);
				if (buffer && buffer->vk_buffer)
				{
					vmaDestroyBuffer(gpu_device_->vma_allocator_,
									 buffer->vk_buffer, buffer->vma_allocation);
					buffer->vk_buffer = VK_NULL_HANDLE;
					buffer->vma_allocation = nullptr;
					buffer_pool_.Release(buffer);
				}
				break;
			}
			case ResourceUpdateType::Sampler:
			{
				Sampler *sampler = sampler_pool_.Get(update.handle);
				if (sampler)
				{
					if (sampler->vk_sampler)
					{
						vkDestroySampler(gpu_device_->device_, sampler->vk_sampler, nullptr);
						sampler->vk_sampler = VK_NULL_HANDLE;
					}
					sampler_pool_.Release(sampler);
				}
				break;
			}
			}
		}

		// 一次性删除所有已处理的资源
		if (delete_end != resource_deletion_queue_.begin())
		{
			resource_deletion_queue_.erase(resource_deletion_queue_.begin(), delete_end);
		}
	}

	BufferHandle ResourceManager::CreateBuffer(const BufferCreation &creation)
	{
		Buffer *buffer = buffer_pool_.Obtain();
		if (!buffer)
		{
			return k_invalid_buffer;
		}

		buffer->handle.index = buffer->pool_index;
		buffer->name = creation.name;
		buffer->type_flags = creation.type_flags;
		buffer->usage = creation.usage;
		buffer->size = creation.size;
		buffer->parent_buffer = k_invalid_buffer;
		buffer->global_offset = 0;
		buffer->queue_type = creation.queue_type;
		// Cache creation info
		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			buffer_creation_infos_[buffer->handle] = creation;
		}

		CreateBufferResource(buffer->handle);
		return buffer->handle;
	}

	void ResourceManager::CreateBufferResource(BufferHandle handle)
	{
		Buffer *buffer = GetBuffer(handle);
		if (!buffer)
			return;

		const BufferCreation &creation = buffer_creation_infos_[handle];

		// Create the buffer
		VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		buffer_info.size = creation.size;
		buffer_info.usage = creation.type_flags;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_info.flags = creation.persistent ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;
		alloc_info.requiredFlags = creation.device_only
									   ? 0
									   : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		alloc_info.preferredFlags = creation.device_only
										? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
										: 0;

		// Add host access flags based on usage
		if (creation.initial_data || !creation.device_only)
		{
			alloc_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT; // Allow write access
		}

		VmaAllocationInfo allocation_info;
		VK_CHECK(vmaCreateBuffer(gpu_device_->vma_allocator_, &buffer_info, &alloc_info,
								 &buffer->vk_buffer, &buffer->vma_allocation, &allocation_info));

		buffer->vk_device_memory = allocation_info.deviceMemory;
		buffer->vk_device_size = creation.size;
		buffer->mapped_data = (void *)allocation_info.pMappedData;

		if (creation.name)
		{
			gpu_device_->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->vk_buffer, creation.name);
		}

		// Upload initial data if present
		if (creation.initial_data)
		{
			void *data;
			vmaMapMemory(gpu_device_->vma_allocator_, buffer->vma_allocation, &data);
			memcpy(data, creation.initial_data, creation.size);
			vmaUnmapMemory(gpu_device_->vma_allocator_, buffer->vma_allocation);
		}
	}

	Buffer *ResourceManager::GetBuffer(BufferHandle handle)
	{
		return buffer_pool_.Get(handle.index);
	}

	void ResourceManager::DestroyBuffer(BufferHandle handle)
	{
		Buffer *buffer = GetBuffer(handle);
		if (!buffer)
			return;

		// 将资源添加到删除队列
		{
			std::lock_guard<std::mutex> lock(deletion_queue_mutex_);
			ResourceUpdate update;
			update.type = ResourceUpdateType::Buffer;
			update.handle = handle.index;
			update.current_frame = gpu_device_->current_frame_;
			update.deleting = 1;
			resource_deletion_queue_.push_back(update);
		}

		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			buffer_creation_infos_.erase(handle);
		}
	}

	void ResourceManager::Init(GpuDevice *gpu_device)
	{
		gpu_device_ = gpu_device;

		buffer_pool_.Init(k_buffers_pool_size);
		texture_pool_.Init(k_textures_pool_size);
		pipeline_pool_.Init(k_pipelines_pool_size);
		sampler_pool_.Init(k_samplers_pool_size);
		descriptor_set_layout_pool_.Init(k_descriptor_set_layouts_pool_size);
		descriptor_set_pool_.Init(k_descriptor_sets_pool_size);
		render_pass_pool_.Init(k_render_passes_pool_size);
		framebuffer_pool_.Init(k_render_passes_pool_size);
		shader_state_pool_.Init(k_shaders_pool_size);
	}

	void ResourceManager::Shutdown()
	{
		// 将所有活跃的资源加入删除队列
		{
			std::lock_guard<std::mutex> lock(deletion_queue_mutex_);

			// 添加所有 Buffer
			for (uint32_t i = 0; i < k_buffers_pool_size; ++i)
			{
				Buffer *buffer = buffer_pool_.Get(i);
				if (buffer && buffer->vk_buffer)
				{
					ResourceUpdate update;
					update.type = ResourceUpdateType::Buffer;
					update.handle = i;
					update.current_frame = gpu_device_->previous_frame_; // 确保会在下次处理时被删除
					update.deleting = 1;
					resource_deletion_queue_.push_back(update);
				}
			}

			// 添加所有 Texture
			for (uint32_t i = 0; i < k_textures_pool_size; ++i)
			{
				Texture *texture = texture_pool_.Get(i);
				if (texture && (texture->vk_image || texture->vk_image_view))
				{
					ResourceUpdate update;
					update.type = ResourceUpdateType::Texture;
					update.handle = i;
					update.current_frame = gpu_device_->previous_frame_; // 确保会在下次处理时被删除
					update.deleting = 1;
					resource_deletion_queue_.push_back(update);
				}
			}

			// sampler
			for (uint32_t i = 0; i < k_samplers_pool_size; ++i)
			{
				Sampler *sampler = sampler_pool_.Get(i);
				if (sampler && sampler->vk_sampler)
				{
					ResourceUpdate update;
					update.type = ResourceUpdateType::Sampler;
					update.handle = i;
					update.current_frame = gpu_device_->previous_frame_; // 确保会在下次处理时被删除
					update.deleting = 1;
					resource_deletion_queue_.push_back(update);
				}
			}

			// 将已在队列中的资源也标记为上一帧
			for (auto &update : resource_deletion_queue_)
			{
				update.current_frame = gpu_device_->previous_frame_;
			}
		}

		// 处理所有资源的删除
		ProcessPendingDeletions();

		// 清理资源池
		buffer_pool_.Shutdown();
		texture_pool_.Shutdown();
		pipeline_pool_.Shutdown();
		sampler_pool_.Shutdown();
		descriptor_set_layout_pool_.Shutdown();
		descriptor_set_pool_.Shutdown();
		render_pass_pool_.Shutdown();
		framebuffer_pool_.Shutdown();
		shader_state_pool_.Shutdown();

		// 清空创建信息
		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			buffer_creation_infos_.clear();
			texture_creation_infos_.clear();
			pipeline_creation_infos_.clear();
			sampler_creation_infos_.clear();
			descriptor_set_layout_creation_infos_.clear();
			descriptor_set_creation_infos_.clear();
			render_pass_creation_infos_.clear();
			framebuffer_creation_infos_.clear();
			shader_state_creation_infos_.clear();
		}

		gpu_device_ = nullptr;
	}

	TextureHandle ResourceManager::CreateTexture(const TextureCreation &creation)
	{
		Texture *texture = texture_pool_.Obtain();
		if (!texture)
		{
			return k_invalid_texture;
		}

		texture->handle.index = texture->pool_index;
		texture->name = creation.name;
		texture->vk_extent = {creation.width, creation.height, creation.depth};
		texture->array_layer_count = creation.array_layer_count;
		texture->mip_level_count = creation.mip_level_count;
		texture->type = creation.type;
		texture->flags = creation.flags;
		texture->vk_format = creation.format;
		texture->parent_texture = k_invalid_texture;
		texture->alias_texture = creation.alias;
		texture->queue_type = creation.queue_type;
		// Cache creation info
		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			texture_creation_infos_[texture->handle] = creation;
		}

		CreateTextureResource(texture->handle);
		return texture->handle;
	}

	void ResourceManager::CreateTextureResource(TextureHandle handle)
	{
		Texture *texture = GetTexture(handle);
		if (!texture)
			return;

		const TextureCreation &creation = texture_creation_infos_[handle];

		// Create the image
		VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		image_info.imageType = ToVkImageType(texture->type);
		image_info.format = texture->vk_format;
		image_info.extent = texture->vk_extent;
		image_info.mipLevels = texture->mip_level_count;
		image_info.arrayLayers = texture->array_layer_count;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VulkanGetImageUsage(creation);
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (texture->type == TextureType::TextureCube || texture->type == TextureType::Texture_Cube_Array)
		{
			image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		if (texture->sparse)
		{
			image_info.flags |= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
		}

		// 处理别名纹理
		if (texture->alias_texture.index != k_invalid_texture.index)
		{
			Texture *alias_texture = GetTexture(texture->alias_texture);
			if (!alias_texture)
				return;

			texture->vma_allocation = 0;
			VK_CHECK(vmaCreateAliasingImage(gpu_device_->vma_allocator_,
											alias_texture->vma_allocation, &image_info, &texture->vk_image));
		}
		else
		{
			VmaAllocationCreateInfo alloc_info = {};
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_info.flags = 0;
			alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			VmaAllocationInfo allocation_info;
			VK_CHECK(vmaCreateImage(gpu_device_->vma_allocator_, &image_info, &alloc_info,
									&texture->vk_image, &texture->vma_allocation, &allocation_info));
		}

		if (creation.name)
		{
			gpu_device_->SetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)texture->vk_image, creation.name);
		}

		// Create the image view
		VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		view_info.image = texture->vk_image;
		view_info.viewType = ToVkImageViewType(texture->type);
		view_info.format = texture->vk_format;

		if (TextureFormat::HasDepthOrStencil(texture->vk_format))
		{
			view_info.subresourceRange.aspectMask = TextureFormat::HasDepth(texture->vk_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			view_info.subresourceRange.aspectMask |= TextureFormat::HasStencil(texture->vk_format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
		}
		else
		{
			view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = texture->mip_level_count;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = texture->array_layer_count;

		VK_CHECK(vkCreateImageView(gpu_device_->device_, &view_info, nullptr, &texture->vk_image_view));

		if (creation.name)
		{
			gpu_device_->SetDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->vk_image_view, creation.name);
		}

		// 设置初始状态
		texture->state = RESOURCE_STATE_UNDEFINED;

		// Upload initial data if present
		if (creation.initial_data)
		{
			// Create staging buffer
			BufferCreation staging_buffer_creation{};
			staging_buffer_creation.Reset()
				.SetName("Texture upload staging buffer")
				.SetUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ResourceUsageType::Immutable)
				.SetData(creation.initial_data, creation.initial_data_size)
				.SetPersistent();

			BufferHandle staging_buffer = CreateBuffer(staging_buffer_creation);

			VkQueue &submit_queue = creation.transfer_queue ? gpu_device_->transfer_queue_ : gpu_device_->graphics_queue_;

			// Transition image layout for transfer
			gpu_device_->command_buffer_manager_.ImmediateSubmit(
				[this, texture, staging_buffer](CommandBuffer *cmd)
				{
					// Copy staging buffer to texture
					cmd->AddImageBarrier(texture, RESOURCE_STATE_COPY_DEST, texture->mip_base_level, texture->mip_level_count, texture->array_base_layer, texture->array_layer_count);

					VkBufferImageCopy copy_region = {};
					copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copy_region.imageSubresource.mipLevel = 0;
					copy_region.imageSubresource.baseArrayLayer = 0;
					copy_region.imageSubresource.layerCount = texture->array_layer_count;
					copy_region.imageExtent = texture->vk_extent;

					cmd->CopyBufferToImage(GetBuffer(staging_buffer)->vk_buffer, texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_region);

					// Generate mipmaps if needed
					if (texture->mip_level_count > 1)
					{
						VkImageMemoryBarrier barrier{};
							barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
							barrier.image = texture->vk_image;
							barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
							barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
							barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							barrier.subresourceRange.baseArrayLayer = 0;
							barrier.subresourceRange.layerCount = texture->array_layer_count;
							barrier.subresourceRange.levelCount = texture->mip_level_count;
							barrier.subresourceRange.baseMipLevel = 0;
							barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

						vkCmdPipelineBarrier(cmd->GetVkCommandBuffer(),
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &barrier);

						int32_t mip_width = texture->vk_extent.width;
						int32_t mip_height = texture->vk_extent.height;

						for (uint32_t i = 1; i < texture->mip_level_count; i++) {
							// Transition previous mip level to TRANSFER_SRC
							barrier.subresourceRange.baseMipLevel = i - 1;
							barrier.subresourceRange.levelCount = 1;
							barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
							barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
							barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

							vkCmdPipelineBarrier(cmd->GetVkCommandBuffer(),
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier);

							// Set up current mip level as transfer destination
							barrier.subresourceRange.baseMipLevel = i;
							barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
							barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
							barrier.srcAccessMask = 0;
							barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

							vkCmdPipelineBarrier(cmd->GetVkCommandBuffer(),
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier);

							VkImageBlit blit{};
							blit.srcOffsets[0] = {0, 0, 0};
							blit.srcOffsets[1] = {mip_width, mip_height, 1};
							blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.srcSubresource.mipLevel = i - 1;
							blit.srcSubresource.baseArrayLayer = 0;
							blit.srcSubresource.layerCount = texture->array_layer_count;
							blit.dstOffsets[0] = {0, 0, 0};
							blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1,
												mip_height > 1 ? mip_height / 2 : 1,
												1};
							blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
							blit.dstSubresource.mipLevel = i;
							blit.dstSubresource.baseArrayLayer = 0;
							blit.dstSubresource.layerCount = texture->array_layer_count;

							vkCmdBlitImage(cmd->GetVkCommandBuffer(),
								texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								texture->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								1, &blit,
								VK_FILTER_LINEAR);

							// Transition previous mip level to SHADER_READ_ONLY
							barrier.subresourceRange.baseMipLevel = i - 1;
							barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
							barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
							barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

							vkCmdPipelineBarrier(cmd->GetVkCommandBuffer(),
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier);

							if (mip_width > 1) mip_width /= 2;
							if (mip_height > 1) mip_height /= 2;
						}

						// Transition last mip level to SHADER_READ_ONLY
						barrier.subresourceRange.baseMipLevel = texture->mip_level_count - 1;
						barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

						vkCmdPipelineBarrier(cmd->GetVkCommandBuffer(),
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &barrier);
					}
					else {
						cmd->AddImageBarrier(texture, RESOURCE_STATE_SHADER_RESOURCE, texture->mip_base_level, texture->mip_level_count, texture->array_base_layer, texture->array_layer_count);
					} }, submit_queue);
			// Destroy staging buffer
			DestroyBuffer(staging_buffer);
		}
	}

	TextureHandle ResourceManager::CreateTextureView(const TextureViewCreation &creation)
	{
		Texture *parent_texture = GetTexture(creation.parent_texture);
		if (!parent_texture)
		{
			return k_invalid_texture;
		}

		// 验证视图类型与纹理类型的兼容性
		if (!IsViewTypeCompatible(parent_texture->type, creation.view_type))
		{
			return k_invalid_texture;
		}

		Texture *texture = texture_pool_.Obtain();
		if (!texture)
		{
			return k_invalid_texture;
		}

		// Copy most properties from parent
		texture->handle.index = texture->pool_index;
		texture->name = creation.name;
		texture->vk_extent = parent_texture->vk_extent;
		texture->vk_format = parent_texture->vk_format;
		texture->vk_image = parent_texture->vk_image;
		texture->vk_usage = parent_texture->vk_usage;
		texture->type = parent_texture->type;
		texture->flags = parent_texture->flags;
		texture->sparse = parent_texture->sparse;
		texture->vma_allocation = parent_texture->vma_allocation;
		texture->parent_texture = creation.parent_texture;
		texture->state = parent_texture->state;

		// Set view-specific properties
		texture->mip_base_level = creation.sub_resource.mip_base_level;
		texture->mip_level_count = creation.sub_resource.mip_level_count;
		texture->array_base_layer = creation.sub_resource.array_base_layer;
		texture->array_layer_count = creation.sub_resource.array_layer_count;

		// Create image view for the texture view
		VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		view_info.image = texture->vk_image;
		view_info.viewType = creation.view_type;
		view_info.format = texture->vk_format;

		if (TextureFormat::HasDepthOrStencil(texture->vk_format))
		{
			view_info.subresourceRange.aspectMask = TextureFormat::HasDepth(texture->vk_format) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			view_info.subresourceRange.aspectMask |= TextureFormat::HasStencil(texture->vk_format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
		}
		else
		{
			view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		view_info.subresourceRange.baseMipLevel = texture->mip_base_level;
		view_info.subresourceRange.levelCount = texture->mip_level_count;
		view_info.subresourceRange.baseArrayLayer = texture->array_base_layer;
		view_info.subresourceRange.layerCount = texture->array_layer_count;

		VK_CHECK(vkCreateImageView(gpu_device_->device_, &view_info, nullptr, &texture->vk_image_view));

		if (creation.name)
		{
			gpu_device_->SetDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)texture->vk_image_view, creation.name);
		}

		return texture->handle;
	}

	void ResourceManager::UploadBuffer(BufferHandle &buffer_handle, void *buffer_data, size_t size, VkBufferUsageFlags usage, bool transfer)
	{
	}

	PipelineHandle ResourceManager::CreatePipeline(const PipelineCreation &info, const char *cache_path)
	{
		return PipelineHandle();
	}

	void ResourceManager::DestroyPipeline(PipelineHandle handle)
	{
	}

	SamplerHandle ResourceManager::CreateSampler(const SamplerCreation &creation)
	{
		Sampler *sampler = sampler_pool_.Obtain();
		if (!sampler)
		{
			return k_invalid_sampler;
		}
		sampler->handle.index = sampler->pool_index;
		sampler->name = creation.name;
		// Create the Vulkan sampler
		VkSamplerCreateInfo create_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

		// Basic sampling parameters
		create_info.magFilter = creation.mag_filter;
		create_info.minFilter = creation.min_filter;
		create_info.mipmapMode = creation.mip_filter;

		// Addressing modes
		create_info.addressModeU = creation.address_mode_u;
		create_info.addressModeV = creation.address_mode_v;
		create_info.addressModeW = creation.address_mode_w;

		// LOD parameters
		create_info.mipLodBias = 0.0f;
		create_info.minLod = 0.0f;
		create_info.maxLod = VK_LOD_CLAMP_NONE; // Allow using all available mip levels

		// Anisotropy - only enable if the feature is supported
		if (gpu_device_->features_.samplerAnisotropy)
		{
			create_info.anisotropyEnable = VK_TRUE;
			create_info.maxAnisotropy = gpu_device_->properties_.limits.maxSamplerAnisotropy;
		}
		else
		{
			create_info.anisotropyEnable = VK_FALSE;
			create_info.maxAnisotropy = 1.0f;
		}

		// Compare operation - 用于阴影贴图等
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;

		// Border color
		create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		// Unnormalized coordinates
		create_info.unnormalizedCoordinates = VK_FALSE;

		VK_CHECK(vkCreateSampler(gpu_device_->device_, &create_info, nullptr, &sampler->vk_sampler));
		if (creation.name)
		{
			gpu_device_->SetDebugName(VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler->vk_sampler, creation.name);
		}
		// Cache creation info
		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			sampler_creation_infos_[sampler->handle] = creation;
		}
		return sampler->handle;
	}

	Sampler *ResourceManager::GetSampler(SamplerHandle handle)
	{
		return sampler_pool_.Get(handle.index);
	}

	void ResourceManager::DestroySampler(SamplerHandle handle)
	{
		Sampler *sampler = sampler_pool_.Get(handle.index);
		if (!sampler)
			return;
		if (sampler->vk_sampler)
		{
			vkDestroySampler(gpu_device_->device_, sampler->vk_sampler, nullptr);
			sampler->vk_sampler = VK_NULL_HANDLE;
		}
		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			sampler_creation_infos_.erase(handle);
		}
		sampler_pool_.Release(sampler);
	}

	DescriptorSetLayoutHandle ResourceManager::CreateDescriptorSetLayout(const DescriptorSetLayoutCreation &info)
	{
		return DescriptorSetLayoutHandle();
	}

	void ResourceManager::DestroyDescriptorSetLayout(DescriptorSetLayoutHandle handle)
	{
	}

	DescriptorSetHandle ResourceManager::CreateDescriptorSet(const DescriptorSetCreation &info)
	{
		return DescriptorSetHandle();
	}

	void ResourceManager::DestroyDescriptorSet(DescriptorSetHandle handle)
	{
	}

	RenderPassHandle ResourceManager::CreateRenderPass(const RenderPassCreation &info)
	{
		return RenderPassHandle();
	}

	void ResourceManager::DestroyRenderPass(RenderPassHandle handle)
	{
	}

	FramebufferHandle ResourceManager::CreateFramebuffer(const FramebufferCreation &info)
	{
		return FramebufferHandle();
	}

	void ResourceManager::DestroyFramebuffer(FramebufferHandle handle)
	{
	}

	ShaderStateHandle ResourceManager::CreateShaderState(const ShaderStateCreation &info)
	{
		return ShaderStateHandle();
	}

	void ResourceManager::DestroyShaderState(ShaderStateHandle handle)
	{
	}

	Texture *ResourceManager::GetTexture(TextureHandle handle)
	{
		return texture_pool_.Get(handle.index);
	}

	const Texture *ResourceManager::GetTexture(TextureHandle handle) const
	{
		return texture_pool_.Get(handle.index);
	}

	void ResourceManager::DestroyTexture(TextureHandle handle)
	{
		Texture *texture = GetTexture(handle);
		if (!texture)
			return;

		// 将资源添加到删除队列
		{
			std::lock_guard<std::mutex> lock(deletion_queue_mutex_);
			ResourceUpdate update;
			update.type = ResourceUpdateType::Texture;
			update.handle = handle.index;
			update.current_frame = gpu_device_->current_frame_;
			update.deleting = 1;
			resource_deletion_queue_.push_back(update);
		}

		{
			std::unique_lock<std::shared_mutex> lock(creation_info_mutex_);
			texture_creation_infos_.erase(handle);
		}
	}
} // namespace lincore