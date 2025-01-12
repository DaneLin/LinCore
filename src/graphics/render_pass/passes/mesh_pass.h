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

    class MeshPass : public RenderPassBase
    {
    public:
        ~MeshPass() override;
        virtual void Shutdown() override;

    protected:
        virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;
		virtual void SetupQueueType() override;

    private:
        VkPipeline opaque_pipeline_{ VK_NULL_HANDLE };
        VkPipeline transparent_pipeline_{ VK_NULL_HANDLE };
	};
}
