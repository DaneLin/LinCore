#pragma once
#include <vk_types.h>
#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>

class VulkanEngine;

namespace lc
{
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


