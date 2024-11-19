#pragma once 
#include <vk_types.h>
#include "vk_shaders_new.h"

class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;
	VkPipelineRenderingCreateInfo _renderInfo;
	VkFormat _colorAttachmentFormat;

	PipelineBuilder() { clear(); }

	void clear();

	VkPipeline build_pipeline(VkDevice device);

	VkPipeline build_pipeline_compute(VkDevice device);

	void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

	void set_shaders(lc::ShaderEffect* effect);

	void set_input_topology(VkPrimitiveTopology topology);

	void set_polygon_mode(VkPolygonMode mode);

	void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);

	void set_multisampling_none();

	void disable_blending();

	void set_color_attachment_format(VkFormat format);

	void set_depth_format(VkFormat format);

	void disable_depthtest();

	void enable_depthtest(bool depthWriteEnable, VkCompareOp op);

	void enable_blending_additive();

	void enable_blending_alphablend();
};

namespace vkutil {

	bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};