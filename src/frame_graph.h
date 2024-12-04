#pragma once

#include <vk_types.h>

class VulkanEngine;
class CommandBuffer;

namespace RenderPassOperation
{
	enum Enum
	{
		kLoad,
		kStore,
		kClear,
		kDontCare
	};
}

namespace lc
{
	typedef uint32_t FrameGraphHandle;

	struct FrameGraphResourceHandle
	{
		FrameGraphHandle index;
	}; // struct FrameGraphResourceHandle

	struct FrameGraphNodeHandle
	{
		FrameGraphHandle index;
	}; // struct FrameGraphNodeHandle

	enum FrameGraphResourceType
	{
		kInvalied = -1,
		kBuffer = 0,
		KTexture = 1,
		kAttachment = 2,
		kReference =3
	}; // enum FrameGraphResourceType

	struct FrameGraphResourceInfo
	{
		bool external = false;

		union 
		{
			struct
			{
				size_t size;
           		VkBufferUsageFlags flags;
				AllocatedBufferUntyped buffer;
			} buffer;

			struct 
			{
				uint32_t width;
				uint32_t height;
				uint32_t depth;

				VkFormat format;
            	VkImageUsageFlags flags;
				RenderPassOperation::Enum load_op;
				AllocatedImage image;
			} texture;
		};
		
	}; // struct FrameGraphResourceInfo

	// NOTE(marco): an input could be used as a texture or as an attachment.
	// If it's an attachment we want to control whether to discard previous
	// content - for instance the first time we use it - or to load the data
	// from a previous pass
	// NOTE(marco): an output always implies an attachment and a store op
	struct FrameGraphResource
	{
		FrameGraphResourceType type; 
		FrameGraphResourceInfo resource_info;

		FrameGraphNodeHandle producer;
		FrameGraphResourceHandle output_handle;

		int32_t ref_count = 0;
		const char* name = nullptr;
	}; // struct FrameGraphResource

	struct FrameGraphResourceInputCreation
	{
		FrameGraphResourceType type;
		FrameGraphResourceInfo resource_info;

		const char* name;
	}; // struct FrameGraphResourceInputCreation

	struct FrameGraphResourceOutputCreation
	{
		FrameGraphResourceType type;
		FrameGraphResourceInfo resource_info;

		const char* name;
	}; // struct FrameGraphResourceOutputCreation

	struct FrameGraphNodeCreation
	{
		std::vector<FrameGraphResourceInputCreation> inputs;
		std::vector<FrameGraphResourceOutputCreation> outputs;

		bool enabled;

		const char* name;
	}; // struct FrameGraphNodeCreation

	struct FrameGraphRenderPassInfo
	{
		std::vector<VkFormat> color_formats;
		VkFormat depth_format{VK_FORMAT_UNDEFINED};
		uint32_t samples{VK_SAMPLE_COUNT_1_BIT};
	}; // struct FrameGraphRenderPassInfo

	struct FrameGraphRenderPass
	{
		virtual void AddUI() {};
		virtual void PreRender(CommandBuffer* gpu_command) {};
		virtual void Render(CommandBuffer* gpu_command) {};
		virtual void OnResize(VulkanEngine* engine, uint32_t new_width, uint32_t new_height) {};
	}; // struct FrameGraphRenderPass

	struct FrameGraphNode
	{
		uint32_t ref_count = 0;

		FrameGraphRenderPassInfo render_pass_info;
		std::vector<VkRenderingAttachmentInfo> color_attachments;
		VkRenderingAttachmentInfo depth_attachment;

		FrameGraphRenderPass* render_pass;

		std::vector<FrameGraphResourceHandle> inputs;
		std::vector<FrameGraphResourceHandle> outputs;
		std::vector<FrameGraphNodeHandle> edges;

		bool enabled = true;
		const char* name = nullptr;
	}; // struct FrameGraphNode

	struct FrameGraphResourceCache
	{
		void Init(VulkanEngine* engine);
		void Shutdown();

		VulkanEngine* engine;

		std::unordered_map<std::string, uint32_t> resource_map;
		std::vector<FrameGraphResource> resources;
	}; // struct FrameGraphResourceCache

	struct FrameGraphNodeCache
	{
		void Init(VulkanEngine* engine);
		void Shutdown();

		VulkanEngine* engine;

		std::unordered_map<std::string, uint32_t> node_map;
		std::vector<FrameGraphNode> nodes;
	}; // struct FrameGraphNodeCache

	struct FrameGraphRenderPassCache
	{
		void Init(VulkanEngine* engine);
		void Shutdown();

		VulkanEngine* engine;
		std::unordered_map<std::string, FrameGraphRenderPass*> render_pass_map;
	}; // struct FrameGraphRenderPassCache

	struct FrameGraphBuilder
	{
		void Init(VulkanEngine* engine);
		void Shutdown();

		void RegisterRenderPass(const char* name, FrameGraphRenderPass* render_pass);

		FrameGraphResourceHandle CreateNodeOutput(const FrameGraphResourceOutputCreation& creation, FrameGraphNodeHandle producer);
		FrameGraphResourceHandle CreateNodeInput(const FrameGraphResourceInputCreation& creation);
		FrameGraphNodeHandle CreateNode(const FrameGraphNodeCreation& creation);

		FrameGraphNode* GetNode(const std::string& name);
		FrameGraphNode* AccessNode(FrameGraphNodeHandle handle);

		FrameGraphResource* GetResource(const std::string& name);
    	FrameGraphResource* AccessResource(FrameGraphResourceHandle handle);

		FrameGraphResourceCache resource_cache;
		FrameGraphNodeCache node_cache;
		FrameGraphRenderPassCache render_pass_cache;
		
		VulkanEngine* engine;

		static constexpr uint32_t kMAX_RENDER_PASS_COUNT = 256;
		static constexpr uint32_t kMAX_RESOURCE_COUNT = 1024;
		static constexpr uint32_t kMAX_NODES_COUNT = 1024;

		const std::string kNAME = "FrameGraphBuilder";
	}; // struct FrameGraphBuilder

	class FrameGraph
	{
	public:
		void Init(FrameGraphBuilder* builder, VulkanEngine* engine);
		void Shutdown();

		void Reset();
		void EnableRenderPass(const std::string& render_pass_name);
		void DisableRenderPass(const std::string& render_pass_name);
		void Compile();

		void AddUI();
		void Render(CommandBuffer* gpu_command);
		void OnResize(uint32_t new_width, uint32_t new_height);

		FrameGraphNode* GetNode(const std::string& name);
		FrameGraphNode* AccessNode(FrameGraphNodeHandle handle);

		std::vector<FrameGraphNodeHandle> nodes;
		FrameGraphBuilder* builder{nullptr};

		std::string name;

	private:
		void ComputeEdges(FrameGraphNode* node, uint32_t node_index);
		void BuildAndSortNodes();
		bool CreateNodeResources(FrameGraphNode* node);
	private:

		VulkanEngine* engine;
		
	}; // class FrameGraph

} // namespace lc