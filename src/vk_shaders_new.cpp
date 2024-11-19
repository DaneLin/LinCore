#include "vk_shaders_new.h"
#include "vk_initializers.h"
#include "logging.h"

#include "vk_engine.h"

#include <fstream>
#include <algorithm>
#include "spirv_reflect.h"
#include <spirv-headers/spirv.h>

namespace lc {



	bool vkutil::load_shader(VkDevice device, const char* filePath, ShaderModule* outShaderModule)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			LOGE("Failed to open file: %s", filePath);
			return false;
		}

		size_t fileSize = (size_t)file.tellg();

		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

		file.seekg(0);

		file.read((char*)buffer.data(), fileSize);

		file.close();

		VkShaderModuleCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr };
		createInfo.codeSize = buffer.size() * sizeof(uint32_t);
		createInfo.pCode = buffer.data();

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

		outShaderModule->code = std::move(buffer);
		outShaderModule->module = shaderModule;
		return true;
	}


	// FNV-1a 32bit hashing algorithm
	constexpr uint32_t fnv1a_32(char const* s, std::size_t count) {
		return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}

	uint32_t vkutil::hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info)
	{
		// put all the data into a string and then hash the string
		std::stringstream ss;

		ss << info->flags;
		ss << info->bindingCount;

		for (uint32_t i = 0; i < info->bindingCount; ++i) {
			const VkDescriptorSetLayoutBinding& binding = info->pBindings[i];

			ss << binding.binding;
			ss << binding.descriptorCount;
			ss << binding.descriptorType;
			ss << binding.stageFlags;
		}
		auto str = ss.str();

		return fnv1a_32(str.c_str(), str.length());
	}

	void ShaderEffect::add_stage(ShaderModule* shaderModule, VkShaderStageFlagBits stage)
	{
		ShaderStage newStage = { shaderModule, stage };
		stages.push_back(newStage);
	}

	struct DescriptorSetLayoutData {
		uint32_t set_number;
		VkDescriptorSetLayoutCreateInfo create_info;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};


	void ShaderEffect::reflect_layout(VkDevice device, ReflectionOverrides* overrides, int overrideCount, uint32_t overrideConstantSize)
	{
		std::vector<DescriptorSetLayoutData> set_Layouts;

		std::vector<VkPushConstantRange> constantRanges;

		for (auto& s : stages) {
			SpvReflectShaderModule spvModule;
			SpvReflectResult result = spvReflectCreateShaderModule(s.module->code.size() * sizeof(uint32_t), s.module->code.data(), &spvModule);

			uint32_t count = 0;
			result = spvReflectEnumerateDescriptorSets(&spvModule, &count, nullptr);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectDescriptorSet*> sets(count);
			result = spvReflectEnumerateDescriptorSets(&spvModule, &count, sets.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			for (size_t idx = 0; idx < sets.size(); idx++) {
				const SpvReflectDescriptorSet& reflectSet = *(sets[idx]);

				DescriptorSetLayoutData layout = {};

				layout.bindings.resize(reflectSet.binding_count);
				for (uint32_t i_binding = 0; i_binding < reflectSet.binding_count; i_binding++)
				{
					const SpvReflectDescriptorBinding& reflectBinding = *(reflectSet.bindings[i_binding]);
					VkDescriptorSetLayoutBinding& layoutBinding = layout.bindings[i_binding];
					layoutBinding.binding = reflectBinding.binding;
					layoutBinding.descriptorType = static_cast<VkDescriptorType>(reflectBinding.descriptor_type);

					for (int ov = 0; ov < overrideCount; ov++) {
						if (strcmp(reflectBinding.name, overrides[ov].name) == 0) {
							layoutBinding.descriptorType = overrides[ov].overridenType;
						}
					}

					layoutBinding.descriptorCount = 1;
					for (uint32_t i_dim = 0; i_dim < reflectBinding.array.dims_count; ++i_dim) {
						layoutBinding.descriptorCount *= reflectBinding.array.dims[i_dim];
					}
					layoutBinding.stageFlags = static_cast<VkShaderStageFlagBits>(spvModule.shader_stage);

					ReflectedBinding reflected;
					reflected.binding = layoutBinding.binding;
					reflected.set = reflectSet.set;
					reflected.type = layoutBinding.descriptorType;

					bindings[reflectBinding.name] = reflected;
				}
				layout.set_number = reflectSet.set;
				layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layout.create_info.bindingCount = reflectSet.binding_count;
				layout.create_info.pBindings = layout.bindings.data();

				set_Layouts.push_back(layout);
			}

			// push constants

			result = spvReflectEnumeratePushConstantBlocks(&spvModule, &count, nullptr);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectBlockVariable*> pconstants(count);
			result = spvReflectEnumeratePushConstantBlocks(&spvModule, &count, pconstants.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			if (count > 0) {
				VkPushConstantRange pcs{};
				pcs.offset = pconstants[0]->offset;
				pcs.size = overrideConstantSize == -1 ? pconstants[0]->size : overrideConstantSize;
				pcs.stageFlags = s.stage;

				constantRanges.push_back(pcs);
			}
		}

		std::array<DescriptorSetLayoutData, 4> mergedLayouts;

		for (int i = 0; i < 4; i++) {
			DescriptorSetLayoutData& layout = mergedLayouts[i];

			layout.set_number = i;

			layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

			std::unordered_map<int, VkDescriptorSetLayoutBinding> binds;
			for (auto& s : set_Layouts) {
				if (s.set_number == i) {
					for (auto& b : s.bindings) {
						auto it = binds.find(b.binding);
						if (it == binds.end()) {
							binds[b.binding] = b;
						}
						else {
							// merge flags
							binds[b.binding].stageFlags |= b.stageFlags;
						}
					}
				}
			}

			for (auto [k, v] : binds) {
				layout.bindings.push_back(v);
			}

			// sort the bindings, for hash purposes
			std::sort(layout.bindings.begin(), layout.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
				return a.binding < b.binding;
				});

			layout.create_info.bindingCount = static_cast<uint32_t>(layout.bindings.size());
			layout.create_info.pBindings = layout.bindings.data();
			layout.create_info.flags = 0;
			layout.create_info.pNext = 0;

			if (layout.create_info.bindingCount > 0) {
				setHashes[i] = vkutil::hash_descriptor_layout_info(&layout.create_info);
				vkCreateDescriptorSetLayout(device, &layout.create_info, nullptr, &setLayouts[i]);
			}
			else {
				setHashes[i] = 0;
				setLayouts[i] = VK_NULL_HANDLE;
			}
		}

		// we start from just the default empty pipeline layout info
		VkPipelineLayoutCreateInfo pipelineCreateInfo = vkinit::pipeline_layout_create_info();

		pipelineCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(constantRanges.size());
		pipelineCreateInfo.pPushConstantRanges = constantRanges.data();

		std::array<VkDescriptorSetLayout, 4> compactedLayouts;
		int s = 0;
		for (int i = 0; i < 4; i++) {
			if (setLayouts[i] != VK_NULL_HANDLE) {
				compactedLayouts[s] = setLayouts[i];
				s++;
			}
		}

		pipelineCreateInfo.setLayoutCount = s;
		pipelineCreateInfo.pSetLayouts = compactedLayouts.data();

		VK_CHECK(vkCreatePipelineLayout(device, &pipelineCreateInfo, nullptr, &builtLayout));

	}

	void ShaderEffect::fill_stage(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages)
	{
		for (auto& s : stages) {
			pipelineStages.push_back(vkinit::pipeline_shader_stage_create_info(s.stage, s.module->module));
		}
	}

	VkPipelineBindPoint ShaderEffect::get_bind_point() const
	{

		for (const auto& stage : stages) {
			switch (stage.stage) {
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
		for (int i = 0; i < 4; i++) {
			if (setLayouts[i] != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(VulkanEngine::Get()._device, setLayouts[i], nullptr);
			}
		}
	}


	void ShaderDescriptorBinder::bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo)
	{
		bind_dynamic_buffer(name, -1, bufferInfo);
	}

	void ShaderDescriptorBinder::bind_image(const char* name, const VkDescriptorImageInfo& imageInfo)
	{
		if (shaders == nullptr) {
			LOGE("No shader set");
			return;
		}

		auto found = shaders->bindings.find(name);
		if (found != shaders->bindings.end()) {
			const ShaderEffect::ReflectedBinding& bind = (*found).second;

			for (auto& write : imageWrites) {
				if (write.dstBinding == bind.binding && write.dstSet == bind.set) {
					if (write.imageInfo.sampler != imageInfo.sampler ||
						write.imageInfo.imageView != imageInfo.imageView ||
						write.imageInfo.imageLayout != imageInfo.imageLayout) {
						write.imageInfo = imageInfo;
					}
					return;
				}
			}

			ImageWriteDescriptor newWrite;
			newWrite.dstSet = bind.set;
			newWrite.dstBinding = bind.binding;
			newWrite.descriptorType = bind.type;
			newWrite.imageInfo = imageInfo;

			imageWrites.push_back(newWrite);
		}
	}

	void ShaderDescriptorBinder::bind_dynamic_buffer(const char* name, uint32_t offset, const VkDescriptorBufferInfo& bufferInfo)
	{
		if (shaders == nullptr) {
			LOGE("No shader set");
			return;
		}

		auto found = shaders->bindings.find(name);
		if (found != shaders->bindings.end()) {
			const ShaderEffect::ReflectedBinding& bind = (*found).second;

			for (auto& write : bufferWrites) {
				if (write.dstBinding == bind.binding && write.dstSet == bind.set) {
					if (write.bufferInfo.buffer != bufferInfo.buffer ||
						write.bufferInfo.range != bufferInfo.range ||
						write.bufferInfo.offset != bufferInfo.offset) {
						write.bufferInfo = bufferInfo;
						write.dynamic_offset = offset;

						cachedDescriptorSets[write.dstSet] = VK_NULL_HANDLE;
					}
					else {
						// already in the write lists, but matches buffer
						write.dynamic_offset = offset;
					}
					return;
				}
			}

			BufferWriteDescriptor newWrite;
			newWrite.dstSet = bind.set;
			newWrite.dstBinding = bind.binding;
			newWrite.descriptorType = bind.type;
			newWrite.bufferInfo = bufferInfo;
			newWrite.dynamic_offset = offset;

			cachedDescriptorSets[bind.set] = VK_NULL_HANDLE;

			bufferWrites.push_back(newWrite);
		}
	}

	void ShaderDescriptorBinder::apply_binds(VkCommandBuffer cmd, VkPipelineLayout layout)
	{
		for (size_t i = 0; i < cachedDescriptorSets.size(); ++i) {
			if (cachedDescriptorSets[i] != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(cmd, shaders->get_bind_point(), shaders->builtLayout, i, 1, &cachedDescriptorSets[i], setOffsets[i].count, setOffsets[i].offset.data());
			}
		}
	}

	void ShaderDescriptorBinder::build_sets(VkDevice device, DescriptorAllocatorGrowable& allocator)
	{
		std::array<std::vector<VkWriteDescriptorSet>, 4> writes{};

		std::sort(bufferWrites.begin(), bufferWrites.end(), [](BufferWriteDescriptor& a, BufferWriteDescriptor& b) {
			if (b.dstBinding == a.dstBinding) {
				return a.dstSet < b.dstSet;
			}
			else {
				return a.dstBinding < b.dstBinding;
			}
			});

		// reset the dynamic offsets
		for (auto& s : setOffsets) {
			s.count = 0;
		}

		for (BufferWriteDescriptor& w : bufferWrites) {
			uint32_t set = w.dstSet;
			VkWriteDescriptorSet write = vkinit::write_descriptor_buffer(w.descriptorType, VK_NULL_HANDLE, &w.bufferInfo, w.dstBinding);

			writes[set].push_back(write);

			// dynamic offsets
			if (w.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
				w.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
				DynOffset& offsetSet = setOffsets[set];
				offsetSet.offset[offsetSet.count] = w.dynamic_offset;
				offsetSet.count++;
			}
		}

		for (ImageWriteDescriptor& w : imageWrites) {
			uint32_t set = w.dstSet;
			VkWriteDescriptorSet write = vkinit::write_descriptor_image(w.descriptorType, VK_NULL_HANDLE, &w.imageInfo, w.dstBinding);

			writes[set].push_back(write);
		}

		for (size_t i = 0; i < cachedDescriptorSets.size(); ++i) {
			if (writes[i].size() > 0) {
				if (cachedDescriptorSets[i] == VK_NULL_HANDLE) {
					// alloc
					auto layout = shaders->setLayouts[i];

					VkDescriptorSet newDescriptor = allocator.allocate(device, layout);

					for (auto& w : writes[i]) {
						w.dstSet = newDescriptor;
					}

					vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes[i].size()), writes[i].data(), 0, nullptr);

					cachedDescriptorSets[i] = newDescriptor;
				}
			}
		}

	}

	void ShaderDescriptorBinder::set_shader(ShaderEffect* newShader)
	{
		// invalidate nonequal layouts
		if (shaders && shaders != newShader) {
			for (size_t i = 0; i < cachedDescriptorSets.size(); i++) {
				if (newShader->setHashes[i] != shaders->setHashes[i]) {
					cachedDescriptorSets[i] = VK_NULL_HANDLE;
				}
				else if (newShader->setHashes[i] == 0) {
					cachedDescriptorSets[i] = VK_NULL_HANDLE;
				}
			}
		}
		else {
			for (size_t i = 0; i < cachedDescriptorSets.size(); i++) {
				cachedDescriptorSets[i] = VK_NULL_HANDLE;
			}
		}

		shaders = newShader;
	}

	void ShaderDescriptorBinder::destroy()
	{
		delete shaders;
	}

	ShaderModule* ShaderCache::get_shader(const std::string& path)
	{
		auto it = module_cache.find(path);
		if (it == module_cache.end()) {
			ShaderModule newShader;

			bool result = vkutil::load_shader(VulkanEngine::Get()._device, path.c_str(), &newShader);
			if (!result) {
				LOGE("Error when compiling shader {}", path);
				return nullptr;
			}

			module_cache[path] = newShader;
		}
		return &module_cache[path];
	}

	void ShaderCache::clear()
	{
		for (auto& [k, v] : module_cache) {
			vkDestroyShaderModule(VulkanEngine::Get()._device, v.module, nullptr);
		}
		module_cache.clear();
	}
}