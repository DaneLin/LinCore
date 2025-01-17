#pragma once
// lincore
#include "graphics/vk_types.h"
#include "graphics/vk_shaders.h"

namespace lincore
{
	enum class PassType : uint8_t
	{
		kCompute,
		kRaster,
		kMesh
	};

	namespace render_graph
	{
		enum class PassType;
		class PassResource;
	}

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

	// Pipeline状态配置
	struct PipelineStateConfig
	{
		// 图形管线配置
		VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
		VkPolygonMode polygon_mode{VK_POLYGON_MODE_FILL};
		VkCullModeFlags cull_mode{VK_CULL_MODE_BACK_BIT};
		VkFrontFace front_face{VK_FRONT_FACE_CLOCKWISE};

		// 深度测试配置
		bool depth_test{true};
		bool depth_write{true};
		VkCompareOp depth_compare_op{VK_COMPARE_OP_LESS};

		// 混合模式
		enum class BlendMode
		{
			None,
			Additive,
			AlphaBlend
		} blend_mode{BlendMode::None};

		// 获取默认配置
		static PipelineStateConfig GetDefault(PassType type);
	};

	class PipelineBuilder
	{
	public:
		std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_;
		VkPipelineRasterizationStateCreateInfo rasterizer_;
		VkPipelineColorBlendAttachmentState color_blend_attachment_;
		std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments_;
		VkPipelineMultisampleStateCreateInfo multisampling_;
		VkPipelineLayout pipeline_layout_;
		VkPipelineDepthStencilStateCreateInfo depth_stencil_;
		VkPipelineRenderingCreateInfo render_info_;
		std::vector<VkFormat> color_attachment_formats_;

		PipelineBuilder() { Clear(); }

		void Clear();

		VkPipeline BuildPipeline(VkDevice device, VkPipelineCache cache = VK_NULL_HANDLE);
		void ApplyConfig(const PipelineStateConfig &config);

		void SetShaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader);

		void SetShaders(ShaderEffect *effect);

		void SetInputTopology(VkPrimitiveTopology topology);

		void SetPolygonMode(VkPolygonMode mode);

		void SetCullMode(VkCullModeFlags cullMode, VkFrontFace front_face);

		void SetMultisamplingNone();

		void DisableBlending();
		void SetColorAttachmentFormats(std::vector<VkFormat>& formats);

		void SetDepthFormat(VkFormat format);

		void DisableDepthtest();

		void EnableDepthtest(bool depth_write_enable, VkCompareOp op);

		void EnableBlendingAdditive();

		void EnableBlendingAlphablend();

		void SetColorBlendAttachments(uint32_t count);
	};

	class ComputePipelineBuilder
	{
	public:
		void Clear();

		VkPipeline BuildPipeline(VkDevice device, VkPipelineLayout layout, VkPipelineCache cache = VK_NULL_HANDLE);

		void SetShader(VkShaderModule compute_shader);
		void SetShader(ShaderEffect *effect);

	private:
		std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;
	};

} // namespace lc
