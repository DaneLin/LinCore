#pragma once
// std
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
// external
#include <fastgltf/types.hpp>
// lincore
#include "graphics/scene_graph/scene_types.h"
#include "graphics/vk_resources.h"

namespace lincore
{
    class GpuDevice;
    class CommandBuffer;

    namespace scene
    {
           
        class SceneNode;
        /**
         * @brief 加载的GLTF场景
         * 管理所有加载的资源
         */
        class LoadedGLTF : public IDrawable
        {
        public:
            void Init(GpuDevice *gpu_device);
            void SetName(const std::string &name);
            const std::string &GetName() const { return name_; }
            // 资源管理
            std::vector<std::vector<uint8_t>> buffer_data_; // GLTF缓冲区数据
            std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes_;
            std::unordered_map<std::string, std::shared_ptr<MaterialInstance>> materials_;
            std::vector<std::shared_ptr<SceneNode>> nodes_;
            std::vector<std::shared_ptr<SceneNode>> root_nodes_;

            // 资源管理方法
            void AddMesh(const std::string &name, std::shared_ptr<MeshAsset> mesh);
            void AddMaterial(const std::string &name, std::shared_ptr<MaterialInstance> material);
            void AddNode(std::shared_ptr<SceneNode> node);

            // 实现IDrawable接口
            virtual void Draw(const glm::mat4 &transform, DrawContext &context) override;
            virtual Bounds GetBounds() const override;
            virtual bool IsVisible(const DrawContext& context) const override { return true; }

            // 资源查找
            std::shared_ptr<MeshAsset> FindMesh(const std::string &name) const;
            std::shared_ptr<MaterialInstance> FindMaterial(const std::string &name) const;
            std::shared_ptr<SceneNode> FindNode(const std::string &name) const;

            // 资源清理
            void ClearAll();
            void ReleaseCPUData();

            /**
             * @brief 统计信息
             * 包含网格、材质、节点、三角形、顶点数量和GPU内存使用量
             */
            struct Statistics
            {
                size_t mesh_count{0};
                size_t material_count{0};
                size_t node_count{0};
                size_t triangle_count{0};
                size_t vertex_count{0};
                size_t gpu_memory_usage{0};
            };
            Statistics GetStatistics() const;

        private:
            std::string name_;
            GpuDevice *gpu_device_{nullptr};
        };

        class GLTFLoader
        {
        public:
            // 主加载接口
            static std::shared_ptr<LoadedGLTF> LoadGLTF(
                GpuDevice *device,
                const std::string &path,
                const LoadConfig &config = {});

        private:
            // 加载上下文
            struct LoadContext
            {
                fastgltf::Asset &asset;             // GLTF资产
                GpuDevice *device{nullptr};         // GPU设备
                std::filesystem::path base_path;    // 基础路径(用于外部资源)
                scene::LoadConfig config;           // 加载配置
                std::shared_ptr<LoadedGLTF> output; // 输出结果
                BufferHandle material_data_buffer_handle{ k_invalid_buffer }; // 材质数据缓冲区句柄

                // 缓存
                std::vector<std::shared_ptr<scene::MaterialInstance>> material_cache; // 材质缓存
                std::vector<std::shared_ptr<scene::MeshAsset>> mesh_cache;            // 网格缓存
                std::vector<uint32_t> texture_cache;                                  // 贴图缓存
                std::vector<std::shared_ptr<scene::SceneNode>> node_cache;           // 节点缓存

                LoadContext(GpuDevice *dev, const LoadConfig &cfg, fastgltf::Asset &ast,
                            const std::filesystem::path &path)
                    : device(dev), config(cfg), asset(ast), base_path(path)
                {
                    output = std::make_shared<LoadedGLTF>();
                    output->Init(dev);
                    output->SetName(cfg.debug_name);
                    texture_cache.resize(ast.textures.size(), UINT32_MAX);
                }
            };

            // 分步加载函数
            static bool LoadBuffers(LoadContext &ctx);
            static bool LoadMaterials(LoadContext &ctx);
            static bool LoadMeshes(LoadContext &ctx);
            static bool LoadNodes(LoadContext &ctx);

            // 辅助函数
            static bool LoadExternalBuffer(const std::filesystem::path &path, std::vector<uint8_t> &out_data);
            static void LogLoadingError(const std::string &message);

            // 贴图加载
            static uint32_t LoadTexture(LoadContext &ctx, size_t texture_index);
            static bool LoadTextureFromImage(LoadContext &ctx, const fastgltf::Image &image, TextureHandle &out_texture_handle);
            static SamplerHandle CreateSampler(LoadContext &ctx, const fastgltf::Sampler &sampler);

            // 辅助函数
            static VkFilter ExtractFilter(std::optional<fastgltf::Filter> filter);
            static VkSamplerMipmapMode ExtractMipmapMode(std::optional<fastgltf::Filter> filter);
            static VkSamplerAddressMode ExtractAddressMode(fastgltf::Wrap wrap);
        };
    } // namespace scene
} // namespace lincore
