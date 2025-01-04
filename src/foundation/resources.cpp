#include "resources.h"
#include "graphics/vk_device.h"

namespace lincore
{

	// 添加一个辅助函数来获取对应阶段的访问掩码
	VkAccessFlags GetAccessMaskForStage(PipelineStage::Enum stage)
	{
		switch (stage)
		{
		case PipelineStage::ComputeShader:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		case PipelineStage::DrawIndirect:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

		case PipelineStage::VertexInput:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;

		case PipelineStage::VertexShader:
			return VK_ACCESS_SHADER_READ_BIT;

		case PipelineStage::FragmentShader:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		case PipelineStage::RenderTarget:
			return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		case PipelineStage::Transfer:
			return VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

		default:
			return 0;
		}
	}

	// 对应的 VK_KHR_synchronization2 版本
	VkAccessFlags2KHR GetAccessMaskForStage2(PipelineStage::Enum stage)
	{
		switch (stage)
		{
		case PipelineStage::ComputeShader:
			return VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR;

		case PipelineStage::DrawIndirect:
			return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;

		case PipelineStage::VertexInput:
			return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR | VK_ACCESS_2_INDEX_READ_BIT_KHR;

		case PipelineStage::VertexShader:
			return VK_ACCESS_2_SHADER_READ_BIT_KHR;

		case PipelineStage::FragmentShader:
			return VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR;

		case PipelineStage::RenderTarget:
			return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;

		case PipelineStage::Transfer:
			return VK_ACCESS_2_TRANSFER_READ_BIT_KHR | VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;

		default:
			return 0;
		}
	}
	DepthStencilCreation &DepthStencilCreation::SetDepth(bool write, VkCompareOp comparison_test)
	{
		depth_write = write;
		depth_compare_op = comparison_test;
		// Setting depth like this means we want to use the depth test
		depth_enable = 1;

		return *this;
	}

	BlendState &BlendState::SetColor(VkBlendFactor source_color, VkBlendFactor destination_color, VkBlendOp color_operation)
	{
		this->source_color = source_color;
		this->dest_color = destination_color;
		this->color_operation = color_operation;
		this->blend_enabled = 1;

		return *this;
	}

	BlendState &BlendState::SetAlpha(VkBlendFactor source_color, VkBlendFactor destination_color, VkBlendOp color_operation)
	{
		this->source_alpha = source_color;
		this->dest_alpha = destination_color;
		this->alpha_operation = color_operation;
		this->separate_blend = 1;

		return *this;
	}

	BlendState &BlendState::SetColorWriteMask(ColorWriteEnabled::Mask value)
	{
		color_write_mask = value;
		return *this;
	}

	BlendStateCreation &BlendStateCreation::Reset()
	{
		active_states = 0;

		return *this;
	}

	BlendState &BlendStateCreation::AddBlendState()
	{
		return blend_states[active_states++];
	}

	BufferCreation &BufferCreation::Reset()
	{
		type_flags = 0;
		usage = ResourceUsageType::Immutable;
		size = 0;
		initial_data = nullptr;
		persistent = 0;
		device_only = 0;
		name = nullptr;
		immediate_creation = false;

		return *this;
	}

