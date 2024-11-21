// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_initializers.h>

// bootstrap library
#include "VkBootstrap.h"

#include <vk_descriptors.h>
#include <vk_loader.h>

#include "camera.h"

#include "vk_shaders.h"

#include "config.h"
#include "vk_pipelines.h"

constexpr unsigned int kFRAME_OVERLAP = 2;

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

struct MeshRenderPass
{
	const char* name;

	VkPipeline pipeline;

	VkPipelineLayout layout;
};


struct FrameData {
	VkCommandPool command_pool;

	VkCommandBuffer main_command_buffer;

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

	MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, lc::DescriptorAllocatorGrowable& descriptor_allocator);
};

struct RenderObject {
	uint32_t index_count;

	uint32_t first_index;

	VkBuffer index_buffer;

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

struct EngineStats {
	float frame_time;

	int triangle_count;

	int drawcall_count;

	float scene_update_time;

	float mesh_draw_time;
};

struct TextureID {
	uint32_t index;
};

class TextureCache {
public:
	void SetDescriptorSet(VkDescriptorSet bindless_set) {
		bindless_set_ = bindless_set;
	}

	TextureID AddTexture(VkDevice device, const VkImageView& image_view, VkSampler sampler) {
		// 检查是否已存在
		for (uint32_t i = 0; i < cache_.size(); i++) {
			if (cache_[i].imageView == image_view && cache_[i].sampler == sampler) {
				return TextureID{ i };
			}
		}
		// 检查是否超出最大数量
		if (cache_.size() >= kMAX_BINDLESS_RESOURCES) {
			throw std::runtime_error("Exceeded maximum bindless texture count");
		}

		uint32_t idx = static_cast<uint32_t>(cache_.size());

		// 添加到缓存
		VkDescriptorImageInfo image_info{
			.sampler = sampler,
			.imageView = image_view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		cache_.push_back(image_info);

		// 更新bindless descriptor set
		lc::DescriptorWriter writer;
		writer.WriteImageArray(kBINDLESS_TEXTURE_BINDING, idx, { image_info }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.UpdateSet(device, bindless_set_);

		return TextureID{ idx };
	}

private:
	std::vector<VkDescriptorImageInfo> cache_;
	VkDescriptorSet bindless_set_;
};

class VulkanEngine {
public:

	bool is_initialized_{ false };

	int frame_number{ 0 };

	bool freeze_rendering_{ false };

	bool resize_requested_{ false };

	VkExtent2D window_extent_{ 1700 , 900 };

	struct SDL_Window* window_{ nullptr };

	VkInstance instance_;

	VkDebugUtilsMessengerEXT debug_messenger_;

	VkPhysicalDevice chosen_gpu_;

	VkDevice device_;

	VkSurfaceKHR surface_;

	FrameData frames_[kFRAME_OVERLAP];

	VkQueue graphics_queue_;

	uint32_t graphics_queue_family;

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

	VkDescriptorSet draw_image_descriptor_;

	VkDescriptorSetLayout draw_image_descriptor_layout_;

	VkPipelineLayout mesh_pipeline_layout_;

	std::vector<ComputeEffect> background_effects_;

	int current_background_effect_{ 0 };

	MeshRenderPass mesh_renderpass_;

	GPUSceneData scene_data_;

	VkDescriptorSetLayout gpu_scene_data_descriptor_layout_;
	
	struct {
		AllocatedImage white_image;
		AllocatedImage black_image;
		AllocatedImage grey_image;
		AllocatedImage error_checker_board_image;
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

	TextureCache texture_cache_;

	lc::PipelineCache* global_pipeline_cache_;

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

	FrameData& GetCurrentFrame() { return frames_[frame_number % kFRAME_OVERLAP]; }

	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void DestroyBuffer(const AllocatedBuffer& buffer);

	GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	void DestroyImage(const AllocatedImage& image);

	void UpdateScene();

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

	void ResizeSwapchain();

	void CreateSwapchain(uint32_t width, uint32_t height);

	void DestroySwapchain();

	void DrawBackground(VkCommandBuffer cmd);

	void DrawImGui(VkCommandBuffer cmd, VkImageView target_image_view);

	void DrawGeometry(VkCommandBuffer cmd);

	const std::string GetAssetPath(const std::string& path) const;

private:
	// swapchain stuff
	VkSwapchainKHR swapchain_;

	VkFormat swapchain_image_format_;

	std::vector<VkImage> swapchain_images_;

	std::vector<VkImageView> swapchain_image_views_;

	VkExtent2D swapchain_extent_;

	// immediate submit structures
	VkFence imm_fence_;

	VkCommandBuffer imm_command_buffer_;

	VkCommandPool imm_command_pool_;
};
