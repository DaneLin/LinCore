#include "frame_graph.h"

#include "vk_engine.h"

namespace lc
{
    void FrameGraphResourceCache::Init(VulkanEngine *engine)
    {
        this->engine = engine;
        resources.reserve(FrameGraphBuilder::kMAX_RESOURCE_COUNT);
    }

    void FrameGraphResourceCache::Shutdown()
    {
        // Clean up all resources
        for (auto &[name, index] : resource_map)
        {
            FrameGraphResource &resource = resources[index];
            if (resource.resource_info.external == true)
            {
                // Not managed by the frame graph
                continue;
            }

            /*if (resource.type == FrameGraphResourceType::KTexture || resource.type == FrameGraphResourceType::kAttachment)
            {
                engine->DestroyImage(resource.resource_info.texture.image);
            }
            else if (resource.type == FrameGraphResourceType::kBuffer)
            {
                engine->DestroyBuffer(resource.resource_info.buffer.buffer);
            }*/
        }
        resources.clear();
        resource_map.clear();
    }

    void FrameGraphNodeCache::Init(VulkanEngine *engine)
    {
        this->engine = engine;
        nodes.reserve(FrameGraphBuilder::kMAX_NODES_COUNT);
    }

    void FrameGraphNodeCache::Shutdown()
    {
        for (auto &[name, index] : node_map)
        {
            FrameGraphNode &node = nodes[index];

            // Clean up node resources
            node.inputs.clear();
            node.outputs.clear();
            node.edges.clear();

            // Clean up dynamic rendering resources
            node.color_attachments.clear();
        }

        nodes.clear();
        node_map.clear();
    }

    void FrameGraphRenderPassCache::Init(VulkanEngine *engine)
    {
        this->engine = engine;
        render_pass_map.reserve(FrameGraphBuilder::kMAX_RENDER_PASS_COUNT);
    }

    void FrameGraphRenderPassCache::Shutdown()
    {
        render_pass_map.clear();
    }

    void FrameGraphBuilder::Init(VulkanEngine *engine)
    {
        this->engine = engine;
        resource_cache.Init(engine);
        node_cache.Init(engine);
        render_pass_cache.Init(engine);
    }

    void FrameGraphBuilder::Shutdown()
    {
        resource_cache.Shutdown();
        node_cache.Shutdown();
        render_pass_cache.Shutdown();
    }

    void FrameGraphBuilder::RegisterRenderPass(const char *name, FrameGraphRenderPass *render_pass)
    {
        auto it = render_pass_cache.render_pass_map.find(name);
        if (it != render_pass_cache.render_pass_map.end())
        {
            LOGE("Render pass with name {} already exists", name);
            return;
        }

        render_pass_cache.render_pass_map[name] = render_pass;

        auto node = GetNode(name);
        if (node)
        {
            node->render_pass = render_pass;
        }
    }

    FrameGraphResourceHandle FrameGraphBuilder::CreateNodeOutput(const FrameGraphResourceOutputCreation &creation, FrameGraphNodeHandle producer)
    {
        uint32_t resource_index = static_cast<uint32_t>(resource_cache.resources.size());
        resource_cache.resources.push_back({});

        FrameGraphResource &resource = resource_cache.resources[resource_index];
        resource.name = creation.name;
        resource.type = creation.type;
        resource.producer = producer;
        resource.resource_info = creation.resource_info;
        resource.output_handle = {resource_index};
        resource.ref_count = 0;

        resource_cache.resource_map[creation.name] = resource_index;

        if (!creation.resource_info.external)
        {
            // Create the resource
            if (creation.type == FrameGraphResourceType::KTexture || creation.type == FrameGraphResourceType::kAttachment)
            {
                //// Create texture
                //VkExtent3D extent{
                //    creation.resource_info.texture.width,
                //    creation.resource_info.texture.height,
                //    creation.resource_info.texture.depth};
                //resource.resource_info.texture.image = engine->CreateImage(
                //    extent,
                //    creation.resource_info.texture.format,
                //    creation.resource_info.texture.flags);
            }
            else if (creation.type == FrameGraphResourceType::kBuffer)
            {
               /* resource.resource_info.buffer.buffer = engine->CreateBuffer(
                    creation.resource_info.buffer.size,
                    creation.resource_info.buffer.flags,
                    VMA_MEMORY_USAGE_GPU_ONLY);*/
            }
        }

        return {resource_index};
    }

    FrameGraphResourceHandle FrameGraphBuilder::CreateNodeInput(const FrameGraphResourceInputCreation &creation)
    {
        uint32_t resource_index = static_cast<uint32_t>(resource_cache.resources.size());
        resource_cache.resources.push_back({});

        FrameGraphResource &resource = resource_cache.resources[resource_index];
        resource.name = creation.name;
        resource.type = creation.type;
        resource.resource_info = creation.resource_info;
        resource.producer.index = kInvalidIndex;
        resource.output_handle.index = kInvalidIndex;
        resource.ref_count = 0;

        return {resource_index};
    }

