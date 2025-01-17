#pragma once
// external
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
// lincore
#include "gpu_enums.h"

namespace lincore
{
	class GpuDevice;

	static const uint32_t k_invalid_index = 0xffffffff;

	static const uint32_t k_buffers_pool_size = 16384;
	static const uint32_t k_textures_pool_size = 512;
	static const uint32_t k_render_passes_pool_size = 256;
	static const uint32_t k_descriptor_set_layouts_pool_size = 128;
	static const uint32_t k_pipelines_pool_size = 128;
	static const uint32_t k_shaders_pool_size = 128;
	static const uint32_t k_descriptor_sets_pool_size = 4096;
	static const uint32_t k_samplers_pool_size = 1024;

	typedef uint32_t ResourceHandle;

	struct BufferHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const BufferHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct BufferHandle

	struct TextureHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const TextureHandle& other) const { return index == other.index; }
		bool IsValid() const  { return index != k_invalid_index; }
	}; // struct TextureHandle

	struct ShaderStateHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const ShaderStateHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct ShaderStateHandle

	struct SamplerHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const SamplerHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct SamplerHandle

	struct DescriptorSetLayoutHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const DescriptorSetLayoutHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct DescriptorSetLayoutHandle

	struct DescriptorSetHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const DescriptorSetHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct DescriptorSetHandle

	struct PipelineHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const PipelineHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct PipelineHandle

	struct RenderPassHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const RenderPassHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct RenderPassHandle

	struct FramebufferHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const FramebufferHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct FramebufferHandle

	struct PagePoolHandle {
		ResourceHandle index{k_invalid_index};
		bool operator==(const PagePoolHandle& other) const { return index == other.index; }
		bool IsValid() const { return index != k_invalid_index; }
	}; // struct FramebufferHandle

	// Invalid handles
	static BufferHandle k_invalid_buffer{ k_invalid_index };
	static TextureHandle k_invalid_texture{ k_invalid_index };
	static ShaderStateHandle k_invalid_shader{ k_invalid_index };
	static SamplerHandle k_invalid_sampler{ k_invalid_index };
	static DescriptorSetLayoutHandle k_invalid_layout{ k_invalid_index };
	static DescriptorSetHandle k_invalid_set{ k_invalid_index };
	static PipelineHandle k_invalid_pipeline{ k_invalid_index };
	static RenderPassHandle k_invalid_pass{ k_invalid_index };
	static FramebufferHandle            k_invalid_framebuffer{ k_invalid_index };
	static PagePoolHandle               k_invalid_page_pool{ k_invalid_index };
	// Consts
	static const uint8_t k_max_image_outputs = 8; // Maximum number of images/render_targets/fbo attachments usable.
	static const uint8_t k_max_descriptor_set_layouts = 8; // Maximum number of layouts in the pipeline.
	static const uint8_t k_max_shader_stages = 5; // Maximum simultaneous shader stages. Applicable to all different type of pipelines.
	static const uint8_t k_max_descriptors_per_set = 16; // Maximum list elements for both descriptor set layout and descriptor sets.
	static const uint8_t k_max_vertex_streams = 16;
	static const uint8_t k_max_vertex_attributes = 16;

	static const uint32_t k_submit_header_sentinel = 0xfefeb7ba;
	static const uint32_t k_max_resource_deletions = 64;

	// Resource creation structs
	struct Rect2D
	{
		float x = 0.f;
		float y = 0.f;
		float width = 0.f;
		float height = 0.f;
	}; // struct Rect2D

	struct Rect2DInt
	{
		int16_t x = 0;
		int16_t y = 0;
		uint16_t width = 0;
		uint16_t height = 0;
	}; // struct Rect2DInt

	struct Viewport
	{
		Rect2DInt rect;
		float min_depth = 0.f;
		float max_depth = 0.f;
	}; // struct Viewport

	struct ViewportState
	{
		uint32_t num_viewports = 0;
		uint32_t num_scissors = 0;

		Viewport* viewport = nullptr;
		Rect2DInt* scissor = nullptr;
	}; // struct ViewportState

	struct StencilOperationState
	{
		VkStencilOp fail_op = VK_STENCIL_OP_KEEP;
		VkStencilOp pass_op = VK_STENCIL_OP_KEEP;
		VkStencilOp depth_fail_op = VK_STENCIL_OP_KEEP;
		VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
		uint32_t compare_mask = 0xff;
		uint32_t write_mask = 0xff;
		uint32_t reference = 0xff;
	}; // struct StencilOperationState

	struct DepthStencilCreation
	{
		StencilOperationState front;
		StencilOperationState back;

		VkCompareOp depth_compare_op = VK_COMPARE_OP_ALWAYS;

		uint8_t depth_enable : 1;
		uint8_t depth_write : 1;
		uint8_t stencil_enable : 1;
		uint8_t pad : 5;

		// Default constructor
		DepthStencilCreation()
			: depth_enable(0), depth_write(0), stencil_enable(0)
		{
		}

		DepthStencilCreation& SetDepth(bool write, VkCompareOp comparison_test);
	}; // struct DepthStencilCreation

	struct BlendState
	{
		VkBlendFactor source_color = VK_BLEND_FACTOR_ONE;
		VkBlendFactor dest_color = VK_BLEND_FACTOR_ONE;
		VkBlendOp color_operation = VK_BLEND_OP_ADD;

		VkBlendFactor source_alpha = VK_BLEND_FACTOR_ONE;
		VkBlendFactor dest_alpha = VK_BLEND_FACTOR_ONE;
		VkBlendOp alpha_operation = VK_BLEND_OP_ADD;

		ColorWriteEnabled::Mask color_write_mask = ColorWriteEnabled::All_mask;

		uint8_t blend_enabled : 1;
		uint8_t separate_blend : 1;
		uint8_t pad : 6;

		BlendState() : blend_enabled(0), separate_blend(0) {}

		BlendState& SetColor(VkBlendFactor source_color, VkBlendFactor destination_color, VkBlendOp color_operation);
		BlendState& SetAlpha(VkBlendFactor source_color, VkBlendFactor destination_color, VkBlendOp color_operation);
		BlendState& SetColorWriteMask(ColorWriteEnabled::Mask value);
	}; // struct BlendState

	struct BlendStateCreation
	{
		BlendState blend_states[k_max_image_outputs];
		uint32_t active_states = 0;

		BlendStateCreation& Reset();
		BlendState& AddBlendState();
	}; // struct BlendStateCreation

	struct RasterizationCreation
	{
		VkCullModeFlagBits cull_mode = VK_CULL_MODE_NONE;
		VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		FillMode::Enum fill = FillMode::Solid;
	}; // struct RasterizationCreation

	struct BufferCreation
	{
		VkBufferUsageFlags type_flags = 0;
		ResourceUsageType::Enum usage = ResourceUsageType::Immutable;
		QueueType::Enum queue_type = QueueType::Graphics;
		uint32_t queue_family = VK_QUEUE_FAMILY_IGNORED;
		uint32_t size = 0;
		uint32_t persistent = 0;
		uint32_t device_only = 0;
		void* initial_data = nullptr;

		const char* name = nullptr;

		bool immediate_creation = false;

		BufferCreation& Reset();
		BufferCreation& Set(VkBufferUsageFlags flags, ResourceUsageType::Enum usage, uint32_t size);
		BufferCreation& SetData(void* data);
		BufferCreation& SetName(const char* name);
		BufferCreation& SetPersistent();
		BufferCreation& SetDeviceOnly();
		BufferCreation& SetImmediate();
		BufferCreation& SetQueueType(QueueType::Enum queue_type);
		BufferCreation& SetQueueFamily(uint32_t queue_family);
	}; // struct BufferCreation

	struct TextureCreation
	{
		void* initial_data = nullptr;
		uint16_t width = 1;
		uint16_t height = 1;
		uint16_t depth = 1;
		uint16_t array_layer_count = 1;
		uint8_t mip_level_count = 1;
		uint8_t flags = 0;    // TextureFlags bitmasks

		VkFormat format = VK_FORMAT_UNDEFINED;

		TextureType::Enum type = TextureType::Texture2D;
		QueueType::Enum queue_type = QueueType::Graphics;
		TextureHandle alias = k_invalid_texture;

		const char* name = nullptr;

		bool transfer_queue = false;

		bool immediate_creation  = false;
		uint32_t queue_family = VK_QUEUE_FAMILY_IGNORED;
		TextureCreation& Reset();
		TextureCreation& SetImmediate();
		TextureCreation& SetTransferSrc();
		TextureCreation& SetSize(uint16_t width, uint16_t height, uint16_t depth, bool generate_mipmaps = true);
		TextureCreation& SetFlags(uint8_t flags);
		TextureCreation& SetMips(uint32_t mip_level_count);
		TextureCreation& SetLayers(uint32_t layer_count);
		TextureCreation& SetFormat(VkFormat format);
		TextureCreation& SetFormatType(VkFormat format, TextureType::Enum type);
		TextureCreation& SetName(const char* name);
		TextureCreation& SetData(void* data);
		TextureCreation& SetAlias(TextureHandle alias);
		TextureCreation& SetQueueType(QueueType::Enum queue_type);
		TextureCreation& SetQueueFamily(uint32_t queue_family);
	}; // struct TextureCreation

	struct TextureSubResource
	{
		uint8_t mip_base_level = 0;
		uint8_t mip_level_count = 1;
		uint8_t array_base_layer = 0;
		uint8_t array_layer_count = 1;
	}; // struct TextureSubResource

	struct TextureViewCreation
	{
		TextureHandle parent_texture = k_invalid_texture;

		VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_1D;
		TextureSubResource sub_resource;

		const char* name = nullptr;

		bool immediate_creation  = false;

		TextureViewCreation& Reset();
		TextureViewCreation& SetParentTexture(TextureHandle parent_texture);
		TextureViewCreation& SetMips(uint32_t base_mip, uint32_t mip_level_count);
		TextureViewCreation& SetArray(uint32_t base_layer, uint32_t layer_count);
		TextureViewCreation& SetName(const char* name);
		TextureViewCreation& SetViewType(VkImageViewType view_type);
	}; // struct TextureViewCreation

	struct SamplerCreation
	{
		VkFilter min_filter = VK_FILTER_NEAREST;
		VkFilter mag_filter = VK_FILTER_NEAREST;
		VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		const char* name = nullptr;

		bool immediate_creation  = false;

		SamplerCreation& SetMinMagMip(VkFilter min, VkFilter max, VkSamplerMipmapMode mip);
		SamplerCreation& SetMinMag(VkFilter min, VkFilter max);
		SamplerCreation& SetMip(VkSamplerMipmapMode mip);
		SamplerCreation& SetAddressModeU(VkSamplerAddressMode u);
		SamplerCreation& SetAddressModeUV(VkSamplerAddressMode u, VkSamplerAddressMode v);
		SamplerCreation& SetAddressModeUVW(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);
		SamplerCreation& SetName(const char* name);
	}; // struct SamplerCreation

	struct ShaderStage
	{
		const char* code = nullptr;
		uint32_t code_size = 0;
		VkShaderStageFlagBits type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}; // struct ShaderStage

	struct ShaderStateCreation
	{
		ShaderStage stages[k_max_shader_stages];

		const char* name = nullptr;

		uint32_t stages_count = 0;
		uint32_t spv_input = 0;

		// Building helpers
		ShaderStateCreation& Reset();
		ShaderStateCreation& SetName(const char* name);
		ShaderStateCreation& AddStage(const char* code, uint32_t code_size, VkShaderStageFlagBits type);
		ShaderStateCreation& SetSpvInput(bool value);
	}; // struct ShaderStateCreation

	struct DescriptorSetLayoutCreation
	{
		// A single descriptor binding. It can be relative to one or more resources of the same type
		struct Binding
		{
			VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
			uint16_t start = 0;
			uint16_t count = 0;
			const char* name = nullptr; // Optional name for the binding
		};

		Binding bindings[k_max_descriptors_per_set];
		uint32_t num_bindings = 0;
		uint32_t set_index = 0;
		bool bindless = false;
		bool dynamic = false;

		const char* name = nullptr;

		DescriptorSetLayoutCreation& Reset();
		DescriptorSetLayoutCreation& SetName(const char* name);
		DescriptorSetLayoutCreation& SetSetIndex(uint32_t index);
		DescriptorSetLayoutCreation& AddBinding(const Binding& binding);
		DescriptorSetLayoutCreation& AddBinding(VkDescriptorType type, uint32_t index, uint32_t count, const char* name);
		DescriptorSetLayoutCreation& AddBindingAtIndex(const Binding& binding, int index);
	}; // struct DescriptorSetLayoutCreation

	struct DescriptorSetCreation
	{
		ResourceHandle resources[k_max_descriptors_per_set];
		SamplerHandle samplers[k_max_descriptors_per_set];
		uint16_t bindings[k_max_descriptors_per_set];

		DescriptorSetLayoutHandle layout;
		uint32_t num_resources = 0;

		const char* name = nullptr;

		DescriptorSetCreation& Reset();
		DescriptorSetCreation& SetName(const char* name);
		DescriptorSetCreation& SetLayout(DescriptorSetLayoutHandle layout);
		DescriptorSetCreation& Texture(TextureHandle texture, uint16_t binding);
		DescriptorSetCreation& TextureSampler(TextureHandle texture, SamplerHandle sampler, uint16_t binding);
		DescriptorSetCreation& Buffer(BufferHandle buffer, uint16_t binding);
	}; // struct DescriptorSetCreation

	struct DescriptorSetUpdate
	{
		DescriptorSetHandle descriptor_set;

		uint32_t frame_issued = 0;
	}; // struct DescriptorSetUpdate

	struct VertexAttribute
	{
		uint16_t location = 0;
		uint16_t binding = 0;
		uint32_t offset = 0;
		VertexComponentFormat::Enum format = VertexComponentFormat::Count;
	}; // struct VertexAttribute

	struct VertexStream
	{
		uint16_t binding = 0;
		uint16_t stride = 0;
		VertexInputRate::Enum input_rate = VertexInputRate::Count;
	}; // struct VertexStream

	struct VertexInputCreation
	{
		uint32_t num_vertex_streams = 0;
		uint32_t num_vertex_attributes = 0;

		VertexStream vertex_streams[k_max_vertex_streams];
		VertexAttribute vertex_attributes[k_max_vertex_attributes];

		VertexInputCreation& Reset();
		VertexInputCreation& AddVertexStream(const VertexStream& stream);
		VertexInputCreation& AddVertexAttribute(const VertexAttribute& attribute);
	}; // struct VertexInputCreation

	struct RenderPassOutput
	{
		VkFormat color_formats[k_max_image_outputs];
		VkImageLayout color_final_layouts[k_max_image_outputs];
		RenderPassOperation::Enum color_operations[k_max_image_outputs];

		VkFormat depth_stencil_format;
		VkImageLayout depth_stencil_final_layout;

		uint32_t num_color_formats;

		RenderPassOperation::Enum depth_operation = RenderPassOperation::DontCare;
		RenderPassOperation::Enum stencil_operation = RenderPassOperation::DontCare;

		RenderPassOutput& Reset();
		RenderPassOutput& Color(VkFormat format, VkImageLayout layout, RenderPassOperation::Enum load_op);
		RenderPassOutput& Depth(VkFormat format, VkImageLayout layout);
		RenderPassOutput& SetDepthStencilOperations(RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil);
	}; // struct RenderPassOutput

	struct RenderPassCreation
	{
		uint16_t num_render_targets = 0;

		VkFormat color_formats[k_max_image_outputs];
		VkImageLayout color_final_layouts[k_max_image_outputs];
		RenderPassOperation::Enum color_operations[k_max_image_outputs];

		VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
		VkImageLayout depth_stencil_final_layout;

		RenderPassOperation::Enum depth_operation = RenderPassOperation::DontCare;
		RenderPassOperation::Enum stencil_operation = RenderPassOperation::DontCare;

		const char* name = nullptr;

		RenderPassCreation& Reset();
		RenderPassCreation& AddAttachment(VkFormat format, VkImageLayout layout, RenderPassOperation::Enum load_op);
		RenderPassCreation& SetDepthStencilTexture(VkFormat format, VkImageLayout layout);
		RenderPassCreation& SetName(const char* name);
		RenderPassCreation& SetDepthStencilOperations(RenderPassOperation::Enum depth, RenderPassOperation::Enum stencil);

	}; // struct RenderPassCreation

	struct FramebufferCreation {

		RenderPassHandle render_pass;

		uint16_t num_render_targets = 0;

		TextureHandle output_textures[k_max_image_outputs];
		TextureHandle depth_stencil_texture = { k_invalid_index };

		uint16_t width = 0;
		uint16_t height = 0;

		float scale_x = 1.f;
		float scale_y = 1.f;
		uint8_t resize = 1;

		const char* name = nullptr;

		FramebufferCreation& Reset();
		FramebufferCreation& AddRenderTexture(TextureHandle texture);
		FramebufferCreation& SetDepthStencilTexture(TextureHandle texture);
		FramebufferCreation& SetScaling(float scale_x, float scale_y, uint8_t resize);
		FramebufferCreation& SetName(const char* name);

	}; // struct RenderPassCreation

	struct PipelineCreation {

		RasterizationCreation rasterization;
		DepthStencilCreation depth_stencil;
		BlendStateCreation blend_state;
		VertexInputCreation  vertex_input;
		ShaderStateCreation  shaders;

		VkPrimitiveTopology  topology;

		RenderPassOutput render_pass;
		DescriptorSetLayoutHandle descriptor_set_layout[k_max_descriptor_set_layouts];
		const ViewportState* viewport = nullptr;

		uint32_t num_active_layouts = 0;

		const char* name = nullptr;

		PipelineCreation& AddDescriptorSetLayout(DescriptorSetLayoutHandle handle);
		RenderPassOutput& RenderPassOutput();

	}; // struct PipelineCreation

	namespace TextureFormat
	{
		inline bool IsDepthStencil(VkFormat value) {
			return value == VK_FORMAT_D16_UNORM_S8_UINT || value == VK_FORMAT_D24_UNORM_S8_UINT || value == VK_FORMAT_D32_SFLOAT_S8_UINT;
		}
		inline bool IsDepthOnly(VkFormat value) {
			return value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_D32_SFLOAT;
		}
		inline bool IsStencilOnly(VkFormat value) {
			return value == VK_FORMAT_S8_UINT;
		}

		inline bool HasDepth(VkFormat value) {
			return (value >= VK_FORMAT_D16_UNORM && value < VK_FORMAT_S8_UINT) || (value >= VK_FORMAT_D16_UNORM_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT);
		}
		inline bool HasStencil(VkFormat value) {
			return value >= VK_FORMAT_S8_UINT && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
		}
		inline bool HasDepthOrStencil(VkFormat value) {
			return value >= VK_FORMAT_D16_UNORM && value <= VK_FORMAT_D32_SFLOAT_S8_UINT;
		}
	} // namespace TextureFormat

	struct DescriptorData {

		void* data = nullptr;

	}; // struct DescriptorData

	struct DescriptorBinding
	{
		VkDescriptorType type;
		uint16_t index = 0;
		uint16_t count = 0;
		uint16_t set = 0;

		const char* name = nullptr;
	}; // struct ResourceBinding

	struct ShaderStateDescription
	{
		void* native_handle = nullptr;
		const char* name = nullptr;
	}; // struct ShaderStateDescription

	struct BufferDescription
	{
		void* native_handle = nullptr;
		const char* name = nullptr;

		VkBufferUsageFlags type_flags = 0;
		ResourceUsageType::Enum usage = ResourceUsageType::Immutable;
		uint32_t size = 0;
		BufferHandle parent_handle;
	}; // struct BufferDescription

	struct TextureDescription
	{
		void* native_handle = nullptr;
		const char* name = nullptr;

		uint16_t width = 1;
		uint16_t height = 1;
		uint16_t depth = 1;
		uint8_t mipmaps = 1;
		uint8_t render_target = 0;
		uint8_t compute_access = 0;

		VkFormat format = VK_FORMAT_UNDEFINED;
		TextureType::Enum type = TextureType::Texture2D;
	}; // struct TextureDescription

	struct SamplerDescription
	{
		const char* name = nullptr;

		VkFilter min_filter = VK_FILTER_NEAREST;
		VkFilter mag_filter = VK_FILTER_NEAREST;
		VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		VkSamplerAddressMode address_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode address_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode address_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		VkSamplerReductionMode reduction_mode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

	}; // struct SamplerDescription

	struct DescriptorSetLayoutDescription
	{
		DescriptorBinding* bindings = nullptr;
		uint32_t num_active_bindings = 0;
	}; // struct DescriptorSetLayoutDescription

	struct DescriptorSetDescription
	{
		DescriptorData* resources = nullptr;
		uint32_t num_active_resources = 0;
	}; // struct DescriptorSetDescription

	struct PipelineDescription
	{
		ShaderStateHandle shader;
	}; // struct PipelineDescription

	// API-agnostic resource modifications ////////////////////////////////////

	struct MapBufferParameters
	{
		BufferHandle buffer;
		uint32_t offset = 0;
		uint32_t size = 0;
	}; // struct MapBufferParameters

	// Synchronization ////////////////////////////////////////////////////////

	struct ImageBarrier
	{
		TextureHandle texture;
	}; // struct ImageBarrier

	struct MemoryBarrier
	{
		BufferHandle buffer;
	}; // struct MemoryBarrier

	struct ExecutionBarrier
	{
		PipelineStage::Enum source_pipeline_stage;
		PipelineStage::Enum dest_pipeline_stage;

		uint32_t new_barrier_experimental = UINT32_MAX;
		uint32_t load_op = 0;

		uint32_t num_image_barriers;
		uint32_t num_memory_barriers;

		ImageBarrier image_barriers[8];
		MemoryBarrier memory_barriers[8];

		ExecutionBarrier& Reset();
		ExecutionBarrier& Set(PipelineStage::Enum source, PipelineStage::Enum destination);
		ExecutionBarrier& AddImageBarrier(const ImageBarrier& image_barrier);
		ExecutionBarrier& AddMemoryBarrier(const MemoryBarrier& memory_barrier);
	}; // struct ExecutionBarrier

	struct ResourceUpdate {
		ResourceUpdateType::Enum type;
		ResourceHandle handle;
		uint32_t current_frame;
		uint32_t deleting;
	}; // struct ResourceUpdate

	// Resources //////////////////////////////////////////////////////////////

	static const uint32_t k_max_swapchain_images = 3;
	static const uint32_t k_max_frames = 1;

	struct Buffer {
		VkBuffer vk_buffer;
		VmaAllocation vma_allocation;
		VkDeviceMemory vk_device_memory;
		VkDeviceSize vk_device_size;

		ResourceState state = RESOURCE_STATE_UNDEFINED;

		VkBufferUsageFlags type_flags = 0;
		ResourceUsageType::Enum usage = ResourceUsageType::Immutable;
		QueueType::Enum queue_type = QueueType::Graphics;
		uint32_t queue_family = VK_QUEUE_FAMILY_IGNORED;
		uint32_t size = 0;
		uint32_t global_offset = 0;    // Offset into global constant, if dynamic

		uint32_t pool_index = 0;
		BufferHandle handle;
		BufferHandle parent_buffer;

		void* mapped_data = nullptr;
		const char* name = nullptr;
	}; // struct BufferVulkan

	struct Sampler
	{
		VkSampler vk_sampler;

		uint32_t pool_index = 0;
		SamplerHandle handle;

		VkFilter min_filter = VK_FILTER_NEAREST;
		VkFilter mag_filter = VK_FILTER_NEAREST;
		VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		VkSamplerReductionMode reduction_mode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

		const char* name = nullptr;
	}; // struct SamplerVulkan

	struct Texture
	{
		VkImage vk_image;
		VkImageView vk_image_view;
		VkFormat vk_format;
		VkImageUsageFlags vk_usage;
		VmaAllocation vma_allocation;
		ResourceState state = RESOURCE_STATE_UNDEFINED;

		VkExtent3D vk_extent;
		uint16_t array_layer_count = 1;
		uint8_t mip_level_count = 1;
		uint8_t flags = 0;
		uint16_t mip_base_level = 0;    // Not 0 when texture is a view.
		uint16_t array_base_layer = 0;   // Not 0 when texture is a view.
		bool sparse = false;

		uint32_t pool_index = 0;
		TextureHandle handle;
		TextureHandle parent_texture;    // Used when a texture view.
		TextureHandle alias_texture;
		TextureType::Enum type = TextureType::Texture2D;
		QueueType::Enum queue_type = QueueType::Graphics;
		uint32_t queue_family = VK_QUEUE_FAMILY_IGNORED;
		Sampler* sampler = nullptr;

		const char* name = nullptr;
	}; // struct TextureVulkan


	struct ShaderState
	{
		VkPipelineShaderStageCreateInfo shader_stage_info[k_max_shader_stages];

		const char* name = nullptr;

		uint32_t active_shaders = 0;
		bool graphics_pipeline = false;
	}; // struct ShaderStateVulkan

	struct DescriptorSetLayout {

		VkDescriptorSetLayout vk_descriptor_set_layout;

		VkDescriptorSetLayoutBinding* vk_binding = nullptr;
		DescriptorBinding* bindings = nullptr;
		uint8_t* index_to_binding = nullptr; // Mapping between binding point and binding data.
		uint16_t num_bindings = 0;
		uint16_t set_index = 0;
		uint8_t bindless = 0;
		uint8_t dynamic = 0;

		DescriptorSetLayoutHandle handle;

	}; // struct DesciptorSetLayout


	struct DescriptorSet {

		VkDescriptorSet vk_descriptor_set;

		ResourceHandle* resources = nullptr;
		SamplerHandle* samplers = nullptr;
		uint16_t* bindings = nullptr;

		const DescriptorSetLayout* layout = nullptr;
		uint32_t num_resources = 0;
	}; // struct DesciptorSetVulkan

	struct Pipeline
	{
		VkPipeline vk_pipeline;
		VkPipelineLayout vk_pipeline_layout;

		VkPipelineBindPoint vk_bind_point;

		ShaderStateHandle shader_state;

		const DescriptorSetLayout* descriptor_set_layout[k_max_descriptor_set_layouts];
		DescriptorSetLayoutHandle descriptor_set_layout_handle[k_max_descriptor_set_layouts];
		uint32_t num_active_layouts = 0;

		DepthStencilCreation depth_stencil;
		BlendStateCreation blend_state;
		RasterizationCreation rasterization;

		PipelineHandle handle;

		bool graphics_pipeline = true;
	}; // struct pipelineVulkan

	struct RenderPass {
		VkRenderPass vk_render_pass;

		RenderPassOutput output;

		uint16_t dispatch_x = 0;
		uint16_t dispatch_y = 0;
		uint16_t dispatch_z = 0;

		uint8_t num_render_targets = 0;

		const char* name = nullptr;
	}; // struct RenderPassVulkan

	struct Framebuffer {

		// NOTE(marco): this will be a null handle if dynamic rendering is available
		VkFramebuffer vk_framebuffer;

		// NOTE(marco): cache render pass handle
		RenderPassHandle render_pass;

		uint16_t width = 0;
		uint16_t height = 0;

		float scale_x = 1.f;
		float scale_y = 1.f;

		TextureHandle color_attachments[k_max_image_outputs];
		TextureHandle depth_stencil_attachment;
		uint32_t num_color_attachments;

		uint8_t resize = 0;

		const char* name = nullptr;
	}; // struct Framebuffer

	struct ComputeLocalSize {

		uint32_t x : 10;
		uint32_t y : 10;
		uint32_t z : 10;
		uint32_t pad : 2;
	}; // struct ComputeLocalSize


	// Enum translations. Use tables or switches depending on the case. /////////
	const char* ToCompilerExtension(VkShaderStageFlagBits value);
	const char* ToStageDefines(VkShaderStageFlagBits value);

	VkImageType ToVkImageType(TextureType::Enum type);
	VkImageViewType ToVkImageViewType(TextureType::Enum type);

	VkFormat ToVkVertexFormat(VertexComponentFormat::Enum value);

	VkPipelineStageFlags ToVkPipelineStage(PipelineStage::Enum value);

	VkAccessFlags UtilToVkAccessFlags(ResourceState state);
	VkAccessFlags UtilToVkAccessFlags2(ResourceState state);

	VkImageLayout UtilToVkImageLayout(ResourceState usage);
	VkImageLayout UtilToVkImageLayout2(ResourceState usage);

	VkDescriptorBufferInfo UtilToVkDescriptorBufferInfo(Buffer* buffer);
	VkDescriptorImageInfo UtilToVkDescriptorImageInfo(Texture* texture);

	// Determines pipeline stages involved for given accesses
	VkPipelineStageFlags UtilDeterminePipelineStageFlags(VkAccessFlags access_flags, QueueType::Enum queue_type);
	VkPipelineStageFlags2KHR UtilDeterminePipelineStageFlags2(VkAccessFlags2KHR access_flags, QueueType::Enum queue_type);

	ResourceState UtilDetermineResourceState(VkDescriptorType type, VkShaderStageFlags stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS);

	VkFormat UtilStringToVkFormat(const char* format);

	const std::string GetAssetPath(const std::string& path);

}

// Hash functions for handles
namespace std {
	template<>
	struct hash<lincore::BufferHandle> {
		size_t operator()(const lincore::BufferHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::TextureHandle> {
		size_t operator()(const lincore::TextureHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::ShaderStateHandle> {
		size_t operator()(const lincore::ShaderStateHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::DescriptorSetLayoutHandle> {
		size_t operator()(const lincore::DescriptorSetLayoutHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::DescriptorSetHandle> {
		size_t operator()(const lincore::DescriptorSetHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::PipelineHandle> {
		size_t operator()(const lincore::PipelineHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::RenderPassHandle> {
		size_t operator()(const lincore::RenderPassHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::FramebufferHandle> {
		size_t operator()(const lincore::FramebufferHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::PagePoolHandle> {
		size_t operator()(const lincore::PagePoolHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::SamplerHandle> {
		size_t operator()(const lincore::SamplerHandle& h) const { return h.index; }
	};

	template<>
	struct hash<lincore::ResourceHandle> {
		size_t operator()(const lincore::ResourceHandle& h) const { return h; }
	};

}