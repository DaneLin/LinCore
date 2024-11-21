#pragma once
#include <vk_types.h>
#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>
#include <TaskScheduler.h>

#include "config.h"



class VulkanEngine;

namespace lc
{
	// AsynchronousLoader class has the following responsibilities:
	// - Process load from file requests
    // - Process GPU upload transfers
    // - Manage a staging buffer to handle a copy of the data
    // - Enqueue the command buffers with copy commands
	// - Signal to the renderer that a texture has finished a transfer
    class AsynchronousLoader {
    public:
        enki::TaskScheduler* task_scheduler = nullptr;

        void Init()
        {

        }

        void Update()
        {

        }

        VkCommandPool command_pool[kFRAME_OVERLAP];
		VkCommandBuffer command_buffer[kFRAME_OVERLAP];
		AllocatedBuffer staging_buffer;
    };

    struct AsynchronousLoadTask : enki::IPinnedTask {
        void Execute() override;

        AsynchronousLoader* async_loader;
        enki::TaskScheduler* task_scheduler;
        bool execute = true;
    };

    struct RunPinnedTaskLoopTask :enki::IPinnedTask {
		void Execute() override;

        enki::TaskScheduler* task_scheduler;
        bool execute = true;
	}; // struct RunPinnedTaskLoopTask

    

    struct TextureID {
        uint32_t index;
    };

    class TextureCache {
    public:
        void SetDescriptorSet(VkDescriptorSet bindless_set) {
            bindless_set_ = bindless_set;
        }

        TextureID AddTexture(VkDevice device, const VkImageView& image_view, VkSampler sampler) {
            // 检查是否已存在
            for (uint32_t i = 0; i < cache_.size(); i++) {
                if (cache_[i].imageView == image_view && cache_[i].sampler == sampler) {
                    return TextureID{ i };
                }
            }
            // 检查是否超出最大数量
            if (cache_.size() >= kMAX_BINDLESS_RESOURCES) {
                throw std::runtime_error("Exceeded maximum bindless texture count");
            }

            uint32_t idx = static_cast<uint32_t>(cache_.size());

            // 添加到缓存
            VkDescriptorImageInfo image_info{
                .sampler = sampler,
                .imageView = image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            cache_.push_back(image_info);

            // 更新bindless descriptor set
            lc::DescriptorWriter writer;
            writer.WriteImageArray(kBINDLESS_TEXTURE_BINDING, idx, { image_info }, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            writer.UpdateSet(device, bindless_set_);

            return TextureID{ idx };
        }

    private:
        std::vector<VkDescriptorImageInfo> cache_;
        VkDescriptorSet bindless_set_;
    };

    struct GLTFMaterial {
        MaterialInstance data;
    };

    struct Bounds {
        glm::vec3 origin;
        float sphere_radius;
        glm::vec3 extents;
    };

    struct GeoSurface {
        uint32_t start_index;
        uint32_t count;
        Bounds bounds;
        std::shared_ptr<GLTFMaterial> material;
    };

    struct MeshAsset {
        std::string name;

        std::vector<GeoSurface> surfaces;
        GPUMeshBuffers mesh_buffers;
    };

    struct LoadedGLTF : public IRenderable {
        // storage for all the data on a given glTF file
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
        std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
        std::unordered_map<std::string, AllocatedImage> images;
        std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

        // nodes that dont have a parent, for iterating through the file in tree order
        std::vector<std::shared_ptr<Node>> top_nodes;

        std::vector<VkSampler> samplers;

        lc::DescriptorAllocatorGrowable descriptor_pool;

        AllocatedBuffer material_data_buffer;

        VulkanEngine* creator;

        ~LoadedGLTF() {
            ClearAll();
        }

        virtual void Draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
    private:
        void ClearAll();
    };


    std::optional<std::shared_ptr<LoadedGLTF>> LoadGltf(VulkanEngine* engine, std::string_view file_path);
} // namespace lc


