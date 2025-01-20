#pragma once

#include "graphics/render_pass/render_pass.h"

namespace lincore
{
	class GpuDevice;
	class CommandBuffer;
	class ShaderEffect;
	struct FrameData;

	class SkyBoxPass : public RenderPassBase
	{
	public:
		~SkyBoxPass() override;
		virtual void Shutdown() override;

	protected:
		virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;
		virtual void SetupQueueType() override;

	private:
		VkPipeline pipeline_{ VK_NULL_HANDLE };
	};
} // namespace lincore