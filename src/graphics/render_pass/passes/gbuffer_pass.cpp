#include "gbuffer_pass.h"

#include "graphics/vk_pipelines.h"
#include "graphics/vk_device.h"
#include "graphics/vk_command_buffer.h"
#include "graphics/scene_graph/scene_view.h"
#include "graphics/scene_graph/scene_graph.h"
namespace lincore
{

    GBufferPass::~GBufferPass()
    {
        Shutdown();
    }

    void GBufferPass::Shutdown()
    {
        if (gbuffer_pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, gbuffer_pipeline_, nullptr);
            gbuffer_pipeline_ = VK_NULL_HANDLE;
        }
    }

    void GBufferPass::PrepareShader()
    {
        shader_ = gpu_device_->CreateShaderEffect({"shaders/mrt.vert.spv", "shaders/mrt.frag.spv"}, "GBufferPass");
        shader_->ReflectLayout();
    }

    void GBufferPass::PreparePipeline()
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
        pipelineBuilder.SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        pipelineBuilder.SetMultisamplingNone();
        pipelineBuilder.DisableBlending();
        pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

        // render format
        // position, normal, albedo, arm, emission
        std::vector<VkFormat> formats;
        formats.push_back(VK_FORMAT_R16G16B16A16_SFLOAT);
        formats.push_back(VK_FORMAT_R16G16B16A16_SFLOAT);
        formats.push_back(VK_FORMAT_R8G8B8A8_UNORM);
        formats.push_back(VK_FORMAT_R8G8B8A8_UNORM);
        formats.push_back(VK_FORMAT_R8G8B8A8_UNORM);
        pipelineBuilder.SetColorAttachmentFormats(formats);

        pipelineBuilder.SetDepthFormat(gpu_device_->GetDepthImage()->vk_format);

        gbuffer_pipeline_ = pipelineBuilder.BuildPipeline(gpu_device_->device_, gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)gbuffer_pipeline_, "gbuffer_pipeline");
    }

    void GBufferPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
    {
        VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "gbuffer_pass");

        VkClearValue clear_values;
        clear_values.color = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::vector<VkRenderingAttachmentInfo> color_attachments = gpu_device_->CreateRenderingAttachmentsColor(color_targets_, &clear_values);
        VkRenderingAttachmentInfo depth_attachment = gpu_device_->CreateRenderingAttachmentsDepth(depth_target_);

        VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_->draw_extent_, color_attachments, &depth_attachment);
        cmd->BeginRendering(render_info);

        cmd->BindPipeline(gbuffer_pipeline_);
        cmd->SetViewport(0, 0, static_cast<float>(gpu_device_->draw_extent_.width), static_cast<float>(gpu_device_->draw_extent_.height), 0.f, 1.f);
        cmd->SetScissor(0, 0, gpu_device_->draw_extent_.width, gpu_device_->draw_extent_.height);
        // 绑定描述符集
        shader_->ApplyBinds(cmd->vk_command_buffer_);

        // 从scene_graph_中获取顶点缓冲区
        scene::GPUResourcePool &gpu_resource_pool = scene_graph_->GetGPUResourcePool();
        cmd->BindIndexBuffer(gpu_device_->GetResource<Buffer>(gpu_resource_pool.index_buffer.index)->vk_buffer, 0, VK_INDEX_TYPE_UINT32);
        // 使用间接绘制命令进行渲染
        cmd->DrawIndexedIndirect(
            gpu_device_->GetResource<Buffer>(gpu_resource_pool.draw_indirect_buffer.index)->vk_buffer,
            0,                 
            sizeof(scene::DrawCommand),  
            gpu_resource_pool.draw_count);

        cmd->EndRendering();
    }

	void GBufferPass::SetupQueueType()
	{   
		queue_type_ = QueueType::Graphics;
	}

}