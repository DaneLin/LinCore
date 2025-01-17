#include "graphics/render_pass/passes/culling_pass.h"
// lincore
#include "graphics/vk_device.h"
#include "graphics/vk_command_buffer.h"
#include "culling_pass.h"

namespace lincore
{
    CullingPass::~CullingPass()
    {
        Shutdown();
    }

    void CullingPass::Shutdown()
    {
        if (pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(gpu_device_->device_, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
    }

    void CullingPass::SetCullData(const scene::DrawCullData &cull_data)
    {
        cull_data_ = cull_data;
    }

    void CullingPass::PrepareShader()
    {
        pass_name_ = "Culling Pass";
        shader_ = gpu_device_->CreateShaderEffect({"shaders/cull.comp.spv"}, "CullingPass");
        shader_->ReflectLayout();
    }

    void CullingPass::PreparePipeline()
    {
        // 创建计算管线
        ComputePipelineBuilder builder{};
        builder.SetShader(shader_);
        pipeline_ = builder.BuildPipeline(gpu_device_->device_,
                                          shader_->built_layout_, // 使用ShaderEffect中已创建的布局
                                          gpu_device_->pipeline_cache_.GetCache());
        gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline_, "culling_pass");
    }

    void CullingPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
    {
        VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "culling_pass");

        
        // 计算管线
        cmd->BindPipeline(pipeline_, VK_PIPELINE_BIND_POINT_COMPUTE);
        
        // 绑定描述符集
        shader_->ApplyBinds(cmd->vk_command_buffer_);
        
        // 设置push常量
        cmd->PushConstants(shader_->built_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(scene::DrawCullData), &cull_data_);
        
        // // 添加内存屏障，确保draw buffer可写
        VkMemoryBarrier barrier{
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,    // sType
            nullptr,                             // pNext
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,  // srcAccessMask
            VK_ACCESS_SHADER_WRITE_BIT           // dstAccessMask
        };
        
        vkCmdPipelineBarrier(
            cmd->vk_command_buffer_,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );

        // 分发计算
        cmd->Dispatch(static_cast<uint32_t>(std::ceil(cull_data_.draw_count / 256.0)), 1, 1);

        // 添加计算完成后的内存屏障
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(
            cmd->vk_command_buffer_,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );
    }
    void CullingPass::SetupQueueType()
    {
        queue_type_ = QueueType::Compute;
    }
}
