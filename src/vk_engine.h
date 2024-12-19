#pragma once

#include <vk_types.h>
#include <vk_initializers.h>
// bootstrap library
#include "VkBootstrap.h"

#include <vk_descriptors.h>
#include "camera.h"
#include "vk_shaders.h"
#include "config.h"
#include "vk_loader.h"
#include "TaskScheduler.h"
#include <command_buffer.h>
#include "vk_device.h"

namespace lincore
{
	struct ComputePushConstants {
		glm::vec4 data1;
		glm::vec4 data2;
		glm::vec4 data3;
		glm::vec4 data4;
	};

	struct ComputeEffect {
		const char* name;
		VkPipeline pipeline;
		ShaderDescriptorBinder descriptor_binder;
		VkPipelineLayout layout;
		ComputePushConstants data;
	};

	struct GLTFMetallic_Roughness {
		MaterialPipeline opaque_pipeline;
		MaterialPipeline transparent_pipeline;
		VkDescriptorSetLayout material_layout;

		struct MaterialConstants {
			glm::vec4 color_factors;
			glm::vec4 metal_rough_factors;
			// padding ,we need it anyway for uniform buffers
			uint32_t color_tex_id;
			uint32_t metal_rought_tex_id;
			uint32_t pad1;
			uint32_t pad2;
			glm::vec4 extra[13];
		};

		struct MaterialResources {
			TextureHandle color_image;
			VkSampler color_sampler;
			TextureHandle metal_rough_image;
			VkSampler metal_rought_sampler;
			BufferHandle data_buffer;
			uint32_t data_buffer_offset;
		};
		VulkanEngine* engine;

		DescriptorWriter writer;

		void BuildPipelines(VulkanEngine* engine);

		void ClearResources();

		MaterialInstance WriteMaterial(MeshPassType pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptor_allocator);
	};

	struct RenderObject {
		uint32_t index_count;

		uint32_t first_index;

		//VkBuffer index_buffer;
		BufferHandle index_buffer_handle;

		size_t indirect_draw_index;

		MaterialInstance* material;

		Bounds bounds;

		glm::mat4 transform;

		VkDeviceAddress vertex_buffer_address;
	};

	struct DrawContext {
		std::vector<RenderObject> opaque_surfaces;

		std::vector<RenderObject> transparent_surfaces;
	};

	struct MeshNode : public Node {
		std::shared_ptr<MeshAsset> mesh;

		virtual void Draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
	};

	struct GlobalMeshBuffer {
		BufferHandle vertex_buffer_handle;
		BufferHandle index_buffer_handle;
		BufferHandle indirect_command_buffer_handle;

		std::vector<Vertex> vertex_data;
		std::vector<uint32_t> index_data;
		std::vector<VkDrawIndexedIndirectCommand> indirect_commands;

		void UploadToGPU(VulkanEngine* engine);
	};

	class GpuDevice;  // Forward declaration

	struct EngineStats {
		float frame_time;

		int triangle_count;

		int drawcall_count;

		float scene_update_time;

		float mesh_draw_time;
	};

	struct RenderInfo
	{
		MaterialPipeline* last_pipeline = nullptr;
		MaterialInstance* last_material = nullptr;
		VkBuffer last_index_buffer = VK_NULL_HANDLE;

		void Clear()
		{
			last_pipeline = nullptr;
			last_material = nullptr;
			last_index_buffer = VK_NULL_HANDLE;
		}
	};

	class VulkanEngine {
	public:

		bool is_initialized_{ false };

		bool freeze_rendering_{ false };
		bool resize_requested_{ false };

		VkExtent2D window_extent_{ 1700 , 900 };

		struct SDL_Window* window_{ nullptr };

		DeletionQueue main_deletion_queue_;

		VkPipelineLayout mesh_pipeline_layout_;

		std::vector<ComputeEffect> background_effects_;
		int current_background_effect_{ 0 };
		int current_scene_{ 0 };

		MaterialInstance dafault_data_;

		GLTFMetallic_Roughness metal_rough_material_;

		DrawContext main_draw_context_;

		Camera main_camera_;

		std::unordered_map <std::string, std::shared_ptr<LoadedGLTF>> loaded_scenes_;

		EngineStats engine_stats_;

		ShaderCache shader_cache_;

		enki::TaskScheduler render_task_scheduler_;
		enki::TaskScheduler io_task_scheduler_;
		RunPinnedTaskLoopTask run_pinned_task;
		AsyncLoadTask async_load_task;
		AsyncLoader async_loader_;

		GlobalMeshBuffer global_mesh_buffer_;

		GpuDevice gpu_device_;

	public:
		static VulkanEngine& Get();
		//initializes everything in the engine
		void Init();
		//shuts down the engine
		void CleanUp();
		//draw loop
		void Draw();
		//run main loop
		void Run();

		GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
		void UpdateScene();
		void DrawObject(CommandBuffer* cmd, const RenderObject& r, RenderInfo& render_info);

	private:
		void InitPipelines();
		void InitBackgounrdPipelines();
		void InitImGui();
		void InitDefaultData();
		void InitTaskSystem();
		void DrawBackground(CommandBuffer* cmd);
		void DrawImGui(CommandBuffer* cmd, VkImageView target_image_view);
		void DrawGeometry(CommandBuffer* cmd);
		const std::string GetAssetPath(const std::string& path) const;
	};

}