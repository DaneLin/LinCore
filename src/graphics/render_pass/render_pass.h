#pragma once
// std
#include <unordered_map>
// lincore
#include "graphics/vk_types.h"

namespace lincore
{
	class GpuDevice;
	class CommandBuffer;
	class ShaderEffect;
	struct FrameData;
	namespace scene
	{
		class SceneGraph;
	}
	/**
	 * @brief 渲染Pass输入
	 * 包含需要传递给Pass的资源
	 */
	struct PassInput
	{
		struct Resource
		{
			const char *name;	   // 资源名字
			ResourceHandle handle; // 资源句柄
		};

		std::unordered_map<std::string, Resource> resources_map;
	};

	/**
	 * @brief 渲染Pass输出
	 * 包含需要从Pass输出的资源
	 */
	struct PassOutput
	{
		struct Resource
		{
			const char *name;	  // 资源名字
			TextureHandle handle; // 资源句柄
		};

		std::vector<Resource> resources_list;
	};

	/**
	 * @brief RenderPass基类
	 * 所有渲染Pass的基类
	 */

	class RenderPassBase
	{
	public:
		virtual ~RenderPassBase() = default;

		RenderPassBase &Init(GpuDevice *device);
		virtual void Shutdown() {};

		// Pass配置API
		RenderPassBase &BindInputs(std::initializer_list<PassInput::Resource> resources);
		RenderPassBase &BindRenderTargets(std::initializer_list<PassOutput::Resource> color_resources, std::initializer_list<PassOutput::Resource> depth_resources);
		RenderPassBase& SetSceneGraph(scene::SceneGraph* graph);
		// 完成Pass配置并初始化
		virtual void Finalize();

		virtual void Execute(CommandBuffer *cmd, FrameData *frame = nullptr);

	protected:
		// 准备Shader
		virtual void PrepareShader() = 0;
		// 准备管线
		virtual void PreparePipeline() = 0;
		// 准备特定资源
		virtual void PrepareSpecificResources() {}
		// pass执行逻辑
		virtual void ExecutePass(CommandBuffer *cmd, FrameData *frame) = 0;

	private:
		// 验证输入
		void ValidateInput();
		// 验证输出
		void ValidateOutput();
		// 更新输入资源
		void UpdateInputResources(CommandBuffer *cmd, FrameData *frame);

	protected:
		GpuDevice *gpu_device_{nullptr};
		ShaderEffect *shader_{nullptr};

		// Pass配置
		PassInput pass_input_;
		PassOutput pass_output_;
		std::vector<TextureHandle> color_targets_;
		TextureHandle depth_target_;

		scene::SceneGraph* scene_graph_{ nullptr };

		bool is_finalized_{false};
	};
}