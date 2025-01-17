#include "graphics/render_pass/render_pass.h"
#include "graphics/backend/vk_device.h"
#include "foundation/resources.h"
#include "foundation/logging.h"

namespace lincore
{

	RenderPassBase &RenderPassBase::BindInputs(std::initializer_list<PassInput::Resource> resources)
	{
		for (const auto &resource : resources)
		{
			pass_input_.resources_map[resource.name] = resource;
		}

		return *this;
	}

	RenderPassBase &RenderPassBase::Init(GpuDevice *device)
	{
		gpu_device_ = device;
		return *this;
	}

	RenderPassBase &RenderPassBase::BindRenderTargets(std::initializer_list<PassOutput::Resource> color_resources, std::initializer_list<PassOutput::Resource> depth_resources)
	{
		// 清理之前的状态
		color_targets_.clear();
		depth_target_.index = k_invalid_index;
		pass_output_.resources_list.clear();

		// 添加color attachments
		for (const auto &resource : color_resources)
		{
			pass_output_.resources_list.push_back(resource);
			color_targets_.push_back(resource.handle);
		}

		// 添加depth attachment
		if (depth_resources.size() > 0)
		{
			pass_output_.resources_list.push_back(*depth_resources.begin());
			depth_target_ = depth_resources.begin()->handle;
		}

		return *this;
	}

	void RenderPassBase::Finalize()
	{
		if (is_finalized_)
		{
			LOGE("RenderPassBase::Finalize() called multiple times");
			return;
		}
		SetupQueueType();
		PrepareShader();
		ValidateInput();
		ValidateOutput();
		PreparePipeline();
		PrepareSpecificResources();
		is_finalized_ = true;
	}

	void RenderPassBase::Execute(CommandBuffer *cmd, FrameData *frame)
	{
		UpdateInputResources(cmd, frame);
		UpdateRenderTargets(cmd);

		// 添加调试标签
		VkDebugUtilsLabelEXT label_info = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			nullptr,
			pass_name_.c_str(),
			{1.0f, 1.0f, 1.0f, 1.0f}
		};
		vkCmdBeginDebugUtilsLabelEXT(cmd->GetVkCommandBuffer(), &label_info);

		ExecutePass(cmd, frame);

		vkCmdEndDebugUtilsLabelEXT(cmd->GetVkCommandBuffer());
	}

	void RenderPassBase::ValidateInput()
	{
		for (auto &[key, resource] : pass_input_.resources_map)
		{
			auto binding_it = shader_->bindings_.find(key);
			if (binding_it == shader_->bindings_.end())
			{
				throw std::runtime_error("Shader binding '" + key + "' not found!");
			}
		}
	}

	void RenderPassBase::ValidateOutput()
	{
		// TODO: 验证输出
	}

	void RenderPassBase::UpdateInputResources(CommandBuffer *cmd, FrameData *frame)
	{
		for (auto &[key, resource] : pass_input_.resources_map)
		{
			auto binding_it = shader_->bindings_.find(key);
			if (binding_it == shader_->bindings_.end())
			{
				LOGW("Shader binding {} not found!", key.c_str());
				continue;
			}

			// 1.检测资源状态
			ResourceState new_state = UtilDetermineResourceState(shader_->bindings_[key].type);

			// 2.根据shader反射信息检测资源类型
			VkDescriptorType desc_type = binding_it->second.type;
			if (desc_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
				desc_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
				desc_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
				desc_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
			{
				Buffer *buffer = gpu_device_->GetResource<Buffer>(resource.handle);
				if (buffer->state != new_state)
				{
					cmd->AddBufferBarrier(buffer, new_state, buffer->queue_family, queue_type_);
				}
				shader_->BindBuffer(key.c_str(), UtilToVkDescriptorBufferInfo(buffer));
			}
			else if (desc_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
					 desc_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
					 desc_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
					 desc_type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
			{
				Texture *texture = gpu_device_->GetResource<Texture>(resource.handle);
				if (texture->state != new_state)
				{
					cmd->AddImageBarrier(texture, new_state,
										 texture->mip_base_level, texture->mip_level_count,
										 texture->array_base_layer, texture->array_layer_count,
										 texture->queue_family, queue_type_);
				}
				shader_->BindImage(key.c_str(), UtilToVkDescriptorImageInfo(texture));
			}
		}
		shader_->BuildSets(&frame->frame_descriptors);
	}

	void RenderPassBase::UpdateRenderTargets(CommandBuffer *cmd)
	{
		// Add memory barriers to ensure proper synchronization
        for (const auto& target : color_targets_) {
            Texture* texture = gpu_device_->GetResource<Texture>(target.index);
            cmd->AddImageBarrier(texture, ResourceState::RESOURCE_STATE_RENDER_TARGET);
        }
        if (depth_target_.index != k_invalid_index) {
            Texture* depth_texture = gpu_device_->GetResource<Texture>(depth_target_.index);
            cmd->AddImageBarrier(depth_texture, ResourceState::RESOURCE_STATE_DEPTH_WRITE);
        }
	}
}


