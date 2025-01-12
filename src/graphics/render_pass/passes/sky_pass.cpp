#include "graphics/render_pass/passes/sky_pass.h"
// lincore
#include "graphics/vk_pipelines.h"
#include "graphics/vk_device.h"
#include "graphics/vk_command_buffer.h"
#include "sky_pass.h"

namespace lincore
{
	SkyBackgroundPass::~SkyBackgroundPass()
	{
		Shutdown();
	}

	void SkyBackgroundPass::Shutdown()
	{
		if (pipeline_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(gpu_device_->device_, pipeline_, nullptr);
			pipeline_ = VK_NULL_HANDLE;
		}
	}

	void SkyBackgroundPass::PrepareShader()
	{
		shader_ = gpu_device_->CreateShaderEffect({"shaders/sky.comp.spv"}, "sky_pass");
		shader_->ReflectLayout();
	}

	void SkyBackgroundPass::PreparePipeline()
	{
		// 创建计算管线
		ComputePipelineBuilder builder{};
		builder.SetShader(shader_);
		pipeline_ = builder.BuildPipeline(gpu_device_->device_,
										  shader_->built_layout_, // 使用ShaderEffect中已创建的布局
										  gpu_device_->pipeline_cache_.GetCache());
		gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline_, "sky_pass");

		data_ = {
			.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97)};
	}

	void SkyBackgroundPass::ExecutePass(CommandBuffer *cmd, FrameData *frame)
	{
		VulkanScopeTimer timer(cmd->vk_command_buffer_, &gpu_device_->profiler_, "sky_pass");

		// 计算管线
		cmd->BindPipeline(pipeline_, VK_PIPELINE_BIND_POINT_COMPUTE);
		// 绑定描述符集
		shader_->ApplyBinds(cmd->vk_command_buffer_);
		// 设置push常量
		cmd->PushConstants(shader_->built_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &data_);
		// 分发计算
		cmd->Dispatch(static_cast<uint32_t>(std::ceil(gpu_device_->draw_extent_.width / 16.0)), static_cast<uint32_t>(std::ceil(gpu_device_->draw_extent_.height / 16.0)), 1);
	}
	void SkyBackgroundPass::SetupQueueType()
	{
		queue_type_ = QueueType::Compute;
	}
}
