// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_initializers.h>

// bootstrap library
#include "VkBootstrap.h"

#include <vk_descriptors.h>

#include "camera.h"

#include "vk_shaders.h"

#include "config.h"
#include "vk_pipelines.h"
#include "vk_loader.h"
#include "TaskScheduler.h"
#include <command_buffer.h>\

namespace vkutils {
	class VulkanProfiler;
}

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;
	VkPipeline pipeline;
	lc::ShaderDescriptorBinder descriptor_binder;
	VkPipelineLayout layout;
	ComputePushConstants data;
};


struct FrameData {
	/*VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;*/
	VkSemaphore swapchain_semaphore, render_semaphore;
	VkFence render_fence;
	DeletionQueue deletion_queue;
	class lc::DescriptorAllocatorGrowable frame_descriptors;
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
		AllocatedImage color_image;
		VkSampler color_sampler;
		AllocatedImage metal_rough_image;
		VkSampler metal_rought_sampler;
		VkBuffer data_buffer;
		uint32_t data_buffer_offset;
	};

	lc::DescriptorWriter writer;

	void BuildPipelines(VulkanEngine* engine);

	void ClearResources(VkDevice device);

	MaterialInstance WriteMaterial(VkDevice device, MeshPassType pass, const MaterialResources& resources, lc::DescriptorAllocatorGrowable& descriptor_allocator);
};

struct RenderObject {
	uint32_t index_count;

	uint32_t first_index;

	//VkBuffer index_buffer;
	BufferHandle index_buffer_handle;

	size_t indirect_draw_index;

	MaterialInstance* material;

	lc::Bounds bounds;
	
	glm::mat4 transform;

	VkDeviceAddress vertex_buffer_address;
};

struct DrawContext {
	std::vector<RenderObject> opaque_surfaces;

	std::vector<RenderObject> transparent_surfaces;
};

struct MeshNode : public Node {
	std::shared_ptr<lc::MeshAsset> mesh;

	virtual void Draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
};

struct GlobalMeshBuffer {
	/*AllocatedBuffer<Vertex> vertex_buffer;
	AllocatedBuffer<uint32_t> index_buffer;
	AllocatedBuffer<VkDrawIndexedIndirectCommand> indirect_command_buffer;*/
	BufferHandle vertex_buffer_handle;
	BufferHandle index_buffer_handle;
	BufferHandle indirect_command_buffer_handle;

	std::vector<Vertex> vertex_data;
	std::vector<uint32_t> index_data;
	std::vector<VkDrawIndexedIndirectCommand> indirect_commands;

	void UploadToGPU(VulkanEngine* engine);
};



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

	int frame_number_{ 0 };

	bool freeze_rendering_{ false };

	bool resize_requested_{ false };

	VkExtent2D window_extent_{ 1700 , 900 };

	struct SDL_Window* window_{ nullptr };

	VkInstance instance_;

	VkDebugUtilsMessengerEXT debug_messenger_;

	VkPhysicalDevice chosen_gpu_;

	VkPhysicalDeviceProperties gpu_properties_;

	VkDevice device_;

	VkSurfaceKHR surface_;

	FrameData frames_[kFRAME_OVERLAP];

	VkQueue main_queue_;
	VkQueue transfer_queue_;
	uint32_t main_queue_family_;
	uint32_t transfer_queue_family_;

	DeletionQueue main_deletion_queue_;

	VmaAllocator allocator_;

	// draw resources
	AllocatedImage draw_image_;
	AllocatedImage depth_image_;
	VkExtent2D draw_extent_;
	float render_scale_ = 1.f;

	lc::DescriptorAllocatorGrowable global_descriptor_allocator_;

	// 添加bindless相关成员
	VkDescriptorSetLayout bindless_texture_layout_{ VK_NULL_HANDLE };
	VkDescriptorSet bindless_texture_set_{ VK_NULL_HANDLE };
	
	VkDescriptorSetLayout draw_image_descriptor_layout_{ VK_NULL_HANDLE };;
	VkDescriptorSet draw_image_descriptor_{ VK_NULL_HANDLE };

	VkDescriptorSetLayout gpu_scene_data_descriptor_layout_;

	VkPipelineLayout mesh_pipeline_layout_;

	std::vector<ComputeEffect> background_effects_;
	int current_background_effect_{ 0 };
	int current_scene_{ 0 };

	GPUSceneData scene_data_;

	struct {
		TextureHandle white_image;
		TextureHandle black_image;
		TextureHandle grey_image;
		TextureHandle error_checker_board_image;
	}default_images_;

	struct {
		VkSampler linear;
		VkSampler nearest;
	}default_samplers_;

	MaterialInstance dafault_data_;

	GLTFMetallic_Roughness metal_rough_material_;

	DrawContext main_draw_context_;

	Camera main_camera_;

	std::unordered_map <std::string, std::shared_ptr<lc::LoadedGLTF>> loaded_scenes_;

	EngineStats engine_stats_;

	lc::ShaderCache shader_cache_;
	lc::TextureCache texture_cache_;
	lc::PipelineCache* global_pipeline_cache_;

	vkutils::VulkanProfiler* profiler_;

	enki::TaskScheduler render_task_scheduler_;
	enki::TaskScheduler io_task_scheduler_;
	lc::RunPinnedTaskLoopTask run_pinned_task;
	lc::AsyncLoadTask async_load_task;
	lc::AsyncLoader async_loader_;

	GlobalMeshBuffer global_mesh_buffer_;

	CommandBufferManager command_buffer_manager_;
	ResourceManager resource_manager_;

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

	FrameData& GetCurrentFrame() { return frames_[frame_number_ % kFRAME_OVERLAP]; }

	GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	void UpdateScene();

	void DrawObject(CommandBuffer* cmd, const RenderObject& r, RenderInfo& render_info);

private:
	void InitVulkan();

	void InitSwapchain();

	void InitCommands();

	void InitSyncStructures();

	void InitDescriptors();

	void InitPipelines();

	void InitBackgounrdPipelines();

	void InitImGui();

	void InitDefaultData();

	void InitTaskSystem();

	void ResizeSwapchain();

	void CreateSwapchain(uint32_t width, uint32_t height);

	void DestroySwapchain();

	void DrawBackground(CommandBuffer* cmd);

	void DrawImGui(CommandBuffer* cmd, VkImageView target_image_view);

	void DrawGeometry(CommandBuffer* cmd);

	const std::string GetAssetPath(const std::string& path) const;

	
private:
	// swapchain stuff
	VkSwapchainKHR swapchain_;

	VkFormat swapchain_image_format_;

	std::vector<VkImage> swapchain_images_;

	std::vector<VkImageView> swapchain_image_views_;

	VkExtent2D swapchain_extent_;
	
};