    FrameGraphNodeHandle FrameGraphBuilder::CreateNode(const FrameGraphNodeCreation &creation)
    {
        uint32_t node_index = static_cast<uint32_t>(node_cache.nodes.size());
        node_cache.nodes.push_back({});

        FrameGraphNode &node = node_cache.nodes[node_index];
        node.name = creation.name;
        node.enabled = creation.enabled;
        node.ref_count = 0;

        // Set up render pass info
        VkFormat color_format = engine->gpu_device_.draw_image_.format;
        VkFormat depth_format = engine->gpu_device_.depth_image_.format;

        node.render_pass_info.color_formats.push_back(color_format);
        node.render_pass_info.depth_format = depth_format;
        node.render_pass_info.samples = VK_SAMPLE_COUNT_1_BIT;

        node_cache.node_map[creation.name] = node_index;

        // Create outputs first
        for (const auto &output : creation.outputs)
        {
            auto output_handle = CreateNodeOutput(output, {node_index});
            node.outputs.push_back(output_handle);
        }

        // Then inputs
        for (const auto &input : creation.inputs)
        {
            auto input_handle = CreateNodeInput(input);
            node.inputs.push_back(input_handle);
        }

        return {node_index};
    }

    FrameGraphNode *FrameGraphBuilder::GetNode(const std::string &name)
    {
        auto it = node_cache.node_map.find(name);
        if (it == node_cache.node_map.end())
        {
            return nullptr;
        }
        return &node_cache.nodes[it->second];
    }

    FrameGraphNode *FrameGraphBuilder::AccessNode(FrameGraphNodeHandle handle)
    {
        if (handle.index >= node_cache.nodes.size())
        {
            return nullptr;
        }
        return &node_cache.nodes[handle.index];
    }

    FrameGraphResource *FrameGraphBuilder::GetResource(const std::string &name)
    {
		auto it = resource_cache.resource_map.find(name);
        if (it == resource_cache.resource_map.end())
        {
            return nullptr;
        }
        return &resource_cache.resources[it->second];
    }

    FrameGraphResource *FrameGraphBuilder::AccessResource(FrameGraphResourceHandle handle)
    {
        if (handle.index >= resource_cache.resources.size())
        {
            return nullptr;
        }
		return &resource_cache.resources[handle.index];
    }

    void FrameGraph::Init(FrameGraphBuilder *builder, VulkanEngine *engine)
    {
        this->builder = builder;
        this->engine = engine;
        nodes.reserve(FrameGraphBuilder::kMAX_NODES_COUNT);
    }

    void FrameGraph::Shutdown()
    {
        nodes.clear();
        all_nodes.clear();
    }

    void FrameGraph::Reset()
    {
        for (auto handle : nodes) {
            auto node = builder->AccessNode(handle);
            if (node) {
                node->edges.clear();
                node->color_attachments.clear();
            }
        }
    }

    void FrameGraph::EnableRenderPass(const std::string &render_pass_name)
    {
        auto node = builder->GetNode(render_pass_name);
        if (node) {
            node->enabled = true;
        }
    }
    void FrameGraph::DisableRenderPass(const std::string &render_pass_name)
    {
        auto node = builder->GetNode(render_pass_name);
        if (node) {
            node->enabled = false;
        }
    }
    void FrameGraph::Compile()
    {
        // Build node dependencies
        for (auto handle : nodes) {
            auto node = builder->AccessNode(handle);
            if (!node || !node->enabled) continue;
            ComputeEdges(node, handle.index); 
        }

        // Topologically sort nodes
        BuildAndSortNodes();

        // Create render pass resources
        for (auto handle : nodes) {
            auto node = builder->AccessNode(handle);
            if (!node || !node->enabled) continue;
            CreateNodeResources(node);
        }
    }

    void FrameGraph::AddUI()
    {
        for (size_t idx = 0; idx < nodes.size(); ++idx)
        {
            FrameGraphNode* node = builder->AccessNode(nodes[idx]);
            if (!node->enabled)
            {
                continue;
            }
            node->render_pass->AddUI();
        }
    }
    void FrameGraph::Render(CommandBuffer *gpu_command)
    {
        for (size_t idx = 0; idx < nodes.size(); ++idx)
        {
			FrameGraphNode* node = builder->AccessNode(nodes[idx]);
            if (!node->enabled || !node)
            {
                continue;
            }

            // set up dynamic rendering

        }
    }
    void FrameGraph::OnResize(uint32_t new_width, uint32_t new_height)
    {
        for (auto handle : nodes)
        {
            auto node = builder->AccessNode(handle);
            if (node && node->enabled && node->render_pass)
            {
				node->render_pass->OnResize(engine, new_width, new_height);
            }
        }
    }

    FrameGraphNode *FrameGraph::GetNode(const std::string &name)
    {
        return builder->GetNode(name);
    }
    FrameGraphNode *FrameGraph::AccessNode(FrameGraphNodeHandle handle)
    {
        return builder->AccessNode(handle);
    }

    FrameGraphResource* FrameGraph::GetResource(const std::string& name)
    {
        return builder->GetResource(name);
    }

    FrameGraphResource* FrameGraph::AccessResource(FrameGraphResourceHandle handle)
    {
        return builder->AccessResource(handle);
    }

    void FrameGraph::AddNode(FrameGraphNodeCreation& node)
    {
    }

    void FrameGraph::ComputeEdges(FrameGraphNode *node, uint32_t node_index)
    {
    }

    void FrameGraph::BuildAndSortNodes()
    {
    }

    bool FrameGraph::CreateNodeResources(FrameGraphNode *node)
    {
        return false;
    }

} // namespace lc