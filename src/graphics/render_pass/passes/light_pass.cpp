#include "light_pass.h"
#include "graphics/vk_pipelines.h"
#include "graphics/vk_device.h"
#include "graphics/vk_command_buffer.h"
#include "graphics/scene_graph/scene_view.h"
#include "graphics/scene_graph/scene_graph.h"

namespace lincore
{

    LightPass::~LightPass()
    {
        Shutdown();
    }

    void LightPass::Shutdown()
    {
        if (light_pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, light_pipeline_, nullptr);
            light_pipeline_ = VK_NULL_HANDLE;
        }
    }

    void LightPass::PrepareShader()
    {
        pass_name_ = "Light Pass";
        shader_ = gpu_device_->CreateShaderEffect({"shaders/light.vert.spv", "shaders/light.frag.spv"}, "LightPass");
        shader_->ReflectLayout();
    }

    void LightPass::PreparePipeline()
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
        color_formats.emplace_back(gpu_device_->GetDrawImage()->vk_format);
        pipelineBuilder.SetColorAttachmentFormats(color_formats);
        pipelineBuilder.SetDepthFormat(gpu_device_->GetDepthImage()->vk_format);

        light_pipeline_ = pipelineBuilder.BuildPipeline(gpu_device_->device_, gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)light_pipeline_, "light_pipeline");
    }

    void LightPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
    {
        VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "light_pass");

        std::vector<VkRenderingAttachmentInfo> color_attachments = gpu_device_->CreateRenderingAttachmentsColor(color_targets_);

        VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_->draw_extent_, color_attachments, nullptr);
        cmd->BeginRendering(render_info);

        cmd->BindPipeline(light_pipeline_);
        cmd->SetViewport(0, 0, static_cast<float>(gpu_device_->draw_extent_.width), static_cast<float>(gpu_device_->draw_extent_.height), 0.f, 1.f);
        cmd->SetScissor(0, 0, gpu_device_->draw_extent_.width, gpu_device_->draw_extent_.height);

        // 绑定描述符集
        shader_->ApplyBinds(cmd->vk_command_buffer_);

        cmd->Draw(4, 1, 0, 0);
        cmd->EndRendering();
    }

	void LightPass::SetupQueueType()
	{   
		queue_type_ = QueueType::Graphics;
	}

}