	BufferCreation &BufferCreation::Set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage, uint32_t size)
	{
		type_flags = flags;
		this->usage = usage;
		this->size = size;

		return *this;
	}

	BufferCreation &BufferCreation::SetData(void *data)
	{
		initial_data = data;

		return *this;
	}

	BufferCreation &BufferCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	BufferCreation &BufferCreation::SetPersistent()
	{
		persistent = 1;
		return *this;
	}

	BufferCreation &BufferCreation::SetDeviceOnly()
	{
		device_only = 1;
		return *this;
	}

	BufferCreation &BufferCreation::SetImmediate()
	{
		immediate_creation = true;
		return *this;
	}

	SamplerCreation &SamplerCreation::SetMinMagMip(VkFilter min, VkFilter max, VkSamplerMipmapMode mip)
	{
		min_filter = min;
		mag_filter = max;
		mip_filter = mip;

		return *this;
	}

	SamplerCreation &SamplerCreation::SetMinMag(VkFilter min, VkFilter max)
	{
		min_filter = min;
		mag_filter = max;
		return *this;
	}

	SamplerCreation &SamplerCreation::SetMip(VkSamplerMipmapMode mip)
	{
		mip_filter = mip;
		return *this;
	}

	SamplerCreation &SamplerCreation::SetAddressModeU(VkSamplerAddressMode u)
	{
		address_mode_u = u;

		return *this;
	}

	SamplerCreation &SamplerCreation::SetAddressModeUV(VkSamplerAddressMode u, VkSamplerAddressMode v)
	{
		address_mode_u = u;
		address_mode_v = v;

		return *this;
	}

	SamplerCreation &SamplerCreation::SetAddressModeUVW(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w)
	{
		address_mode_u = u;
		address_mode_v = v;
		address_mode_w = w;

		return *this;
	}

	SamplerCreation &SamplerCreation::SetName(const char *name)
	{
		this->name = name;

		return *this;
	}

	ShaderStateCreation &ShaderStateCreation::Reset()
	{
		stages_count = 0;
		return *this;
	}

	ShaderStateCreation &ShaderStateCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	ShaderStateCreation &ShaderStateCreation::AddStage(const char *code, uint32_t code_size, VkShaderStageFlagBits type)
	{
		for (uint32_t s = 0; s < stages_count; ++s)
		{
			ShaderStage &stage = stages[s];

			if (stage.type == type)
			{
				stage.code = code;
				stage.code_size = code_size;
				return *this;
			}
		}

		stages[stages_count].code = code;
		stages[stages_count].code_size = (uint32_t)code_size;
		stages[stages_count].type = type;
		++stages_count;

		return *this;
	}

	ShaderStateCreation &ShaderStateCreation::SetSpvInput(bool value)
	{
		spv_input = value;
		return *this;
	}

	DescriptorSetLayoutCreation &DescriptorSetLayoutCreation::Reset()
	{
		num_bindings = 0;
		set_index = 0;
		return *this;
	}

	DescriptorSetLayoutCreation &DescriptorSetLayoutCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	DescriptorSetLayoutCreation &DescriptorSetLayoutCreation::SetSetIndex(uint32_t index)
	{
		set_index = index;
		return *this;
	}

	DescriptorSetLayoutCreation &DescriptorSetLayoutCreation::AddBinding(const Binding &binding)
	{
		bindings[num_bindings++] = binding;
		return *this;
	}

	DescriptorSetLayoutCreation &DescriptorSetLayoutCreation::AddBinding(VkDescriptorType type, uint32_t index, uint32_t count, const char *name)
	{
		bindings[num_bindings++] = {type, (uint16_t)index, (uint16_t)count, name};
		return *this;
	}

	DescriptorSetLayoutCreation &DescriptorSetLayoutCreation::AddBindingAtIndex(const Binding &binding, int index)
	{
		bindings[index] = binding;
		num_bindings = static_cast<uint32_t>(index + 1) > num_bindings ? (index + 1) : num_bindings;
		return *this;
	}

	DescriptorSetCreation &DescriptorSetCreation::Reset()
	{
		num_resources = 0;
		return *this;
	}

	DescriptorSetCreation &DescriptorSetCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	DescriptorSetCreation &DescriptorSetCreation::SetLayout(DescriptorSetLayoutHandle layout)
	{
		this->layout = layout;
		return *this;
	}

	DescriptorSetCreation &DescriptorSetCreation::Texture(TextureHandle texture, uint16_t binding)
	{
		// Set a default sampler
		samplers[num_resources] = k_invalid_sampler;
		bindings[num_resources] = binding;
		resources[num_resources] = texture.index;
		num_resources++;
		return *this;
	}

	DescriptorSetCreation &DescriptorSetCreation::TextureSampler(TextureHandle texture, SamplerHandle sampler, uint16_t binding)
	{
		bindings[num_resources] = binding;
		resources[num_resources] = texture.index;
		samplers[num_resources++] = sampler;
		return *this;
	}

	DescriptorSetCreation &DescriptorSetCreation::Buffer(BufferHandle buffer, uint16_t binding)
	{
		samplers[num_resources] = k_invalid_sampler;
		bindings[num_resources] = binding;
		resources[num_resources] = buffer.index;
		num_resources++;
		return *this;
	}

	VertexInputCreation &VertexInputCreation::Reset()
	{
		num_vertex_streams = num_vertex_attributes = 0;
		return *this;
	}

	VertexInputCreation &VertexInputCreation::AddVertexStream(const VertexStream &stream)
	{
		vertex_streams[num_vertex_streams++] = stream;
		return *this;
	}

	VertexInputCreation &VertexInputCreation::AddVertexAttribute(const VertexAttribute &attribute)
	{
		vertex_attributes[num_vertex_attributes++] = attribute;
		return *this;
	}

	RenderPassOutput &RenderPassOutput::Reset()
	{
		num_color_formats = 0;
		for (uint32_t i = 0; i < k_max_image_outputs; ++i)
		{
			color_formats[i] = VK_FORMAT_UNDEFINED;
			color_final_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
			color_operations[i] = RenderPassOperation::DontCare;
		}

		depth_stencil_format = VK_FORMAT_UNDEFINED;
		depth_operation = stencil_operation = RenderPassOperation::DontCare;
		return *this;
	}

	RenderPassOutput &RenderPassOutput::Color(VkFormat format, VkImageLayout layout, RenderPassOperation::Enum load_op)
	{
		color_formats[num_color_formats] = format;
		color_operations[num_color_formats] = load_op;
		color_final_layouts[num_color_formats++] = layout;
		return *this;
	}

	RenderPassOutput &RenderPassOutput::Depth(VkFormat format, VkImageLayout layout)
	{
		depth_stencil_format = format;
		depth_stencil_final_layout = layout;
		return *this;
	}

	RenderPassOutput &RenderPassOutput::SetDepthStencilOperations(RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil)
	{
		depth_operation = depth;
		stencil_operation = stencil;
		return *this;
	}

	// PipelineCreation ///////////////////////////////////////////////////////
	PipelineCreation &PipelineCreation::AddDescriptorSetLayout(DescriptorSetLayoutHandle layout)
	{
		descriptor_set_layout[num_active_layouts++] = layout;
		return *this;
	}

	RenderPassOutput &PipelineCreation::RenderPassOutput()
	{
		return render_pass;
	}

	RenderPassCreation &RenderPassCreation::Reset()
	{
		num_render_targets = 0;
		depth_stencil_format = VK_FORMAT_UNDEFINED;
		for (uint32_t i = 0; i < k_max_image_outputs; ++i)
		{
			color_operations[i] = RenderPassOperation::DontCare;
		}
		depth_operation = stencil_operation = RenderPassOperation::DontCare;
		return *this;
	}

	RenderPassCreation &RenderPassCreation::AddAttachment(VkFormat format, VkImageLayout layout, RenderPassOperation::Enum load_op)
	{
		color_formats[num_render_targets] = format;
		color_operations[num_render_targets] = load_op;
		color_final_layouts[num_render_targets++] = layout;
		return *this;
	}

	RenderPassCreation &RenderPassCreation::SetDepthStencilTexture(VkFormat format, VkImageLayout layout)
	{
		depth_stencil_format = format;
		depth_stencil_final_layout = layout;
		return *this;
	}

	RenderPassCreation &RenderPassCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	RenderPassCreation &RenderPassCreation::SetDepthStencilOperations(RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil)
	{
		depth_operation = depth;
		stencil_operation = stencil;
		return *this;
	}

	FramebufferCreation &FramebufferCreation::Reset()
	{
		num_render_targets = 0;
		name = nullptr;
		depth_stencil_texture.index = k_invalid_index;

		resize = 0;
		scale_x = 1.f;
		scale_y = 1.f;

		return *this;
	}

	FramebufferCreation &FramebufferCreation::AddRenderTexture(TextureHandle texture)
	{
		output_textures[num_render_targets++] = texture;
		return *this;
	}

	FramebufferCreation &FramebufferCreation::SetDepthStencilTexture(TextureHandle texture)
	{
		depth_stencil_texture = texture;

		return *this;
	}

	FramebufferCreation &FramebufferCreation::SetScaling(float scale_x, float scale_y, uint8_t resize)
	{
		this->scale_x = scale_x;
		this->scale_y = scale_y;
		this->resize = resize;
		return *this;
	}

	FramebufferCreation &FramebufferCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	ExecutionBarrier &ExecutionBarrier::Reset()
	{
		num_image_barriers = num_memory_barriers = 0;
		source_pipeline_stage = PipelineStage::DrawIndirect;
		dest_pipeline_stage = PipelineStage::DrawIndirect;
		return *this;
	}

	ExecutionBarrier &ExecutionBarrier::Set(PipelineStage::Enum source, PipelineStage::Enum destination)
	{
		source_pipeline_stage = source;
		dest_pipeline_stage = destination;
		return *this;
	}

	ExecutionBarrier &ExecutionBarrier::AddImageBarrier(const ImageBarrier &image_barrier)
	{
		image_barriers[num_image_barriers++] = image_barrier;
		return *this;
	}

	ExecutionBarrier &ExecutionBarrier::AddMemoryBarrier(const MemoryBarrier &memory_barrier)
	{
		memory_barriers[num_memory_barriers++] = memory_barrier;
		return *this;
	}

	const char *ToCompilerExtension(VkShaderStageFlagBits value)
	{
		switch (value)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vert";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "frag";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "comp";
		default:
			return "";
		}
	}

	const char *ToStageDefines(VkShaderStageFlagBits value)
	{
		switch (value)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "VERTEX";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "FRAGMENT";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "COMPUTE";
		default:
			return "";
		}
	}

	VkImageType ToVkImageType(TextureType::Enum type)
	{
		static VkImageType s_vk_target[TextureType::Count] = {VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D, VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D};
		return s_vk_target[type];
	}

	VkImageViewType ToVkImageViewType(TextureType::Enum type)
	{
		static VkImageViewType s_vk_data[] = {VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY};
		return s_vk_data[type];
	}

	VkFormat ToVkVertexFormat(VertexComponentFormat::Enum value)
	{
		// Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, Uint, Uint2, Uint4, Count
		static VkFormat s_vk_vertex_formats[VertexComponentFormat::Count] = {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, /*MAT4 TODO*/ VK_FORMAT_R32G32B32A32_SFLOAT,
																			 VK_FORMAT_R8_SINT, VK_FORMAT_R8G8B8A8_SNORM, VK_FORMAT_R8_UINT, VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SNORM,
																			 VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32A32_UINT};

		return s_vk_vertex_formats[value];
	}

	VkPipelineStageFlags ToVkPipelineStage(PipelineStage::Enum value)
	{
		static VkPipelineStageFlags s_vk_values[] = {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
		return s_vk_values[value];
	}

	VkAccessFlags UtilToVkAccessFlags(ResourceState state)
	{
		VkAccessFlags ret = 0;
		if (state & RESOURCE_STATE_COPY_SOURCE)
		{
			ret |= VK_ACCESS_TRANSFER_READ_BIT;
		}
		if (state & RESOURCE_STATE_COPY_DEST)
		{
			ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		{
			ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		}
		if (state & RESOURCE_STATE_INDEX_BUFFER)
		{
			ret |= VK_ACCESS_INDEX_READ_BIT;
		}
		if (state & RESOURCE_STATE_UNORDERED_ACCESS)
		{
			ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		}
		if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
		{
			ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		}
		if (state & RESOURCE_STATE_RENDER_TARGET)
		{
			ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		if (state & RESOURCE_STATE_DEPTH_WRITE)
		{
			ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
		if (state & RESOURCE_STATE_SHADER_RESOURCE)
		{
			ret |= VK_ACCESS_SHADER_READ_BIT;
		}
		if (state & RESOURCE_STATE_PRESENT)
		{
			ret |= VK_ACCESS_MEMORY_READ_BIT;
		}
#ifdef ENABLE_RAYTRACING
		if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		{
			ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
		}
#endif

		return ret;
	}

	VkAccessFlags UtilToVkAccessFlags2(ResourceState state)
	{
		VkAccessFlags ret = 0;
		if (state & RESOURCE_STATE_COPY_SOURCE)
		{
			ret |= VK_ACCESS_2_TRANSFER_READ_BIT_KHR;
		}
		if (state & RESOURCE_STATE_COPY_DEST)
		{
			ret |= VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
		}
		if (state & RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		{
			ret |= VK_ACCESS_2_UNIFORM_READ_BIT_KHR | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
		}
		if (state & RESOURCE_STATE_INDEX_BUFFER)
		{
			ret |= VK_ACCESS_2_INDEX_READ_BIT_KHR;
		}
		if (state & RESOURCE_STATE_UNORDERED_ACCESS)
		{
			ret |= VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		}
		if (state & RESOURCE_STATE_INDIRECT_ARGUMENT)
		{
			ret |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;
		}
		if (state & RESOURCE_STATE_RENDER_TARGET)
		{
			ret |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
		}
		if (state & RESOURCE_STATE_DEPTH_WRITE)
		{
			ret |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR;
		}
		if (state & RESOURCE_STATE_SHADER_RESOURCE)
		{
			ret |= VK_ACCESS_2_SHADER_READ_BIT_KHR;
		}
		if (state & RESOURCE_STATE_PRESENT)
		{
			ret |= VK_ACCESS_2_MEMORY_READ_BIT_KHR;
		}
#ifdef ENABLE_RAYTRACING
		if (state & RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		{
			ret |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
		}
#endif

		return ret;
	}

	VkImageLayout UtilToVkImageLayout(ResourceState usage)
	{
		if (usage & RESOURCE_STATE_COPY_SOURCE)
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		if (usage & RESOURCE_STATE_COPY_DEST)
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		if (usage & RESOURCE_STATE_RENDER_TARGET)
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (usage & RESOURCE_STATE_DEPTH_WRITE)
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (usage & RESOURCE_STATE_DEPTH_READ)
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		if (usage & RESOURCE_STATE_UNORDERED_ACCESS)
			return VK_IMAGE_LAYOUT_GENERAL;

		if (usage & RESOURCE_STATE_SHADER_RESOURCE)
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (usage & RESOURCE_STATE_PRESENT)
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		if (usage == RESOURCE_STATE_COMMON)
			return VK_IMAGE_LAYOUT_GENERAL;

		return VK_IMAGE_LAYOUT_UNDEFINED;
	}

	VkImageLayout UtilToVkImageLayout2(ResourceState usage)
	{
		if (usage & RESOURCE_STATE_COPY_SOURCE)
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		if (usage & RESOURCE_STATE_COPY_DEST)
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		if (usage & RESOURCE_STATE_RENDER_TARGET)
			return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

		if (usage & RESOURCE_STATE_DEPTH_WRITE)
			return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;

		if (usage & RESOURCE_STATE_DEPTH_READ)
			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

		if (usage & RESOURCE_STATE_UNORDERED_ACCESS)
			return VK_IMAGE_LAYOUT_GENERAL;

		if (usage & RESOURCE_STATE_SHADER_RESOURCE)
			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;

		if (usage & RESOURCE_STATE_PRESENT)
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		if (usage == RESOURCE_STATE_COMMON)
			return VK_IMAGE_LAYOUT_GENERAL;

		return VK_IMAGE_LAYOUT_UNDEFINED;
	}

	VkDescriptorBufferInfo UtilToVkDescriptorBufferInfo(Buffer *buffer)
	{
		VkDescriptorBufferInfo info;
		info.buffer = buffer->vk_buffer;
		info.offset = 0;
		info.range = buffer->size;
		return info;
	}

	VkDescriptorImageInfo UtilToVkDescriptorImageInfo(Texture *texture)
	{
		VkDescriptorImageInfo info;
		info.imageLayout = UtilToVkImageLayout(texture->state);
		info.imageView = texture->vk_image_view;
		if (texture->sampler)
		{
			info.sampler = texture->sampler->vk_sampler;
		}
		else
		{
			info.sampler = VK_NULL_HANDLE;
		}
		return info;
	}

	VkPipelineStageFlags UtilDeterminePipelineStageFlags(VkAccessFlags access_flags, QueueType::Enum queue_type)
	{
		VkPipelineStageFlags flags = 0;

		switch (queue_type)
		{
		case QueueType::Graphics:
		{
			if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

			if ((access_flags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
			{
				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

				// TODO(marco): check RT extension is present/enabled
				flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			}

			if ((access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			if ((access_flags & (VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR)) != 0)
				flags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

			if ((access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			if ((access_flags & VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR) != 0)
				flags = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

			if ((access_flags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

			break;
		}
		case QueueType::Compute:
		{
			if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
				(access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
				(access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
				(access_flags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

			if ((access_flags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			break;
		}
		case QueueType::CopyTransfer:
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		default:
			break;
		}

		// Compatible with both compute and graphics queues
		if ((access_flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
			flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

		if ((access_flags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

		if ((access_flags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_HOST_BIT;

		if (flags == 0)
			flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		return flags;
	}

	VkPipelineStageFlags2KHR UtilDeterminePipelineStageFlags2(VkAccessFlags2KHR access_flags, QueueType::Enum queue_type)
	{
		VkPipelineStageFlags2KHR flags = 0;

		switch (queue_type)
		{
		case QueueType::Graphics:
		{
			if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR;

			if ((access_flags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
			{
				flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
				flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
				flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

				// TODO(marco): check RT extension is present/enabled
				// flags |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
			}

			if ((access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
				flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;

			if ((access_flags & (VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR)) != 0)
				flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

			if ((access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

			if ((access_flags & VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR) != 0)
				flags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

			if ((access_flags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;

			break;
		}
		case QueueType::Compute:
		{
			if ((access_flags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
				(access_flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
				(access_flags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
				(access_flags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
				return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

			if ((access_flags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) != 0)
				flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

			break;
		}
		case QueueType::CopyTransfer:
			return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
		default:
			break;
		}

		// Compatible with both compute and graphics queues
		if ((access_flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
			flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;

		if ((access_flags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;

		if ((access_flags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
			flags |= VK_PIPELINE_STAGE_2_HOST_BIT_KHR;

		if (flags == 0)
			flags = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;

		return flags;
	}

	ResourceState UtilDetermineResourceState(VkDescriptorType type, VkShaderStageFlags stage_flags)
	{
		switch (type)
		{
			// 存储型资源 - 可读写访问
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			return RESOURCE_STATE_UNORDERED_ACCESS;

		// 只读型Uniform资源
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			return RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		// 采样和图像资源
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			return RESOURCE_STATE_SHADER_RESOURCE;

		// 输入附件
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			return RESOURCE_STATE_SHADER_RESOURCE;

		// 采样器本身不需要状态转换
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			return RESOURCE_STATE_COMMON;

		default:
			return RESOURCE_STATE_COMMON;
		}
	}

	void UtilAddImageBarrier(GpuDevice *gpu, VkCommandBuffer command_buffer, Texture *texture, ResourceState new_state, uint32_t base_mip_level, uint32_t mip_count, bool is_depth)
	{
		UtilAddImageBarrier(gpu, command_buffer, texture->vk_image, texture->state, new_state, base_mip_level, mip_count, is_depth);
		texture->state = new_state;
	}

	void UtilAddImageBarrier(GpuDevice *gpu, VkCommandBuffer command_buffer, VkImage image, ResourceState old_state, ResourceState new_state, uint32_t base_mip_level, uint32_t mip_count, bool is_depth)
	{
		if (gpu->synchronization2_extension_present_)
		{
			VkImageMemoryBarrier2KHR barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR};
			barrier.srcAccessMask = UtilToVkAccessFlags2(old_state);
			barrier.srcStageMask = UtilDeterminePipelineStageFlags2(barrier.srcAccessMask, QueueType::Graphics);
			barrier.dstAccessMask = UtilToVkAccessFlags2(new_state);
			barrier.dstStageMask = UtilDeterminePipelineStageFlags2(barrier.dstAccessMask, QueueType::Graphics);
			barrier.oldLayout = UtilToVkImageLayout2(old_state);
			barrier.newLayout = UtilToVkImageLayout2(new_state);
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.baseMipLevel = base_mip_level;
			barrier.subresourceRange.levelCount = mip_count;

			VkDependencyInfoKHR dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
			dependency_info.imageMemoryBarrierCount = 1;
			dependency_info.pImageMemoryBarriers = &barrier;

			// vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);
			vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);
		}
		else
		{
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			barrier.image = image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = mip_count;

			barrier.subresourceRange.baseMipLevel = base_mip_level;
			barrier.oldLayout = UtilToVkImageLayout(old_state);
			barrier.newLayout = UtilToVkImageLayout(new_state);
			barrier.srcAccessMask = UtilToVkAccessFlags(old_state);
			barrier.dstAccessMask = UtilToVkAccessFlags(new_state);

			const VkPipelineStageFlags source_stage_mask = UtilDeterminePipelineStageFlags(barrier.srcAccessMask, QueueType::Graphics);
			const VkPipelineStageFlags destination_stage_mask = UtilDeterminePipelineStageFlags(barrier.dstAccessMask, QueueType::Graphics);

			vkCmdPipelineBarrier(command_buffer, source_stage_mask, destination_stage_mask, 0,
								 0, nullptr, 0, nullptr, 1, &barrier);
		}
	}

	void UtilAddImageBarrierExt(GpuDevice *gpu, VkCommandBuffer command_buffer, VkImage image, ResourceState old_state, ResourceState new_state, uint32_t base_mip_level, uint32_t mip_count, uint32_t base_array_layer, uint32_t array_layer_count, bool is_depth, uint32_t source_family, uint32_t destination_family, QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type)
	{
		if (gpu->synchronization2_extension_present_)
		{
			VkImageMemoryBarrier2KHR barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR};
			barrier.srcAccessMask = UtilToVkAccessFlags2(old_state);
			barrier.srcStageMask = UtilDeterminePipelineStageFlags2(barrier.srcAccessMask, source_queue_type);
			barrier.dstAccessMask = UtilToVkAccessFlags2(new_state);
			barrier.dstStageMask = UtilDeterminePipelineStageFlags2(barrier.dstAccessMask, destination_queue_type);
			barrier.oldLayout = UtilToVkImageLayout2(old_state);
			barrier.newLayout = UtilToVkImageLayout2(new_state);
			barrier.srcQueueFamilyIndex = source_family;
			barrier.dstQueueFamilyIndex = destination_family;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = base_array_layer;
			barrier.subresourceRange.layerCount = array_layer_count;
			barrier.subresourceRange.baseMipLevel = base_mip_level;
			barrier.subresourceRange.levelCount = mip_count;

			VkDependencyInfoKHR dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
			dependency_info.imageMemoryBarrierCount = 1;
			dependency_info.pImageMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);
		}
		else
		{
			VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			barrier.image = image;
			barrier.srcQueueFamilyIndex = source_family;
			barrier.dstQueueFamilyIndex = destination_family;
			barrier.subresourceRange.aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseArrayLayer = base_array_layer;
			barrier.subresourceRange.layerCount = array_layer_count;
			barrier.subresourceRange.levelCount = mip_count;

			barrier.subresourceRange.baseMipLevel = base_mip_level;
			barrier.oldLayout = UtilToVkImageLayout(old_state);
			barrier.newLayout = UtilToVkImageLayout(new_state);
			barrier.srcAccessMask = UtilToVkAccessFlags(old_state);
			barrier.dstAccessMask = UtilToVkAccessFlags(new_state);

			const VkPipelineStageFlags source_stage_mask = UtilDeterminePipelineStageFlags(barrier.srcAccessMask, source_queue_type);
			const VkPipelineStageFlags destination_stage_mask = UtilDeterminePipelineStageFlags(barrier.dstAccessMask, destination_queue_type);

			vkCmdPipelineBarrier(command_buffer, source_stage_mask, destination_stage_mask, 0,
								 0, nullptr, 0, nullptr, 1, &barrier);
		}
	}

	void UtilAddImageBarrierExt(GpuDevice *gpu, VkCommandBuffer command_buffer, Texture *texture, ResourceState new_state, uint32_t base_mip_level, uint32_t mip_count, uint32_t base_array_layer, uint32_t array_layer_count, bool is_depth, uint32_t source_family, uint32_t destination_family, QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type)
	{
		UtilAddImageBarrierExt(gpu, command_buffer, texture->vk_image, texture->state, new_state, base_mip_level, mip_count, base_array_layer, array_layer_count,
							   is_depth, source_family, destination_family, source_queue_type, destination_queue_type);
		texture->state = new_state;
	}

	void UtilAddBufferBarrier(GpuDevice *gpu, VkCommandBuffer command_buffer, Buffer *buffer, ResourceState old_state, ResourceState new_state, uint32_t buffer_size)
	{
		UtilAddBufferBarrierExt(gpu, command_buffer, buffer->vk_buffer, old_state, new_state, buffer_size,
								VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, QueueType::Graphics, QueueType::Graphics);
	}

	void UtilAddBufferBarrier(GpuDevice *gpu, VkCommandBuffer command_buffer, VkBuffer buffer, ResourceState old_state, ResourceState new_state, uint32_t buffer_size)
	{
		UtilAddBufferBarrierExt(gpu, command_buffer, buffer, old_state, new_state, buffer_size,
								VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, QueueType::Graphics, QueueType::Graphics);
	}

	void UtilAddBufferBarrierExt(GpuDevice *gpu, VkCommandBuffer command_buffer, VkBuffer buffer, ResourceState old_state, ResourceState new_state, uint32_t buffer_size, uint32_t source_family, uint32_t destination_family, QueueType::Enum source_queue_type, QueueType::Enum destination_queue_type)
	{
		if (gpu->synchronization2_extension_present_)
		{
			VkBufferMemoryBarrier2KHR barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR};
			barrier.srcAccessMask = UtilToVkAccessFlags2(old_state);
			barrier.srcStageMask = UtilDeterminePipelineStageFlags2(barrier.srcAccessMask, source_queue_type);
			barrier.dstAccessMask = UtilToVkAccessFlags2(new_state);
			barrier.dstStageMask = UtilDeterminePipelineStageFlags2(barrier.dstAccessMask, destination_queue_type);
			barrier.buffer = buffer;
			barrier.offset = 0;
			barrier.size = buffer_size;

			VkDependencyInfoKHR dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
			dependency_info.bufferMemoryBarrierCount = 1;
			dependency_info.pBufferMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);
		}
		else
		{
			VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
			barrier.buffer = buffer;
			barrier.srcQueueFamilyIndex = source_family;
			barrier.dstQueueFamilyIndex = destination_family;
			barrier.offset = 0;
			barrier.size = buffer_size;
			barrier.srcAccessMask = UtilToVkAccessFlags(old_state);
			barrier.dstAccessMask = UtilToVkAccessFlags(new_state);

			const VkPipelineStageFlags source_stage_mask = UtilDeterminePipelineStageFlags(barrier.srcAccessMask, source_queue_type);
			const VkPipelineStageFlags destination_stage_mask = UtilDeterminePipelineStageFlags(barrier.dstAccessMask, destination_queue_type);

			vkCmdPipelineBarrier(command_buffer, source_stage_mask, destination_stage_mask, 0,
								 0, nullptr, 1, &barrier, 0, nullptr);
		}
	}

	void UtilAddStateBarrier(GpuDevice *gpu, VkCommandBuffer command_buffer, PipelineStage::Enum source_stage, PipelineStage::Enum destination_stage)
	{
		if (gpu->synchronization2_extension_present_)
		{
			VkMemoryBarrier2KHR barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR};

			// 设置源阶段和访问掩码
			barrier.srcStageMask = ToVkPipelineStage(source_stage);
			barrier.srcAccessMask = GetAccessMaskForStage(source_stage);

			// 设置目标阶段和访问掩码
			barrier.dstStageMask = ToVkPipelineStage(destination_stage);
			barrier.dstAccessMask = GetAccessMaskForStage(destination_stage);

			VkDependencyInfoKHR dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
			dependency_info.memoryBarrierCount = 1;
			dependency_info.pMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2KHR(command_buffer, &dependency_info);
		}
		else
		{
			VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};

			// 设置适当的访问掩码
			barrier.srcAccessMask = GetAccessMaskForStage(source_stage);
			barrier.dstAccessMask = GetAccessMaskForStage(destination_stage);

			vkCmdPipelineBarrier(
				command_buffer,
				ToVkPipelineStage(source_stage),
				ToVkPipelineStage(destination_stage),
				0,
				1, &barrier,
				0, nullptr,
				0, nullptr);
		}
	}

	VkFormat UtilStringToVkFormat(const char *format)
	{
		if (strcmp(format, "VK_FORMAT_R4G4_UNORM_PACK8") == 0)
		{
			return VK_FORMAT_R4G4_UNORM_PACK8;
		}
		if (strcmp(format, "VK_FORMAT_R4G4B4A4_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_B4G4R4A4_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R5G6B5_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_R5G6B5_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_B5G6R5_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_B5G6R5_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R5G5B5A1_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_B5G5R5A1_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_A1R5G5B5_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R8_UNORM") == 0)
		{
			return VK_FORMAT_R8_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8_SNORM") == 0)
		{
			return VK_FORMAT_R8_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8_USCALED") == 0)
		{
			return VK_FORMAT_R8_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8_SSCALED") == 0)
		{
			return VK_FORMAT_R8_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8_UINT") == 0)
		{
			return VK_FORMAT_R8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R8_SINT") == 0)
		{
			return VK_FORMAT_R8_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R8_SRGB") == 0)
		{
			return VK_FORMAT_R8_SRGB;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_UNORM") == 0)
		{
			return VK_FORMAT_R8G8_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_SNORM") == 0)
		{
			return VK_FORMAT_R8G8_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_USCALED") == 0)
		{
			return VK_FORMAT_R8G8_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_SSCALED") == 0)
		{
			return VK_FORMAT_R8G8_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_UINT") == 0)
		{
			return VK_FORMAT_R8G8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_SINT") == 0)
		{
			return VK_FORMAT_R8G8_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R8G8_SRGB") == 0)
		{
			return VK_FORMAT_R8G8_SRGB;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_UNORM") == 0)
		{
			return VK_FORMAT_R8G8B8_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_SNORM") == 0)
		{
			return VK_FORMAT_R8G8B8_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_USCALED") == 0)
		{
			return VK_FORMAT_R8G8B8_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_SSCALED") == 0)
		{
			return VK_FORMAT_R8G8B8_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_UINT") == 0)
		{
			return VK_FORMAT_R8G8B8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_SINT") == 0)
		{
			return VK_FORMAT_R8G8B8_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8_SRGB") == 0)
		{
			return VK_FORMAT_R8G8B8_SRGB;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_UNORM") == 0)
		{
			return VK_FORMAT_B8G8R8_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_SNORM") == 0)
		{
			return VK_FORMAT_B8G8R8_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_USCALED") == 0)
		{
			return VK_FORMAT_B8G8R8_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_SSCALED") == 0)
		{
			return VK_FORMAT_B8G8R8_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_UINT") == 0)
		{
			return VK_FORMAT_B8G8R8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_SINT") == 0)
		{
			return VK_FORMAT_B8G8R8_SINT;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8_SRGB") == 0)
		{
			return VK_FORMAT_B8G8R8_SRGB;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_UNORM") == 0)
		{
			return VK_FORMAT_R8G8B8A8_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_SNORM") == 0)
		{
			return VK_FORMAT_R8G8B8A8_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_USCALED") == 0)
		{
			return VK_FORMAT_R8G8B8A8_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_SSCALED") == 0)
		{
			return VK_FORMAT_R8G8B8A8_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_UINT") == 0)
		{
			return VK_FORMAT_R8G8B8A8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_SINT") == 0)
		{
			return VK_FORMAT_R8G8B8A8_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R8G8B8A8_SRGB") == 0)
		{
			return VK_FORMAT_R8G8B8A8_SRGB;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_UNORM") == 0)
		{
			return VK_FORMAT_B8G8R8A8_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_SNORM") == 0)
		{
			return VK_FORMAT_B8G8R8A8_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_USCALED") == 0)
		{
			return VK_FORMAT_B8G8R8A8_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_SSCALED") == 0)
		{
			return VK_FORMAT_B8G8R8A8_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_UINT") == 0)
		{
			return VK_FORMAT_B8G8R8A8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_SINT") == 0)
		{
			return VK_FORMAT_B8G8R8A8_SINT;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8A8_SRGB") == 0)
		{
			return VK_FORMAT_B8G8R8A8_SRGB;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_UNORM_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_SNORM_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_USCALED_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_USCALED_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_SSCALED_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_SSCALED_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_UINT_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_UINT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_SINT_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_SINT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A8B8G8R8_SRGB_PACK32") == 0)
		{
			return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2R10G10B10_UNORM_PACK32") == 0)
		{
			return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2R10G10B10_SNORM_PACK32") == 0)
		{
			return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2R10G10B10_USCALED_PACK32") == 0)
		{
			return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2R10G10B10_SSCALED_PACK32") == 0)
		{
			return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2R10G10B10_UINT_PACK32") == 0)
		{
			return VK_FORMAT_A2R10G10B10_UINT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2R10G10B10_SINT_PACK32") == 0)
		{
			return VK_FORMAT_A2R10G10B10_SINT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2B10G10R10_UNORM_PACK32") == 0)
		{
			return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2B10G10R10_SNORM_PACK32") == 0)
		{
			return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2B10G10R10_USCALED_PACK32") == 0)
		{
			return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2B10G10R10_SSCALED_PACK32") == 0)
		{
			return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2B10G10R10_UINT_PACK32") == 0)
		{
			return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_A2B10G10R10_SINT_PACK32") == 0)
		{
			return VK_FORMAT_A2B10G10R10_SINT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_R16_UNORM") == 0)
		{
			return VK_FORMAT_R16_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16_SNORM") == 0)
		{
			return VK_FORMAT_R16_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16_USCALED") == 0)
		{
			return VK_FORMAT_R16_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16_SSCALED") == 0)
		{
			return VK_FORMAT_R16_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16_UINT") == 0)
		{
			return VK_FORMAT_R16_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R16_SINT") == 0)
		{
			return VK_FORMAT_R16_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R16_SFLOAT") == 0)
		{
			return VK_FORMAT_R16_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_UNORM") == 0)
		{
			return VK_FORMAT_R16G16_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_SNORM") == 0)
		{
			return VK_FORMAT_R16G16_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_USCALED") == 0)
		{
			return VK_FORMAT_R16G16_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_SSCALED") == 0)
		{
			return VK_FORMAT_R16G16_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_UINT") == 0)
		{
			return VK_FORMAT_R16G16_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_SINT") == 0)
		{
			return VK_FORMAT_R16G16_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16_SFLOAT") == 0)
		{
			return VK_FORMAT_R16G16_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_UNORM") == 0)
		{
			return VK_FORMAT_R16G16B16_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_SNORM") == 0)
		{
			return VK_FORMAT_R16G16B16_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_USCALED") == 0)
		{
			return VK_FORMAT_R16G16B16_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_SSCALED") == 0)
		{
			return VK_FORMAT_R16G16B16_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_UINT") == 0)
		{
			return VK_FORMAT_R16G16B16_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_SINT") == 0)
		{
			return VK_FORMAT_R16G16B16_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16_SFLOAT") == 0)
		{
			return VK_FORMAT_R16G16B16_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_UNORM") == 0)
		{
			return VK_FORMAT_R16G16B16A16_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_SNORM") == 0)
		{
			return VK_FORMAT_R16G16B16A16_SNORM;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_USCALED") == 0)
		{
			return VK_FORMAT_R16G16B16A16_USCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_SSCALED") == 0)
		{
			return VK_FORMAT_R16G16B16A16_SSCALED;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_UINT") == 0)
		{
			return VK_FORMAT_R16G16B16A16_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_SINT") == 0)
		{
			return VK_FORMAT_R16G16B16A16_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R16G16B16A16_SFLOAT") == 0)
		{
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R32_UINT") == 0)
		{
			return VK_FORMAT_R32_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R32_SINT") == 0)
		{
			return VK_FORMAT_R32_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R32_SFLOAT") == 0)
		{
			return VK_FORMAT_R32_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32_UINT") == 0)
		{
			return VK_FORMAT_R32G32_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32_SINT") == 0)
		{
			return VK_FORMAT_R32G32_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32_SFLOAT") == 0)
		{
			return VK_FORMAT_R32G32_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32B32_UINT") == 0)
		{
			return VK_FORMAT_R32G32B32_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32B32_SINT") == 0)
		{
			return VK_FORMAT_R32G32B32_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32B32_SFLOAT") == 0)
		{
			return VK_FORMAT_R32G32B32_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32B32A32_UINT") == 0)
		{
			return VK_FORMAT_R32G32B32A32_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32B32A32_SINT") == 0)
		{
			return VK_FORMAT_R32G32B32A32_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R32G32B32A32_SFLOAT") == 0)
		{
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R64_UINT") == 0)
		{
			return VK_FORMAT_R64_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R64_SINT") == 0)
		{
			return VK_FORMAT_R64_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R64_SFLOAT") == 0)
		{
			return VK_FORMAT_R64_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64_UINT") == 0)
		{
			return VK_FORMAT_R64G64_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64_SINT") == 0)
		{
			return VK_FORMAT_R64G64_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64_SFLOAT") == 0)
		{
			return VK_FORMAT_R64G64_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64B64_UINT") == 0)
		{
			return VK_FORMAT_R64G64B64_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64B64_SINT") == 0)
		{
			return VK_FORMAT_R64G64B64_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64B64_SFLOAT") == 0)
		{
			return VK_FORMAT_R64G64B64_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64B64A64_UINT") == 0)
		{
			return VK_FORMAT_R64G64B64A64_UINT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64B64A64_SINT") == 0)
		{
			return VK_FORMAT_R64G64B64A64_SINT;
		}
		if (strcmp(format, "VK_FORMAT_R64G64B64A64_SFLOAT") == 0)
		{
			return VK_FORMAT_R64G64B64A64_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_B10G11R11_UFLOAT_PACK32") == 0)
		{
			return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32") == 0)
		{
			return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_D16_UNORM") == 0)
		{
			return VK_FORMAT_D16_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_X8_D24_UNORM_PACK32") == 0)
		{
			return VK_FORMAT_X8_D24_UNORM_PACK32;
		}
		if (strcmp(format, "VK_FORMAT_D32_SFLOAT") == 0)
		{
			return VK_FORMAT_D32_SFLOAT;
		}
		if (strcmp(format, "VK_FORMAT_S8_UINT") == 0)
		{
			return VK_FORMAT_S8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_D16_UNORM_S8_UINT") == 0)
		{
			return VK_FORMAT_D16_UNORM_S8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_D24_UNORM_S8_UINT") == 0)
		{
			return VK_FORMAT_D24_UNORM_S8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_D32_SFLOAT_S8_UINT") == 0)
		{
			return VK_FORMAT_D32_SFLOAT_S8_UINT;
		}
		if (strcmp(format, "VK_FORMAT_BC1_RGB_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC1_RGB_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC1_RGBA_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC1_RGBA_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC2_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC2_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC2_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_BC2_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC3_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC3_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC3_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_BC3_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC4_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC4_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC4_SNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC4_SNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC5_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC5_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC5_SNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC5_SNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC6H_UFLOAT_BLOCK") == 0)
		{
			return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC6H_SFLOAT_BLOCK") == 0)
		{
			return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC7_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_BC7_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_BC7_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_BC7_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_EAC_R11_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_EAC_R11_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_EAC_R11_SNORM_BLOCK") == 0)
		{
			return VK_FORMAT_EAC_R11_SNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_EAC_R11G11_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_EAC_R11G11_SNORM_BLOCK") == 0)
		{
			return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_4x4_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_4x4_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_5x4_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_5x4_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_5x5_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_5x5_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_6x5_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_6x5_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_6x6_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_6x6_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x5_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x5_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x6_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x6_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x8_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x8_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x5_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x5_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x6_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x6_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x8_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x8_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x10_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x10_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_12x10_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_12x10_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_12x12_UNORM_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_12x12_SRGB_BLOCK") == 0)
		{
			return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
		}
		if (strcmp(format, "VK_FORMAT_G8B8G8R8_422_UNORM") == 0)
		{
			return VK_FORMAT_G8B8G8R8_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_B8G8R8G8_422_UNORM") == 0)
		{
			return VK_FORMAT_B8G8R8G8_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM") == 0)
		{
			return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM") == 0)
		{
			return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM") == 0)
		{
			return VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM") == 0)
		{
			return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM") == 0)
		{
			return VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_R10X6_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_R10X6_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R10X6G10X6_UNORM_2PACK16") == 0)
		{
			return VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16") == 0)
		{
			return VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16") == 0)
		{
			return VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
		}
		if (strcmp(format, "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16") == 0)
		{
			return VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R12X4_UNORM_PACK16") == 0)
		{
			return VK_FORMAT_R12X4_UNORM_PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R12X4G12X4_UNORM_2PACK16") == 0)
		{
			return VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
		}
		if (strcmp(format, "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16") == 0)
		{
			return VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16") == 0)
		{
			return VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
		}
		if (strcmp(format, "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16") == 0)
		{
			return VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16") == 0)
		{
			return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16;
		}
		if (strcmp(format, "VK_FORMAT_G16B16G16R16_422_UNORM") == 0)
		{
			return VK_FORMAT_G16B16G16R16_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_B16G16R16G16_422_UNORM") == 0)
		{
			return VK_FORMAT_B16G16R16G16_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM") == 0)
		{
			return VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM") == 0)
		{
			return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM") == 0)
		{
			return VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM") == 0)
		{
			return VK_FORMAT_G16_B16R16_2PLANE_422_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM") == 0)
		{
			return VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG") == 0)
		{
			return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT") == 0)
		{
			return VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT;
		}
		if (strcmp(format, "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT") == 0)
		{
			return VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT;
		}
		if (strcmp(format, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT") == 0)
		{
			return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT;
		}
		if (strcmp(format, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT") == 0)
		{
			return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT;
		}
		if (strcmp(format, "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT") == 0)
		{
			return VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT;
		}
		if (strcmp(format, "VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT") == 0)
		{
			return VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT;
		}
		if (strcmp(format, "VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT") == 0)
		{
			return VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT;
		}

		assert(false);
		return VK_FORMAT_UNDEFINED;
	}

	const std::string GetAssetPath(const std::string& path)
	{
		return std::string("../../" + path);
	}

	TextureCreation &TextureCreation::Reset()
	{
		mip_level_count = 1;
		array_layer_count = 1;
		initial_data = nullptr;
		alias = k_invalid_texture;
		transfer_queue = false;
		width = height = depth = 1;
		format = VK_FORMAT_UNDEFINED;
		flags = 0;
		immediate_creation = false;
		return *this;
	}

	TextureCreation &TextureCreation::SetImmediate()
	{
		immediate_creation = true;
		return *this;
	}

	TextureCreation &TextureCreation::SetTransferSrc()
	{
		transfer_queue = true;
		return *this;
	}

	TextureCreation &TextureCreation::SetSize(uint16_t width, uint16_t height, uint16_t depth, bool generate_mipmaps)
	{
		this->width = width;
		this->height = height;
		this->depth = depth;

		if (generate_mipmaps)
		{
			this->mip_level_count = static_cast<uint8_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}

		return *this;
	}

	TextureCreation &TextureCreation::SetFlags(uint8_t flags)
	{
		this->flags = flags;
		return *this;
	}

	TextureCreation &TextureCreation::SetMips(uint32_t mip_level_count)
	{
		this->mip_level_count = mip_level_count;
		return *this;
	}

	TextureCreation &TextureCreation::SetLayers(uint32_t layer_count)
	{
		this->array_layer_count = layer_count;
		return *this;
	}

	TextureCreation &TextureCreation::SetFormat(VkFormat format)
	{
		this->format = format;
		return *this;
	}

	TextureCreation &TextureCreation::SetFormatType(VkFormat format, TextureType::Enum type)
	{
		this->format = format;
		this->type = type;
		return *this;
	}

	TextureCreation &TextureCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	TextureCreation &TextureCreation::SetData(void *data)
	{
		this->initial_data = data;
		return *this;
	}

	TextureCreation &TextureCreation::SetAlias(TextureHandle alias)
	{
		this->alias = alias;
		return *this;
	}

	TextureViewCreation &TextureViewCreation::Reset()
	{
		parent_texture = k_invalid_texture;
		sub_resource = {0, 1, 0, 1};
		name = nullptr;
		view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
		return *this;
	}

	TextureViewCreation &TextureViewCreation::SetParentTexture(TextureHandle parent_texture)
	{
		this->parent_texture = parent_texture;
		return *this;
	}

	TextureViewCreation &TextureViewCreation::SetMips(uint32_t base_mip, uint32_t mip_level_count)
	{
		sub_resource.mip_base_level = base_mip;
		sub_resource.mip_level_count = mip_level_count;
		return *this;
	}

	TextureViewCreation &TextureViewCreation::SetArray(uint32_t base_layer, uint32_t layer_count)
	{
		sub_resource.array_base_layer = base_layer;
		sub_resource.array_layer_count = layer_count;
		return *this;
	}

	TextureViewCreation &TextureViewCreation::SetName(const char *name)
	{
		this->name = name;
		return *this;
	}

	TextureViewCreation &TextureViewCreation::SetViewType(VkImageViewType view_type)
	{
		this->view_type = view_type;
		return *this;
	}

}