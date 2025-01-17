#include "graphics/render_pass/passes/ssao_pass.h"
#include "graphics/backend/vk_pipelines.h"
#include "graphics/backend/vk_device.h"
#include "graphics/backend/vk_command_buffer.h"
#include "graphics/scene_graph/scene_view.h"
#include "graphics/scene_graph/scene_graph.h"
#include "graphics/backend/vk_profiler.h"

namespace lincore
{

    SSAOPass::~SSAOPass()
    {
        Shutdown();
    }

    void SSAOPass::Shutdown()
    {
        if (ssao_pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, ssao_pipeline_, nullptr);
            ssao_pipeline_ = VK_NULL_HANDLE;
        }
    }

    void SSAOPass::PrepareShader()
    {
        shader_ = gpu_device_->CreateShaderEffect({"shaders/ssao.vert.spv", "shaders/ssao.frag.spv"}, "SSAO_Pass");
        shader_->ReflectLayout();
    }

    void SSAOPass::PreparePipeline()
    {
        // 验证 shader_ 是否正确设置
        if (!shader_ || shader_->built_layout_ == VK_NULL_HANDLE)
        {
            LOGE("Invalid shader or pipeline layout!");
            return;
        }

        // 创建管线
        PipelineBuilder pipelineBuilder;
        pipelineBuilder.SetShaders(shader_);
        pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
        pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        pipelineBuilder.SetMultisamplingNone();
        pipelineBuilder.DisableBlending();

        // render format
        std::vector<VkFormat> color_formats;
        color_formats.emplace_back(VK_FORMAT_R8_SNORM);
        pipelineBuilder.SetColorAttachmentFormats(color_formats);

        ssao_pipeline_ = pipelineBuilder.BuildPipeline(gpu_device_->device_, gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)ssao_pipeline_, "ssao_pipeline");
    }

    void SSAOPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
    {
        VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "ssao_pass");

        std::vector<VkRenderingAttachmentInfo> color_attachments = gpu_device_->CreateRenderingAttachmentsColor(color_targets_);

        VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_->draw_extent_, color_attachments);
        cmd->BeginRendering(render_info);

        push_constants_.bias = 0.025f;
        push_constants_.radius = 0.5f;
        push_constants_.power = 2.0f;

        cmd->PushConstants(shader_->built_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants_), &push_constants_);

        cmd->BindPipeline(ssao_pipeline_);
        cmd->SetViewport(0, 0, static_cast<float>(gpu_device_->draw_extent_.width), static_cast<float>(gpu_device_->draw_extent_.height), 0.f, 1.f);
        cmd->SetScissor(0, 0, gpu_device_->draw_extent_.width, gpu_device_->draw_extent_.height);

        // 绑定描述符集
        shader_->ApplyBinds(cmd->vk_command_buffer_);

        cmd->Draw(3, 1, 0, 0);
        cmd->EndRendering();
    }

	void SSAOPass::SetupQueueType()
	{   
		queue_type_ = QueueType::Graphics;
	}

}