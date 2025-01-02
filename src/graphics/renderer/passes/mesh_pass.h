#pragma once

#include "render_pass.h"

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

        MeshPass& SetSceneGraph(scene::SceneGraph* graph);

    protected:
        virtual void PrepareShader() override;
		virtual void PreparePipeline() override;
		virtual void ExecutePass(CommandBuffer* cmd, FrameData* frame) override;

    private:
        VkPipeline opaque_pipeline_{ VK_NULL_HANDLE };
        VkPipeline transparent_pipeline_{ VK_NULL_HANDLE };
	};
}
