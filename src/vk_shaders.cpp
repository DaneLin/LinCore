#include "vk_shaders.h"
#include "vk_initializers.h"
#include "logging.h"

#include "vk_engine.h"

#include <fstream>
#include <algorithm>
#include "spirv_reflect.h"
#include <spirv-headers/spirv.h>

namespace lc
{

	bool vkutil::LoadShader(VkDevice device, const char *file_path, ShaderModule *out_shader_module)
	{
		std::ifstream file(file_path, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			LOGE("Failed to open file: %s", file_path);
			return false;
		}

		size_t file_size = (size_t)file.tellg();

		std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

		file.seekg(0);

		file.read((char *)buffer.data(), file_size);

		file.close();

		VkShaderModuleCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr};
		create_info.codeSize = buffer.size() * sizeof(uint32_t);
		create_info.pCode = buffer.data();

		VkShaderModule shader_module;
		VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader_module));

		out_shader_module->code = std::move(buffer);
		out_shader_module->module = shader_module;
		return true;
	}

	// FNV-1a 32bit hashing algorithm.
	constexpr uint32_t kFnv1a32(char const *s, std::size_t count)
	{
		return ((count ? kFnv1a32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}

	uint32_t vkutil::HashDescriptorLayoutInfo(VkDescriptorSetLayoutCreateInfo *info)
	{
		// put all the data into a string and then hash the string
		std::stringstream ss;

		ss << info->flags;
		ss << info->bindingCount;

		for (uint32_t i = 0; i < info->bindingCount; ++i)
		{
			const VkDescriptorSetLayoutBinding &binding = info->pBindings[i];

			ss << binding.binding;
			ss << binding.descriptorCount;
			ss << binding.descriptorType;
			ss << binding.stageFlags;
		}
		auto str = ss.str();

		return kFnv1a32(str.c_str(), str.length());
	}

	void ShaderEffect::AddStage(ShaderModule *shader_module, VkShaderStageFlagBits stage)
	{
		ShaderStage newStage = {shader_module, stage};
		stages_.push_back(newStage);
	}

	struct DescriptorSetLayoutData
	{
		uint32_t set_number;
		VkDescriptorSetLayoutCreateInfo create_info;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};

	void ShaderEffect::ReflectLayout(VkDevice device, ReflectionOverrides *overrides, int override_count, uint32_t override_constant_size)
	{
		std::vector<DescriptorSetLayoutData> set_Layouts;

		std::vector<VkPushConstantRange> constant_ranges;

		for (auto &s : stages_)
		{
			SpvReflectShaderModule spv_module;
			SpvReflectResult result = spvReflectCreateShaderModule(s.module->code.size() * sizeof(uint32_t), s.module->code.data(), &spv_module);

			uint32_t count = 0;
			result = spvReflectEnumerateDescriptorSets(&spv_module, &count, nullptr);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectDescriptorSet *> sets(count);
			result = spvReflectEnumerateDescriptorSets(&spv_module, &count, sets.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			for (size_t idx = 0; idx < sets.size(); idx++)
			{
				const SpvReflectDescriptorSet &reflectSet = *(sets[idx]);

				DescriptorSetLayoutData layout = {};

				layout.bindings.resize(reflectSet.binding_count);
				for (uint32_t i_binding = 0; i_binding < reflectSet.binding_count; i_binding++)
				{
					const SpvReflectDescriptorBinding &reflectBinding = *(reflectSet.bindings[i_binding]);
					VkDescriptorSetLayoutBinding &layoutBinding = layout.bindings[i_binding];
					layoutBinding.binding = reflectBinding.binding;
					layoutBinding.descriptorType = static_cast<VkDescriptorType>(reflectBinding.descriptor_type);

					for (int ov = 0; ov < override_count; ov++)
					{
						if (strcmp(reflectBinding.name, overrides[ov].name) == 0)
						{
							layoutBinding.descriptorType = overrides[ov].overriden_type;
						}
					}

					layoutBinding.descriptorCount = 1;
					for (uint32_t i_dim = 0; i_dim < reflectBinding.array.dims_count; ++i_dim)
					{
						layoutBinding.descriptorCount *= reflectBinding.array.dims[i_dim];
					}
					layoutBinding.stageFlags = static_cast<VkShaderStageFlagBits>(spv_module.shader_stage);

					ReflectedBinding reflected;
					reflected.binding = layoutBinding.binding;
					reflected.set = reflectSet.set;
					reflected.type = layoutBinding.descriptorType;

					bindings_[reflectBinding.name] = reflected;
				}
				layout.set_number = reflectSet.set;
				layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layout.create_info.bindingCount = reflectSet.binding_count;
				layout.create_info.pBindings = layout.bindings.data();

				set_Layouts.push_back(layout);
			}

			// push constants

			result = spvReflectEnumeratePushConstantBlocks(&spv_module, &count, nullptr);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectBlockVariable *> pconstants(count);
			result = spvReflectEnumeratePushConstantBlocks(&spv_module, &count, pconstants.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			if (count > 0)
			{
				VkPushConstantRange pcs{};
				pcs.offset = pconstants[0]->offset;
				pcs.size = override_constant_size == -1 ? pconstants[0]->size : override_constant_size;
				pcs.stageFlags = s.stage;

				constant_ranges.push_back(pcs);
			}
		}

		std::array<DescriptorSetLayoutData, 4> merged_layouts;

		for (int i = 0; i < 4; i++)
		{
			DescriptorSetLayoutData &layout = merged_layouts[i];

			layout.set_number = i;

			layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

			std::unordered_map<int, VkDescriptorSetLayoutBinding> binds;
			for (auto &s : set_Layouts)
			{
				if (s.set_number == i)
				{
					for (auto &b : s.bindings)
					{
						auto it = binds.find(b.binding);
						if (it == binds.end())
						{
							binds[b.binding] = b;
						}
						else
						{
							// merge flags
							binds[b.binding].stageFlags |= b.stageFlags;
						}
					}
				}
			}

			for (auto [k, v] : binds)
			{
				layout.bindings.push_back(v);
			}

			// sort the bindings, for hash purposes
			std::sort(layout.bindings.begin(), layout.bindings.end(), [](VkDescriptorSetLayoutBinding &a, VkDescriptorSetLayoutBinding &b)
					  { return a.binding < b.binding; });

			layout.create_info.bindingCount = static_cast<uint32_t>(layout.bindings.size());
			layout.create_info.pBindings = layout.bindings.data();
			layout.create_info.flags = 0;
			layout.create_info.pNext = 0;

			if (layout.create_info.bindingCount > 0)
			{
				set_hashes_[i] = vkutil::HashDescriptorLayoutInfo(&layout.create_info);
				vkCreateDescriptorSetLayout(device, &layout.create_info, nullptr, &set_layouts_[i]);
			}
			else
			{
				set_hashes_[i] = 0;
				set_layouts_[i] = VK_NULL_HANDLE;
			}
		}

		// we start from just the default empty pipeline layout info
		VkPipelineLayoutCreateInfo pipelineCreateInfo = vkinit::PipelineLayoutCreateInfo();

		pipelineCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(constant_ranges.size());
		pipelineCreateInfo.pPushConstantRanges = constant_ranges.data();

		std::array<VkDescriptorSetLayout, 4> compactedLayouts;
		int s = 0;
		for (int i = 0; i < 4; i++)
		{
			if (set_layouts_[i] != VK_NULL_HANDLE)
			{
				compactedLayouts[s] = set_layouts_[i];
				s++;
			}
		}

		pipelineCreateInfo.setLayoutCount = s;
		pipelineCreateInfo.pSetLayouts = compactedLayouts.data();

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineCreateInfo, nullptr, &built_layout_));
	}

	void ShaderEffect::FillStage(std::vector<VkPipelineShaderStageCreateInfo> &pipeline_stages)
	{
		for (auto &s : stages_)
		{
			pipeline_stages.push_back(vkinit::PipelineShaderStageCreateInfo(s.stage, s.module->module));
		}
	}

	VkPipelineBindPoint ShaderEffect::GetBindPoint() const
	{

		for (const auto &stage : stages_)
		{
			switch (stage.stage)
			{
			case VK_SHADER_STAGE_COMPUTE_BIT:
				return VK_PIPELINE_BIND_POINT_COMPUTE;
			default:
				return VK_PIPELINE_BIND_POINT_GRAPHICS;
			}
		}
		return VK_PIPELINE_BIND_POINT_GRAPHICS; // 默认返回图形管线
	}

	ShaderEffect::~ShaderEffect()
	{
		for (int i = 0; i < 4; i++)
		{
			if (set_layouts_[i] != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(VulkanEngine::Get().device_, set_layouts_[i], nullptr);
			}
		}
	}

	void ShaderDescriptorBinder::BindBuffer(const char *name, const VkDescriptorBufferInfo &BufferInfo)
	{
		BindDynamicBuffer(name, -1, BufferInfo);
	}

	void ShaderDescriptorBinder::BindImage(const char *name, const VkDescriptorImageInfo &image_info)
	{
		if (shaders_ == nullptr)
		{
			LOGE("No shader set");
			return;
		}

		auto found = shaders_->bindings_.find(name);
		if (found != shaders_->bindings_.end())
		{
			const ShaderEffect::ReflectedBinding &bind = (*found).second;

			for (auto &write : image_writes_)
			{
				if (write.dst_binding == bind.binding && write.dst_set == bind.set)
				{
					if (write.image_info.sampler != image_info.sampler ||
						write.image_info.imageView != image_info.imageView ||
						write.image_info.imageLayout != image_info.imageLayout)
					{
						write.image_info = image_info;
					}
					return;
				}
			}

			ImageWriteDescriptor new_write;
			new_write.dst_set = bind.set;
			new_write.dst_binding = bind.binding;
			new_write.descriptor_type = bind.type;
			new_write.image_info = image_info;

			image_writes_.push_back(new_write);
		}
	}

	void ShaderDescriptorBinder::BindDynamicBuffer(const char *name, uint32_t offset, const VkDescriptorBufferInfo &BufferInfo)
	{
		if (shaders_ == nullptr)
		{
			LOGE("No shader set");
			return;
		}

		auto found = shaders_->bindings_.find(name);
		if (found != shaders_->bindings_.end())
		{
			const ShaderEffect::ReflectedBinding &bind = (*found).second;

			for (auto &write : buffer_writes_)
			{
				if (write.dst_binding == bind.binding && write.dst_set == bind.set)
				{
					if (write.BufferInfo.buffer != BufferInfo.buffer ||
						write.BufferInfo.range != BufferInfo.range ||
						write.BufferInfo.offset != BufferInfo.offset)
					{
						write.BufferInfo = BufferInfo;
						write.dynamic_offset = offset;

						cached_descriptor_sets_[write.dst_set] = VK_NULL_HANDLE;
					}
					else
					{
						// already in the write lists, but matches buffer
						write.dynamic_offset = offset;
					}
					return;
				}
			}

			BufferWriteDescriptor newWrite;
			newWrite.dst_set = bind.set;
			newWrite.dst_binding = bind.binding;
			newWrite.descriptor_type = bind.type;
			newWrite.BufferInfo = BufferInfo;
			newWrite.dynamic_offset = offset;

			cached_descriptor_sets_[bind.set] = VK_NULL_HANDLE;

			buffer_writes_.push_back(newWrite);
		}
	}

	void ShaderDescriptorBinder::ApplyBinds(VkCommandBuffer cmd, VkPipelineLayout layout)
	{
		for (size_t i = 0; i < cached_descriptor_sets_.size(); ++i)
		{
			if (cached_descriptor_sets_[i] != VK_NULL_HANDLE)
			{
				vkCmdBindDescriptorSets(cmd, shaders_->GetBindPoint(), shaders_->built_layout_, static_cast<uint32_t>(i), 1, &cached_descriptor_sets_[i], set_offsets_[i].count, set_offsets_[i].offset.data());
			}
		}
	}

	void ShaderDescriptorBinder::BuildSets(VkDevice device, DescriptorAllocatorGrowable &allocator)
	{
		std::array<std::vector<VkWriteDescriptorSet>, 4> writes{};

		std::sort(buffer_writes_.begin(), buffer_writes_.end(), [](BufferWriteDescriptor &a, BufferWriteDescriptor &b)
				  {
			if (b.dst_binding == a.dst_binding) {
				return a.dst_set < b.dst_set;
			}
			else {
				return a.dst_binding < b.dst_binding;
			} });

		// reset the dynamic offsets
		for (auto &s : set_offsets_)
		{
			s.count = 0;
		}

		for (BufferWriteDescriptor &w : buffer_writes_)
		{
			uint32_t set = w.dst_set;
			VkWriteDescriptorSet write = vkinit::WriteDescriptorBuffer(w.descriptor_type, VK_NULL_HANDLE, &w.BufferInfo, w.dst_binding);

			writes[set].push_back(write);

			// dynamic offsets
			if (w.descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
				w.descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
			{
				DynOffset &offsetSet = set_offsets_[set];
				offsetSet.offset[offsetSet.count] = w.dynamic_offset;
				offsetSet.count++;
			}
		}

		for (ImageWriteDescriptor &w : image_writes_)
		{
			uint32_t set = w.dst_set;
			VkWriteDescriptorSet write = vkinit::WriteDescriptorImage(w.descriptor_type, VK_NULL_HANDLE, &w.image_info, w.dst_binding);

			writes[set].push_back(write);
		}

		for (size_t i = 0; i < cached_descriptor_sets_.size(); ++i)
		{
			if (writes[i].size() > 0)
			{
				if (cached_descriptor_sets_[i] == VK_NULL_HANDLE)
				{
					// alloc
					auto layout = shaders_->set_layouts_[i];

					VkDescriptorSet new_descriptor = allocator.Allocate(device, layout);

					for (auto &w : writes[i])
					{
						w.dstSet = new_descriptor;
					}

					vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes[i].size()), writes[i].data(), 0, nullptr);

					cached_descriptor_sets_[i] = new_descriptor;
				}
			}
		}
	}

	void ShaderDescriptorBinder::SetShader(ShaderEffect *new_shader)
	{
		// invalidate nonequal layouts
		if (shaders_ && shaders_ != new_shader)
		{
			for (size_t i = 0; i < cached_descriptor_sets_.size(); i++)
			{
				if (new_shader->set_hashes_[i] != shaders_->set_hashes_[i])
				{
					cached_descriptor_sets_[i] = VK_NULL_HANDLE;
				}
				else if (new_shader->set_hashes_[i] == 0)
				{
					cached_descriptor_sets_[i] = VK_NULL_HANDLE;
				}
			}
		}
		else
		{
			for (size_t i = 0; i < cached_descriptor_sets_.size(); i++)
			{
				cached_descriptor_sets_[i] = VK_NULL_HANDLE;
			}
		}

		shaders_ = new_shader;
	}

	ShaderEffect *ShaderCache::GetShaderEffect()
	{
		ShaderEffect *result = new ShaderEffect();
		shader_effect_cache_.push_back(result);
		return result;
	}

	ShaderEffect *ShaderCache::GetShaderEffect(const std::string &path, VkShaderStageFlagBits stage)
	{
		ShaderEffect *result = new ShaderEffect();
		result->AddStage(GetShader(path), stage);
		shader_effect_cache_.push_back(result);
		return result;
	}

	ShaderModule *ShaderCache::GetShader(const std::string &path)
	{
		auto it = module_cache_.find(path);
		if (it == module_cache_.end())
		{
			ShaderModule newShader;

			bool result = vkutil::LoadShader(VulkanEngine::Get().device_, path.c_str(), &newShader);
			if (!result)
			{
				LOGE("Error when compiling shader {}", path);
				return nullptr;
			}

			module_cache_[path] = newShader;
		}
		return &module_cache_[path];
	}

	void ShaderCache::Clear()
	{
		for (auto &[k, v] : module_cache_)
		{
			vkDestroyShaderModule(VulkanEngine::Get().device_, v.module, nullptr);
		}
		for (ShaderEffect *effect : shader_effect_cache_)
		{
			delete effect;
		}
		module_cache_.clear();
	}
}