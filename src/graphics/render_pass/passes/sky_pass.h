#pragma once

#include "graphics/render_pass/render_pass.h"

namespace lincore
{
	class GpuDevice;
	class CommandBuffer;
	class ShaderEffect;
	struct FrameData;

	struct ComputePushConstants {
		glm::vec4 data1;
		glm::vec4 data2;
		glm::vec4 data3;
		glm::vec4 data4;
	};

	class SkyBackgroundPass : public RenderPassBase
	{
	public:
		~SkyBackgroundPass() override;
		virtual void Shutdown() override;

	protected:
		virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;

	private:
		ComputePushConstants data_{};
		VkPipeline pipeline_{ VK_NULL_HANDLE };
	};
} // namespace lincore