#pragma once

#include "graphics/render_pass/render_pass.h"


namespace lincore
{
    struct FrameData;
    
    namespace scene
    {
        class SceneGraph;
    }

    struct SSAOPushConstants
    {
        float radius;
        float power;
        float bias;
    };

    class SSAOPass : public RenderPassBase
    {
    public:
        ~SSAOPass() override;
        virtual void Shutdown() override;

    protected:
        virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;
		virtual void SetupQueueType() override;

    private:
        VkPipeline ssao_pipeline_{ VK_NULL_HANDLE };

        SSAOPushConstants push_constants_;
	};
}
