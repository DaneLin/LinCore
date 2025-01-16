#pragma once
// lincore
#include "graphics/vk_types.h"
#include "foundation/config.h"

namespace lincore
{
	enum class CommandBufferLevel
	{
		kPrimary,
		kSecondary
	};

	struct CommandBufferInheritanceInfo
	{
		VkRect2D render_area{};
		uint32_t color_attachment_count{1};
		VkFormat *color_formats{nullptr};
		VkFormat depth_format{VK_FORMAT_UNDEFINED};
		uint32_t samples{VK_SAMPLE_COUNT_1_BIT};
		bool enable_depth{true};
		bool enable_stencil{false};
	};

	class CommandBuffer
	{
	public:
		void Init(GpuDevice *gpu_device, CommandBufferLevel level = CommandBufferLevel::kPrimary);
		void Shutdowon();

		// Recording state
		void Begin(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		void BeginSecondary(const CommandBufferInheritanceInfo &inheritance_info);
		void End();
		void Reset();

		// Dynamic rendering
		void BeginRendering(const VkRenderingInfo &render_info);
		void EndRendering();

		// Resource binding
		void BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);
		void BindVertexBuffer(VkBuffer buffer, uint32_t binding, uint32_t offset);
		void BindIndexBuffer(VkBuffer buffer, uint32_t offset, VkIndexType index_type);
		void BindDescriptorSets(VkPipelineBindPoint bind_point,
								VkPipelineLayout layout,
								uint32_t first_set,
								uint32_t set_count,
								const VkDescriptorSet *sets,
								uint32_t dynamic_offset_count = 0,
								const uint32_t *dynamic_offsets = nullptr);
		// Push constants
		void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size, const void *values);

		// Drawing commands
		void Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
		void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_intance);
		void DrawIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride);
		void DrawIndexedIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride, uint32_t count = 1);
		void ExecuteCommands(const VkCommandBuffer *secondary_cmd_bufs, uint32_t count);

		void Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

		// State setting
		void SetViewport(float x, float y, float width, float height, float min_depth = 0.f, float max_depth = 1.f);
		void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

		void Clear(float r, float g, float b, float a, uint32_t attachment_index);
		void ClearDepthStencil(float depth, uint8_t stencil);

		// Pipeline barriers
		void PipelineBarrier2(const VkDependencyInfo &dep_info);

		// Resource updates
		void CopyImageToImage(VkImage source, VkImage destination, VkExtent3D src_size, VkExtent3D dst_size);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, VkImageLayout layout, VkBufferImageCopy copy_region);
		void CopyImageToImage(Texture *src, Texture *dst);

		// Image barrier methods
		void AddImageBarrier(Texture *texture, ResourceState new_state,
							 uint32_t base_mip_level = 0, uint32_t mip_count = 1,
							 uint32_t base_array_layer = 0, uint32_t array_layer_count = 1,
							 uint32_t destination_family = VK_QUEUE_FAMILY_IGNORED,
							 QueueType::Enum destination_queue_type = QueueType::Graphics);

		void AddBufferBarrier(Buffer *buffer, ResourceState new_state,
							  uint32_t destination_family = VK_QUEUE_FAMILY_IGNORED,
							  QueueType::Enum destination_queue_type = QueueType::Graphics);

		VkCommandBuffer GetVkCommandBuffer() { return vk_command_buffer_; }
		VkCommandBuffer GetVkCommandBuffer() const { return vk_command_buffer_; }
		bool IsRecording() const { return is_recording_; }
		CommandBufferLevel GetLevel() const { return level_; }

		VkCommandBuffer vk_command_buffer_{VK_NULL_HANDLE};
		// Clear value for each attachment with depth/stencil at the end.
		VkClearValue clear_values_[kMAX_IMAGE_OUTPUT + 1];

	private:
		static const uint32_t kDepth_Stencil_Clear_Index = kMAX_IMAGE_OUTPUT;

		bool is_recording_{false};
		CommandBufferLevel level_{CommandBufferLevel::kPrimary};
		GpuDevice *gpu_device_{nullptr};

		struct
		{
			VkPipeline pipeline{VK_NULL_HANDLE};
			VkPipelineBindPoint bind_point;
			VkViewport viewport{};
			VkRect2D scissor{};
			bool is_rendering{false};
		} state_;
	};

	class CommandBufferManager
	{
	public:
		static constexpr uint32_t MAX_COMMAND_BUFFERS_PER_THREAD = 8;
		static constexpr uint32_t MAX_SECONDARY_COMMAND_BUFFERS = 16;

		void Init(GpuDevice *gpu_device, uint32_t num_threads);
		void Shutdown();

		void ResetPools(uint32_t frame_index);
		CommandBuffer *GetCommandBuffer(uint32_t frame, uint32_t thread_index, bool begin = true);
		CommandBuffer *GetSecondaryCommandBuffer(uint32_t frame, uint32_t thread_index);

		void UploadBuffer(BufferHandle staging_buffer, BufferHandle dst_buffer, const void *data, size_t size, size_t dst_offset = 0);

		void ImmediateSubmit(std::function<void(CommandBuffer *cmd)> &&function, VkQueue queue);

	private:
		GpuDevice *gpu_device_{nullptr};

		uint32_t GetPoolIndex(uint32_t frame_index, uint32_t thread_index);

		std::vector<VkCommandPool> command_pools_;
		std::vector<CommandBuffer> command_buffers_;
		std::vector<CommandBuffer> secondary_command_buffers_;
		std::vector<uint32_t> used_buffers_;
		std::vector<uint32_t> used_secondary_buffers_;
		uint32_t num_pools_per_frame{0};

		// Immediate submit resources
		VkCommandPool immediate_pool_{VK_NULL_HANDLE};
		CommandBuffer immediate_buffer_;
		VkFence immediate_fence_{VK_NULL_HANDLE};

		// Transfer queue support
		VkCommandPool transfer_pool_{VK_NULL_HANDLE};
		CommandBuffer transfer_buffer_;
		VkFence transfer_fence_{VK_NULL_HANDLE};
	};

}