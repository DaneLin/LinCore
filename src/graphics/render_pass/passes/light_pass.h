#pragma once

#include "graphics/render_pass/render_pass.h"
#include "graphics/vk_profiler.h"

namespace lincore
{
    struct FrameData;
    
    namespace scene
    {
        class SceneGraph;
    }

    class LightPass : public RenderPassBase
    {
    public:
        ~LightPass() override;
        virtual void Shutdown() override;

    protected:
        virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;
		virtual void SetupQueueType() override;

    private:
        VkPipeline light_pipeline_{ VK_NULL_HANDLE };
	};
}
