#include "graphics/render_pass/passes/sky_box_pass.h"
// lincore
#include "graphics/backend/vk_pipelines.h"
#include "graphics/backend/vk_device.h"
#include "graphics/backend/vk_command_buffer.h"

namespace lincore
{
    SkyBoxPass::~SkyBoxPass()
    {
        Shutdown();
    }

    void SkyBoxPass::Shutdown()
    {
        if (pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
    }

    void SkyBoxPass::PrepareShader()
    {
        shader_ = gpu_device_->CreateShaderEffect({"shaders/skybox.vert.spv", "shaders/skybox.frag.spv"}, "sky_box_pass");
        shader_->ReflectLayout();
    }

    void SkyBoxPass::PreparePipeline()
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
        pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        pipelineBuilder.SetMultisamplingNone();

        pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

        // 更新后的GBuffer格式
        std::vector<VkFormat> color_formats;
        for (auto &color_target : color_targets_)
        {
            Texture *texture = gpu_device_->GetResource<Texture>(color_target.index);
            color_formats.emplace_back(texture->vk_format);
        }

        pipelineBuilder.SetColorAttachmentFormats(color_formats);
        pipelineBuilder.SetDepthFormat(gpu_device_->GetDepthImage()->vk_format);

        pipelineBuilder.DisableBlending();

        pipeline_ = pipelineBuilder.BuildPipeline(gpu_device_->device_, gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline_, "sky_box_pipeline");
    }

    void SkyBoxPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
    {
        VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "sky_box_pass");

        // VkClearValue clear_values;
        // clear_values.color = {0.0f, 0.0f, 0.0f, 1.0f};
        std::vector<VkRenderingAttachmentInfo> color_attachments = gpu_device_->CreateRenderingAttachmentsColor(color_targets_);
        VkRenderingAttachmentInfo depth_attachment = gpu_device_->CreateRenderingAttachmentsDepth(depth_target_);

        VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_->draw_extent_, color_attachments.data(), &depth_attachment);
        cmd->BeginRendering(render_info);

        cmd->BindPipeline(pipeline_);
        cmd->SetViewport(0, 0, static_cast<float>(gpu_device_->draw_extent_.width), static_cast<float>(gpu_device_->draw_extent_.height), 0.f, 1.f);
        cmd->SetScissor(0, 0, gpu_device_->draw_extent_.width, gpu_device_->draw_extent_.height);
        // 绑定描述符集
        shader_->ApplyBinds(cmd->vk_command_buffer_);

        cmd->Draw(36, 1, 0, 0);

        cmd->EndRendering();
    }
    void SkyBoxPass::SetupQueueType()
    {
        queue_type_ = QueueType::Graphics;
    }
}
