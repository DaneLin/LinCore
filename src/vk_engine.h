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

#include "vk_shaders_new.h"

#include "config.h"

constexpr unsigned int FRAME_OVERLAP = 2;



struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;
	VkPipeline pipeline;
	lc::ShaderDescriptorBinder descriptorBinder;
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
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	struct DescriptorAllocatorGrowable _frameDescriptors;
};


struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metalRoughFactors;
		// padding ,we need it anyway for uniform buffers
		uint32_t colorTexID;
		uint32_t metalRoughTexID;
		uint32_t pad1;
		uint32_t pad2;
		glm::vec4 extra[13];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;
	Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
};

struct MeshNode : public Node {
	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct EngineStats {
	float frametime;
	int triangeCount;
	int drawcallCount;
	float sceneUpdateTime;
	float meshDrawTime;
};

struct TextureID {
	uint32_t Index;
};

class TextureCache {
public:
	void set_descriptor_set(VkDescriptorSet bindless_set) {
		bindless_set_ = bindless_set;
	}

	TextureID add_texture(VkDevice device, const VkImageView& image_view, VkSampler sampler) {
		// 检查是否已存在
		for (uint32_t i = 0; i < cache_.size(); i++) {
			if (cache_[i].imageView == image_view && cache_[i].sampler == sampler) {
				return TextureID{ i };
			}
		}
		// 检查是否超出最大数量
		if (cache_.size() >= MAX_BINDLESS_RESOURCES) {
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
		DescriptorWriter writer;
		writer.write_image_array(BINDLESS_TEXTURE_BINDING, idx, { image_info }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.update_set(device, bindless_set_);

		return TextureID{ idx };
	}

private:
	std::vector<VkDescriptorImageInfo> cache_;
	VkDescriptorSet bindless_set_;
};



class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool stop_rendering{ false };
	bool resize_requested{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	// swapchain stuff
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	FrameData _frames[FRAME_OVERLAP];

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator;

	// draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	DescriptorAllocatorGrowable _globalDescriptorAllocator;

	// 添加bindless相关成员
	VkDescriptorSetLayout bindless_texture_layout_{ VK_NULL_HANDLE };
	VkDescriptorSet bindless_texture_set_{ VK_NULL_HANDLE };


	VkDescriptorSet _drawImageDescriptor;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkPipelineLayout mesh_pipeline_layout_;

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	MeshRenderPass _meshRenderPass;


	GPUSceneData sceneData;

	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& image);

	struct {
		AllocatedImage white_image;
		AllocatedImage black_image;
		AllocatedImage grey_image;
		AllocatedImage error_checker_board_image;
	}default_images;

	struct {
		VkSampler linear;
		VkSampler nearest;
	}_default_samplers;

	MaterialInstance _default_data;
	GLTFMetallic_Roughness metal_rough_material;

	DrawContext mainDrawContext;

	void update_scene();

	Camera mainCamera;

	std::unordered_map < std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	EngineStats stats;

	lc::ShaderCache _shaderCache;
	TextureCache texture_cache_;


private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_background_pipelines();
	void init_imgui();
	void init_default_data();

	void resize_swapchain();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	void draw_background(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void draw_geometry(VkCommandBuffer cmd);

	const std::string get_asset_path(const std::string& path) const;
};
