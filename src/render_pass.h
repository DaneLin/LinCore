#pragma once

#include "frame_graph.h"

class VulkanEngine;

namespace lc
{
	class BackgroundPass : public FrameGraphRenderPass
	{
	public:
		virtual void PreRender(CommandBuffer* gpu_command) override;
		virtual void Render(CommandBuffer* gpu_command) override;
		virtual void OnResize(VulkanEngine* engine, uint32_t new_width, uint32_t new_height) override;
	private:

	};
}