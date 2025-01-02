#pragma once
// std
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <unordered_map>
// external
#include <vk_mem_alloc.h>
// lincore
#include "foundation/gpu_enums.h"
#include "foundation/resources.h"
#include "foundation/data_structure.h"

namespace lincore
{
	class GpuDevice;

	class ResourceManager {
	public:
		ResourceManager() = default;
		~ResourceManager() = default;

		void Init(GpuDevice* gpu_device);
		void Shutdown();

		// Buffer resource creation and management
		BufferHandle CreateBuffer(const BufferCreation& info);
		Buffer* GetBuffer(BufferHandle handle);
		void DestroyBuffer(BufferHandle handle);

		// Texture resource creation and management
		TextureHandle CreateTexture(const TextureCreation& info);
		Texture* GetTexture(TextureHandle handle);
		const Texture* GetTexture(TextureHandle handle) const;
		void DestroyTexture(TextureHandle handle);

		TextureHandle CreateTextureView(const TextureViewCreation& creation);

		void UploadBuffer(BufferHandle& buffer_handle, void* buffer_data, size_t size, VkBufferUsageFlags usage, bool transfer);

		// Pipeline resource creation and management
		PipelineHandle CreatePipeline(const PipelineCreation& info, const char* cache_path = nullptr);
		void DestroyPipeline(PipelineHandle handle);

		// Sampler resource creation and management
		SamplerHandle CreateSampler(const SamplerCreation& creation);
		Sampler* GetSampler(SamplerHandle handle);
		void DestroySampler(SamplerHandle handle);

		// Descriptor Set Layout resource creation and management
		DescriptorSetLayoutHandle CreateDescriptorSetLayout(const DescriptorSetLayoutCreation& info);
		void DestroyDescriptorSetLayout(DescriptorSetLayoutHandle handle);

		// Descriptor Set resource creation and management
		DescriptorSetHandle CreateDescriptorSet(const DescriptorSetCreation& info);
		void DestroyDescriptorSet(DescriptorSetHandle handle);

		// Render Pass resource creation and management
		RenderPassHandle CreateRenderPass(const RenderPassCreation& info);
		void DestroyRenderPass(RenderPassHandle handle);

		// Framebuffer resource creation and management
		FramebufferHandle CreateFramebuffer(const FramebufferCreation& info);
		void DestroyFramebuffer(FramebufferHandle handle);

		// Shader State resource creation and management
		ShaderStateHandle CreateShaderState(const ShaderStateCreation& info);
		void DestroyShaderState(ShaderStateHandle handle);

		// 处理待删除的资源
		void ProcessPendingDeletions();

	private:
		void CreateBufferResource(BufferHandle handle);
		void CreateTextureResource(TextureHandle handle);

		bool IsViewTypeCompatible(TextureType::Enum texture_type, VkImageViewType view_type) {
			switch (texture_type) {
			case TextureType::Texture1D:
				return view_type == VK_IMAGE_VIEW_TYPE_1D ||
					view_type == VK_IMAGE_VIEW_TYPE_1D_ARRAY;
			case TextureType::Texture2D:
				return view_type == VK_IMAGE_VIEW_TYPE_2D ||
					view_type == VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			case TextureType::Texture3D:
				return view_type == VK_IMAGE_VIEW_TYPE_3D;
			case TextureType::TextureCube:
				return view_type == VK_IMAGE_VIEW_TYPE_CUBE ||
					view_type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
			case TextureType::Texture_Cube_Array:
				return view_type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
			default:
				return false;
			}
		}

		std::vector<ResourceUpdate> resource_deletion_queue_;
		std::mutex deletion_queue_mutex_;

		GpuDevice* gpu_device_{ nullptr };

		TypedResourcePool<Buffer> buffer_pool_;
		TypedResourcePool<Texture> texture_pool_;
		TypedResourcePool<Pipeline> pipeline_pool_;
		TypedResourcePool<Sampler> sampler_pool_;
		TypedResourcePool<DescriptorSetLayout> descriptor_set_layout_pool_;
		TypedResourcePool<DescriptorSet> descriptor_set_pool_;
		TypedResourcePool<RenderPass> render_pass_pool_;
		TypedResourcePool<Framebuffer> framebuffer_pool_;
		TypedResourcePool<ShaderState> shader_state_pool_;

		std::unordered_map<BufferHandle, BufferCreation> buffer_creation_infos_;
		std::unordered_map<TextureHandle, TextureCreation> texture_creation_infos_;
		std::unordered_map<PipelineHandle, PipelineCreation> pipeline_creation_infos_;
		std::unordered_map<SamplerHandle, SamplerCreation> sampler_creation_infos_;
		std::unordered_map<DescriptorSetLayoutHandle, DescriptorSetLayoutCreation> descriptor_set_layout_creation_infos_;
		std::unordered_map<DescriptorSetHandle, DescriptorSetCreation> descriptor_set_creation_infos_;
		std::unordered_map<RenderPassHandle, RenderPassCreation> render_pass_creation_infos_;
		std::unordered_map<FramebufferHandle, FramebufferCreation> framebuffer_creation_infos_;
		std::unordered_map<ShaderStateHandle, ShaderStateCreation> shader_state_creation_infos_;

		mutable std::shared_mutex creation_info_mutex_;
	};
}