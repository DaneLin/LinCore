#pragma once

#include "graphics/render_pass/render_pass.h"
#include "graphics/scene_graph/scene_types.h"

namespace lincore
{
    class GpuDevice;
	class CommandBuffer;
	class ShaderEffect;
	struct FrameData;

	class CullingPass : public RenderPassBase
	{
	public:
		~CullingPass() override;
		virtual void Shutdown() override;

		void SetCullData(const scene::DrawCullData& cull_data);

	protected:
		virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;

	private:
		VkPipeline pipeline_{ VK_NULL_HANDLE };
		scene::DrawCullData cull_data_{};
	};
}