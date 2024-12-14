#pragma once
#include <vk_types.h>
#include "vk_shaders.h"

namespace lc
{
	namespace vkutils
	{

	} // namespace vkutils

	class PipelineCache
	{
	public:

		void Init(VkDevice device, const std::string cache_file_path);
		void CleanUp();

		VkPipelineCache GetCache() { return cache_; }

		void SaveCache();

	private:
		bool LoadPipelineCache();
		void CreatePipelineCache();

	private:
		VkPipelineCache cache_;
		VkDevice device_;
		std::string cache_file_path_;
	};

	class PipelineBuilder
	{
	public:
		std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_;
		VkPipelineRasterizationStateCreateInfo rasterizer_;
		VkPipelineColorBlendAttachmentState color_blend_attachment_;
		VkPipelineMultisampleStateCreateInfo multisampling_;
		VkPipelineLayout pipeline_layout_;
		VkPipelineDepthStencilStateCreateInfo depth_stencil_;
		VkPipelineRenderingCreateInfo render_info_;
		VkFormat color_attachment_format_;

		PipelineBuilder() { Clear(); }

		void Clear();

		VkPipeline BuildPipeline(VkDevice device, VkPipelineCache cache = VK_NULL_HANDLE);

		void SetShaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader);

		void SetShaders(ShaderEffect *effect);

		void SetInputTopology(VkPrimitiveTopology topology);

		void SetPolygonMode(VkPolygonMode mode);

		void SetCullMode(VkCullModeFlags cullMode, VkFrontFace front_face);

		void SetMultisamplingNone();

		void DisableBlending();

		void SetColorAttachmentFormat(VkFormat format);

		void SetDepthFormat(VkFormat format);

		void DisableDepthtest();

		void EnableDepthtest(bool depth_write_enable, VkCompareOp op);

		void EnableBlendingAdditive();

		void EnableBlendingAlphablend();
	};

} // namespace lc
