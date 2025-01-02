#include "vk_shaders.h"
// std
#include <fstream>
#include <algorithm>
// external
#include "spirv_reflect.h"
#include <spirv-headers/spirv.h>
#include <vulkan/vulkan.h>
// foundation
#include "foundation/logging.h"
#include "graphics/vk_initializers.h"
#include "graphics/vk_device.h"

namespace lincore
{

	bool vkutil::LoadShader(VkDevice device, const char *file_path, ShaderModule *out_shader_module)
	{
		std::ifstream file(file_path, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			LOGE("Failed to open file: {}", file_path);
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
		std::vector<VkDescriptorBindingFlags> binding_flags;
		VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info;
	};

	ShaderEffect::ShaderEffect(GpuDevice *gpu_device, const std::string &name)
		: gpu_device_(gpu_device), built_layout_(VK_NULL_HANDLE), name_(name)
	{
		for (int i = 0; i < 4; i++)
		{
			set_layouts_[i] = VK_NULL_HANDLE;
			set_hashes_[i] = 0;
			cached_descriptor_sets_[i] = VK_NULL_HANDLE;
		}
	}

	ShaderEffect::~ShaderEffect()
	{
		if (built_layout_ != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(gpu_device_->device_, built_layout_, nullptr);
		}

		for (auto layout : set_layouts_)
		{
			if (layout != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorSetLayout(gpu_device_->device_, layout, nullptr);
			}
		}
	}

	void ShaderEffect::ReflectLayout(uint32_t override_constant_size)
	{
		std::vector<DescriptorSetLayoutData> set_Layouts;

		std::vector<VkPushConstantRange> constant_ranges;
		// 清除现有的绑定
		bindings_.clear();

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
				bool is_bindless = false;
				for (uint32_t i_binding = 0; i_binding < reflectSet.binding_count; i_binding++)
				{
					const SpvReflectDescriptorBinding &reflectBinding = *(reflectSet.bindings[i_binding]);
					VkDescriptorSetLayoutBinding &layoutBinding = layout.bindings[i_binding];
					layoutBinding.binding = reflectBinding.binding;
					layoutBinding.descriptorType = static_cast<VkDescriptorType>(reflectBinding.descriptor_type);

					is_bindless = reflectBinding.binding == kBINDLESS_TEXTURE_BINDING;
					layoutBinding.descriptorCount = is_bindless ? kMAX_BINDLESS_RESOURCES : 1;
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

			// Add bindless support if needed
			bool has_bindless = false;
			for (const auto &binding : layout.bindings)
			{
				if (binding.binding == kBINDLESS_TEXTURE_BINDING)
				{
					has_bindless = true;
					break;
				}
			}

			if (has_bindless)
			{
				layout.create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

				// Set up binding flags
				layout.binding_flags.resize(layout.bindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
				layout.binding_flags.back() |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
											   VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

				layout.binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
				layout.binding_flags_create_info.bindingCount = static_cast<uint32_t>(layout.binding_flags.size());
				layout.binding_flags_create_info.pBindingFlags = layout.binding_flags.data();
				layout.binding_flags_create_info.pNext = nullptr;

				layout.create_info.pNext = &layout.binding_flags_create_info;
			}

			if (layout.create_info.bindingCount > 0)
			{
				set_hashes_[i] = vkutil::HashDescriptorLayoutInfo(&layout.create_info);
				vkCreateDescriptorSetLayout(gpu_device_->device_, &layout.create_info, nullptr, &set_layouts_[i]);
				gpu_device_->SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)set_layouts_[i], (name_ + "_set_" + std::to_string(i)).c_str());
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

		std::array<VkDescriptorSetLayout, 4> compacted_layouts;
		int s = 0;
		for (int i = 0; i < 4; i++)
		{
			if (set_layouts_[i] != VK_NULL_HANDLE)
			{
				compacted_layouts[s] = set_layouts_[i];
				s++;
			}
		}

		pipelineCreateInfo.setLayoutCount = s;
		pipelineCreateInfo.pSetLayouts = compacted_layouts.data();

		VK_CHECK(vkCreatePipelineLayout(gpu_device_->device_, &pipelineCreateInfo, nullptr, &built_layout_));
		gpu_device_->SetDebugName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)built_layout_, (name_ + "_layout").c_str());
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

	void ShaderEffect::BindBuffer(const char *name, const VkDescriptorBufferInfo &BufferInfo)
	{
		BindDynamicBuffer(name, -1, BufferInfo);
	}

	void ShaderEffect::BindImage(const char *name, const VkDescriptorImageInfo &image_info)
	{

		auto found = bindings_.find(name);
		if (found != bindings_.end())
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

	void ShaderEffect::BindDynamicBuffer(const char *name, uint32_t offset, const VkDescriptorBufferInfo &BufferInfo)
	{
		auto found = bindings_.find(name);
		if (found != bindings_.end())
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

	void ShaderEffect::ApplyBinds(VkCommandBuffer cmd)
	{
		for (size_t i = 0; i < cached_descriptor_sets_.size(); ++i)
		{
			// 不绑定bindless和空描述符集
			if (cached_descriptor_sets_[i] != VK_NULL_HANDLE && i != kBINDLESS_TEXTURE_SET_ID)
			{
				vkCmdBindDescriptorSets(cmd, GetBindPoint(), built_layout_, static_cast<uint32_t>(i), 1, &cached_descriptor_sets_[i], set_offsets_[i].count, set_offsets_[i].offset.data());
			}
		}
	}

	void ShaderEffect::BuildSets(DescriptorAllocatorGrowable *allocator)
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

		// clear cached descriptor sets
		for (auto &set : cached_descriptor_sets_)
		{
			set = VK_NULL_HANDLE;
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
					auto layout = set_layouts_[i];
					VkDescriptorSet new_descriptor;
					if (allocator)
					{
						new_descriptor = allocator->Allocate(gpu_device_->device_, layout);
					}
					else
					{
						new_descriptor = gpu_device_->descriptor_allocator_.Allocate(gpu_device_->device_, layout);
					}

					for (auto &w : writes[i])
					{
						w.dstSet = new_descriptor;
					}

					vkUpdateDescriptorSets(gpu_device_->device_, static_cast<uint32_t>(writes[i].size()), writes[i].data(), 0, nullptr);
					gpu_device_->SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)new_descriptor, "ShaderEffect");
					cached_descriptor_sets_[i] = new_descriptor;
				}
			}
		}
	}

	ShaderManager::~ShaderManager()
	{
		for (auto &[path, module] : module_cache_)
		{
			delete module;
		}
		module_cache_.clear();

		for (auto *effect : shader_effect_cache_)
		{
			delete effect;
		}
		shader_effect_cache_.clear();
	}

	void ShaderManager::Init(GpuDevice *gpu_device)
	{
		gpu_device_ = gpu_device;
	}

	void ShaderManager::Shutdown()
	{
		// 清理shader effects
		for (auto *effect : shader_effect_cache_)
		{
			delete effect;
		}
		shader_effect_cache_.clear();

		// 清理shader modules
		for (auto &[path, module] : module_cache_)
		{
			if (module->module != VK_NULL_HANDLE)
			{
				vkDestroyShaderModule(gpu_device_->device_, module->module, nullptr);
				module->module = VK_NULL_HANDLE;
			}
			delete module;
		}
		module_cache_.clear();

		gpu_device_ = nullptr;
	}

	ShaderEffect *ShaderManager::GetShaderEffect(std::initializer_list<std::string> file_paths, const std::string &name)
	{
		ShaderEffect *effect = new ShaderEffect(gpu_device_, name);

		for (const auto &path : file_paths)
		{
			// 加载shader module
			ShaderModule *shader_module = GetShader(path);
			if (!shader_module)
			{
				LOGE("Failed to load shader module: {}", path.c_str());
				delete effect; // 清理已创建的effect
				return nullptr;
			}

			// 使用SPIR-V反射获取shader stage
			SpvReflectShaderModule spv_module;
			SpvReflectResult result = spvReflectCreateShaderModule(
				shader_module->code.size() * sizeof(uint32_t),
				shader_module->code.data(),
				&spv_module);

			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				delete effect;
				return nullptr;
			}

			// 从SPIR-V反射中获取stage
			VkShaderStageFlagBits stage = static_cast<VkShaderStageFlagBits>(spv_module.shader_stage);

			// 清理反射数据
			spvReflectDestroyShaderModule(&spv_module);

			// 添加shader stage到同一个effect中
			effect->AddStage(shader_module, stage);
		}

		shader_effect_cache_.push_back(effect);
		return effect;
	}

	ShaderModule *ShaderManager::GetShader(const std::string &path)
	{
		// 检查缓存
		auto it = module_cache_.find(path);
		if (it != module_cache_.end())
		{
			return it->second;
		}

		// 创建新的shader module
		auto *shader_module = new ShaderModule();
		if (!vkutil::LoadShader(gpu_device_->device_, path.c_str(), shader_module))
		{
			delete shader_module;
			return nullptr;
		}

		module_cache_[path] = shader_module;
		return shader_module;
	}

}