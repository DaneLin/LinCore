#pragma once

#include "graphics/render_pass/render_pass.h"

namespace lincore
{
    struct FrameData;
    
    namespace scene
    {
        class SceneGraph;
    }

    class GBufferPass : public RenderPassBase
    {
    public:
            ~GBufferPass() override;
        virtual void Shutdown() override;

    protected:
        virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;
		virtual void SetupQueueType() override;

    private:
        VkPipeline gbuffer_pipeline_{ VK_NULL_HANDLE };
	};
}