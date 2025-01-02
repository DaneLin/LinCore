#include "graphics/vk_pipelines.h"
// std
#include <fstream>
// lincore
#include "foundation/logging.h"
#include "graphics/vk_initializers.h"
#include "graphics/vk_engine.h"

namespace lincore
{
	void PipelineBuilder::Clear()
	{
		input_assembly_ = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		rasterizer_ = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		color_blend_attachment_ = {};
		multisampling_ = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
		pipeline_layout_ = {};
		depth_stencil_ = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
		render_info_ = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
		shader_stages_.clear();
	}

	VkPipeline PipelineBuilder::BuildPipeline(VkDevice device, VkPipelineCache cache)
	{
		// make viewport state from our stored viewport and scissor
		VkPipelineViewportStateCreateInfo viewport_state{};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.pNext = nullptr;

		viewport_state.viewportCount = 1;
		viewport_state.scissorCount = 1;

		// setup dummy color blending. We aren't using transparent objects yet so this is fine
		VkPipelineColorBlendStateCreateInfo color_blending{};
		color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending.pNext = nullptr;

		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment_;

		// completely clear vertexinputstatecreateinfo, as we have no need for it
		VkPipelineVertexInputStateCreateInfo vertex_input_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

		// build the actual pipeline
		VkGraphicsPipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
		// connect the renderinfo to the pNext extension mechanism
		pipeline_info.pNext = &render_info_;

		pipeline_info.stageCount = static_cast<uint32_t>(shader_stages_.size());
		pipeline_info.pStages = shader_stages_.data();
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly_;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasterizer_;
		pipeline_info.pMultisampleState = &multisampling_;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDepthStencilState = &depth_stencil_;
		pipeline_info.layout = pipeline_layout_;
		// setting up dynamic state
		VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

		VkPipelineDynamicStateCreateInfo dynamic_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		dynamic_state.pDynamicStates = &state[0];
		dynamic_state.dynamicStateCount = 2;

		pipeline_info.pDynamicState = &dynamic_state;

		VkPipeline new_pipeline;
		if (vkCreateGraphicsPipelines(device, cache, 1, &pipeline_info, nullptr, &new_pipeline) != VK_SUCCESS)
		{
			fmt::print("failed to create pipeline\n");
			return VK_NULL_HANDLE;
		}
		else
		{
			return new_pipeline;
		}
	}

	void PipelineBuilder::ApplyConfig(const PipelineStateConfig &config)
	{
		SetInputTopology(config.topology);
		SetPolygonMode(config.polygon_mode);
		SetCullMode(config.cull_mode, config.front_face);

		if (config.depth_test)
		{
			EnableDepthtest(config.depth_write, config.depth_compare_op);
		}
		else
		{
			DisableDepthtest();
		}

		switch (config.blend_mode)
		{
		case PipelineStateConfig::BlendMode::None:
			DisableBlending();
			break;
		case PipelineStateConfig::BlendMode::Additive:
			EnableBlendingAdditive();
			break;
		case PipelineStateConfig::BlendMode::AlphaBlend:
			EnableBlendingAlphablend();
			break;
		}
	}

