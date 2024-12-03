#pragma once

#include <vk_types.h>

enum class CommandBufferLevel
{
	kPrimary,
	kSecondary
};

struct CommandBufferInheritanceInfo
{
	VkRect2D render_area{};
	uint32_t color_attachment_count{ 1 };
	VkFormat* color_formats{ nullptr };
	VkFormat depth_format{ VK_FORMAT_UNDEFINED };
	uint32_t samples{ VK_SAMPLE_COUNT_1_BIT };
	bool enable_depth{ true };
	bool enable_stencil{ false };
};

class CommandBuffer
{
public:

	void Init(CommandBufferLevel level = CommandBufferLevel::kPrimary);
	void Shutdowon();

	// Recording state
	void Begin(VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	void BeginSecondary(const CommandBufferInheritanceInfo& inheritance_info);
	void End();
	void Reset();

	// Dynamic rendering
	void BeginRendering(const VkRenderingInfo& render_info);
	void EndRendering();

	// Resource binding
	void BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS);
	void BindVertexBuffer(VkBuffer buffer, uint32_t binding, uint32_t offset);
	void BindIndexBuffer(VkBuffer buffer, uint32_t offset, VkIndexType index_type);
	void BindDescriptorSets(VkPipelineBindPoint bind_point,
		VkPipelineLayout layout,
		uint32_t first_set,
		uint32_t set_count,
		const VkDescriptorSet* sets,
		uint32_t dynamic_offset_count = 0,
		const uint32_t* dynamic_offsets = nullptr);

	// Drawing commands
	void Draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
	void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_intance);
	void DrawIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride);
	void DrawIndexedIndirect(VkBuffer buffer, uint32_t offset, uint32_t stride, uint32_t count = 1);
	void ExecuteCommands(const VkCommandBuffer* secondary_cmd_bufs, uint32_t count);

	void Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

	// State setting
	void SetViewport(float x, float y, float width, float height, float min_depth = 0.f, float max_depth = 1.f);
	void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

	// Pipeline barriers
	void PipelineBarrier2(const VkDependencyInfo& dep_info);
	
	void UploadTextureData(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);

	// Resource updates
	void TransitionImage(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
	void CopyImageToImage( VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size);
	void GenerateMipmaps(VkImage image, VkExtent2D image_size);

	// Push constants
	void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stage_flags, uint32_t offset, uint32_t size, const void* values);

	VkCommandBuffer GetCommandBuffer() const { return command_buffer_; }
	bool IsRecording() const { return is_recording_; }
	CommandBufferLevel GetLevel() const { return level_; }


	VkCommandBuffer command_buffer_{ VK_NULL_HANDLE };

private:
	
	bool is_recording_{ false };
	CommandBufferLevel level_{ CommandBufferLevel::kPrimary };

	struct {
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineBindPoint bind_point;
		VkViewport viewport{};
		VkRect2D scissor{};
		bool is_rendering{ false };
	}state_;
};

class CommandBufferManager
{
public:

	void Init(uint32_t num_threads);
	void Shutdown();

	void ResetPools(uint32_t frame_index);
	CommandBuffer* GetCommandBuffer(uint32_t frame, uint32_t thread_index, bool begin = true);
	CommandBuffer* GetSecondaryCommandBuffer(uint32_t frame, uint32_t thread_index);

	void ImmediateSubmit(std::function<void(CommandBuffer* cmd)>&& function, VkQueue queue);

private:
	static constexpr uint32_t MAX_COMMAND_BUFFERS_PER_THREAD = 8;
	static constexpr uint32_t MAX_SECONDARY_COMMAND_BUFFERS = 16;
	uint32_t GetPoolIndex(uint32_t frame_index, uint32_t thread_index);

	std::vector<VkCommandPool> command_pools_;
	std::vector<CommandBuffer> command_buffers_;
	std::vector<CommandBuffer> secondary_command_buffers_;
	std::vector<uint32_t> used_buffers_;
	std::vector<uint32_t> used_secondary_buffers_;
	uint32_t num_pools_per_frame{ 0 };

	// Immediate submit resources
	VkCommandPool immediate_pool_{ VK_NULL_HANDLE };
	CommandBuffer immediate_buffer_;
	VkFence immediate_fence_{ VK_NULL_HANDLE };

	// Transfer queue support
	VkCommandPool transfer_pool_{ VK_NULL_HANDLE };
	CommandBuffer transfer_buffer_;
	VkFence transfer_fence_{ VK_NULL_HANDLE };
};