#include "mesh_pass.h"

#include "graphics/vk_pipelines.h"
#include "graphics/vk_device.h"
#include "graphics/vk_command_buffer.h"
#include "graphics/scene_graph/scene_view.h"
#include "graphics/scene_graph/scene_graph.h"
namespace lincore
{

    MeshPass::~MeshPass()
    {
        Shutdown();
    }

    void MeshPass::Shutdown()
    {
        if (opaque_pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, opaque_pipeline_, nullptr);
            opaque_pipeline_ = VK_NULL_HANDLE;
        }
        if (transparent_pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, transparent_pipeline_, nullptr);
            transparent_pipeline_ = VK_NULL_HANDLE;
        }
    }

    void MeshPass::PrepareShader()
    {
        shader_ = gpu_device_->CreateShaderEffect({"shaders/mesh.vert.spv", "shaders/mesh.frag.spv"}, "MeshPass");
        shader_->ReflectLayout();
    }

    void MeshPass::PreparePipeline()
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
        pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

        // render format
        pipelineBuilder.SetColorAttachmentFormat(gpu_device_->GetDrawImage()->vk_format);
        pipelineBuilder.SetDepthFormat(gpu_device_->GetDepthImage()->vk_format);

        opaque_pipeline_ = pipelineBuilder.BuildPipeline(gpu_device_->device_, gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)opaque_pipeline_, "opaque_pipeline");

        // create the transparent variant
        pipelineBuilder.EnableBlendingAdditive();

        pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

        transparent_pipeline_ = pipelineBuilder.BuildPipeline(gpu_device_->device_, gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)transparent_pipeline_, "transparent_pipeline");
    }

    void MeshPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
    {
        VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "mesh_pass");

        std::vector<VkRenderingAttachmentInfo> color_attachments = gpu_device_->CreateRenderingAttachmentsColor(color_targets_);
        VkRenderingAttachmentInfo depth_attachment = gpu_device_->CreateRenderingAttachmentsDepth(depth_target_);

        VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_->draw_extent_, color_attachments.data(), &depth_attachment);
        cmd->BeginRendering(render_info);

        cmd->BindPipeline(opaque_pipeline_);
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

	void MeshPass::SetupQueueType()
	{   
		queue_type_ = QueueType::Graphics;
	}

}