	void PipelineBuilder::SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
	{
		shader_stages_.clear();

		shader_stages_.push_back(
			vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

		shader_stages_.push_back(
			vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
	}

	void PipelineBuilder::SetShaders(ShaderEffect *effect)
	{
		shader_stages_.clear();
		effect->FillStage(shader_stages_);

		pipeline_layout_ = effect->built_layout_;
	}

	void PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
	{
		input_assembly_.topology = topology;

		input_assembly_.primitiveRestartEnable = VK_FALSE;
	}

	void PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
	{
		rasterizer_.polygonMode = mode;
		rasterizer_.lineWidth = 1.0f;
	}

	void PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
	{
		rasterizer_.cullMode = cullMode;
		rasterizer_.frontFace = frontFace;
	}

	void PipelineBuilder::SetMultisamplingNone()
	{
		multisampling_.sampleShadingEnable = VK_FALSE;
		// multisampling defaulted to no multisampling (1 sample per pixel)
		multisampling_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling_.minSampleShading = 1.0f;
		multisampling_.pSampleMask = nullptr;
		// no alpha to coverage either
		multisampling_.alphaToCoverageEnable = VK_FALSE;
		multisampling_.alphaToOneEnable = VK_FALSE;
	}

	void PipelineBuilder::DisableBlending()
	{
		// default write mask
		color_blend_attachment_.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		// no blending
		color_blend_attachment_.blendEnable = VK_FALSE;
	}

	void PipelineBuilder::SetColorAttachmentFormat(VkFormat format)
	{
		color_attachment_format_ = format;

		render_info_.colorAttachmentCount = 1;
		render_info_.pColorAttachmentFormats = &color_attachment_format_;
	}

	void PipelineBuilder::SetDepthFormat(VkFormat format)
	{
		render_info_.depthAttachmentFormat = format;
	}

	void PipelineBuilder::DisableDepthtest()
	{
		depth_stencil_.depthTestEnable = VK_FALSE;
		depth_stencil_.depthWriteEnable = VK_FALSE;
		depth_stencil_.depthCompareOp = VK_COMPARE_OP_NEVER;
		depth_stencil_.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_.stencilTestEnable = VK_FALSE;
		depth_stencil_.front = {};
		depth_stencil_.back = {};
		depth_stencil_.minDepthBounds = 0.f;
		depth_stencil_.maxDepthBounds = 1.f;
	}

	void PipelineBuilder::EnableDepthtest(bool depthWriteEnable, VkCompareOp op)
	{
		depth_stencil_.depthTestEnable = VK_TRUE;
		depth_stencil_.depthWriteEnable = depthWriteEnable;
		depth_stencil_.depthCompareOp = op;
		depth_stencil_.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_.stencilTestEnable = VK_FALSE;
		depth_stencil_.front = {};
		depth_stencil_.back = {};
		depth_stencil_.minDepthBounds = 0.f;
		depth_stencil_.maxDepthBounds = 1.f;
	}

	void PipelineBuilder::EnableBlendingAdditive()
	{
		color_blend_attachment_.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment_.blendEnable = VK_TRUE;
		color_blend_attachment_.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	// outColor = srcColor * srcColorBlendFactor <op> dstColor * dstColorBlendFactor;

	void PipelineBuilder::EnableBlendingAlphablend()
	{
		color_blend_attachment_.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment_.blendEnable = VK_TRUE;
		color_blend_attachment_.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment_.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	void PipelineCache::Init(VkDevice device, const std::string cache_file_path)
	{
		device_ = device;
		cache_file_path_ = cache_file_path;

		if (!LoadPipelineCache())
		{
			LOGI("Creating new pipeline cache");
			CreatePipelineCache();
		}
		else
		{
			LOGI("Loaded pipeline cache from file: {}", cache_file_path_);
		}
	}

	void PipelineCache::CleanUp()
	{
		SaveCache();
		vkDestroyPipelineCache(device_, cache_, nullptr);
	}

	void PipelineCache::SaveCache()
	{
		size_t cache_size = 0;
		VkResult result = vkGetPipelineCacheData(device_, cache_, &cache_size, nullptr);
		if (result != VK_SUCCESS || cache_size == 0)
		{
			return;
		}

		std::vector<char> cache_data(cache_size);
		result = vkGetPipelineCacheData(device_, cache_, &cache_size, cache_data.data());
		if (result != VK_SUCCESS)
		{
			return;
		}

		// 将缓存数据写入到文件
		std::ofstream file(cache_file_path_, std::ios::binary);
		if (file.is_open())
		{
			file.write(cache_data.data(), cache_data.size());
		}

		file.close();
		LOGI("Saved pipeline cache to: {}", cache_file_path_);
	}

	bool PipelineCache::LoadPipelineCache()
	{
		std::ifstream file(cache_file_path_, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			LOGI("Failed to open pipeline cache file: {}", cache_file_path_);
			return false;
		}

		// 获取文件大小并读取数据
		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		std::vector<char> cacheData(size);
		if (!file.read(cacheData.data(), size))
		{
			return false;
		}

		// 使用缓存数据创建 Pipeline Cache
		VkPipelineCacheCreateInfo cache_create_info = {};
		cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cache_create_info.initialDataSize = cacheData.size();
		cache_create_info.pInitialData = cacheData.data();

		VkResult result = vkCreatePipelineCache(device_, &cache_create_info, nullptr, &cache_);
		return result == VK_SUCCESS;
	}

	void PipelineCache::CreatePipelineCache()
	{
		VkPipelineCacheCreateInfo cache_create_info = {};
		cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cache_create_info.pNext = nullptr;
		cache_create_info.flags = 0;

		VK_CHECK(vkCreatePipelineCache(device_, &cache_create_info, nullptr, &cache_));
	}

	void ComputePipelineBuilder::Clear()
	{
		shader_stages_.clear();
	}

	VkPipeline ComputePipelineBuilder::BuildPipeline(VkDevice device, VkPipelineLayout layout, VkPipelineCache cache)
	{
		if (shader_stages_.empty() || shader_stages_[0].stage != VK_SHADER_STAGE_COMPUTE_BIT)
		{
			LOGE("Invalid compute shader configuration");
			return VK_NULL_HANDLE;
		}
		VkComputePipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_info.stage = shader_stages_[0];
		pipeline_info.layout = layout;

		VkPipeline pipeline;
		VK_CHECK(vkCreateComputePipelines(device, cache, 1, &pipeline_info, nullptr, &pipeline));
		return pipeline;
	}

	void ComputePipelineBuilder::SetShader(VkShaderModule compute_shader)
	{
		shader_stages_.clear();
		shader_stages_.push_back(vkinit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, compute_shader));
	}

	void ComputePipelineBuilder::SetShader(ShaderEffect *effect)
	{
		shader_stages_.clear();
		effect->FillStage(shader_stages_);
	}

	PipelineStateConfig PipelineStateConfig::GetDefault(PassType type)
	{
		PipelineStateConfig config;
		switch (type)
		{
		case PassType::kCompute:
			// 计算Pass不需要大多数图形管线状态
			break;
		case PassType::kRaster:
			// 默认的光栅化配置
			config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			config.polygon_mode = VK_POLYGON_MODE_FILL;
			config.cull_mode = VK_CULL_MODE_BACK_BIT;
			config.front_face = VK_FRONT_FACE_CLOCKWISE;
			config.depth_test = true;
			config.depth_write = true;
			config.depth_compare_op = VK_COMPARE_OP_LESS;
			config.blend_mode = BlendMode::None;
			break;
		case PassType::kMesh:
			// 网格着色器Pass的默认配置
			config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			config.depth_test = true;
			break;
		}
		return config;
	}

	inline bool HasAlphaChannel(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return true;
            default:
                return false;
        }
    }
}
