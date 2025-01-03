#pragma once
#include "scene_types.h"
#include "scene_node.h"
#include <unordered_map>
#include <memory>

namespace lincore
{
    class CommandBuffer;
    class GpuDevice;
    class ShaderEffect;
    namespace scene
    {
        class SceneNode;
        class LoadedGLTF;
        /**
         * @brief 场景图类
         * 管理场景对象和渲染资源
         */

        /**
         * @brief GPU资源池
         * 包含网格数据和实例数据缓冲区
         */
        struct GPUResourcePool
        {
            // 网格数据缓冲区
            BufferHandle vertex_buffer;
            BufferHandle index_buffer;
            size_t vertex_capacity{0};
            size_t index_capacity{0};

            // 实例数据缓冲区
            BufferHandle instance_data_buffer; // 存储全部 ObjectData
            size_t instance_capacity{0};

            // 绘制计数
            uint32_t draw_count{0};

            // 间接绘制缓冲区
            BufferHandle draw_indirect_buffer;  // 存储 DrawCommand
            size_t draw_command_capacity{0};

            // 添加材质缓冲区
            BufferHandle material_buffer; // 存储所有MaterialInstance
            size_t material_capacity{0};
            uint32_t current_material_count_{0};
            // 暂存缓冲区
            BufferHandle staging_buffer;

            void Init(GpuDevice *device, const SceneConfig &config);
            void Shutdown(GpuDevice *device);
            // 确保缓冲容量足够
            void EnsureVertexCapacity(size_t size);
            void EnsureIndexCapacity(size_t size);
            void EnsureInstanceCapacity(size_t size);
            void EnsureDrawCommandCapacity(size_t size);
        };

        class SceneGraph
        {
        public:
            explicit SceneGraph(GpuDevice *device);
            ~SceneGraph();

            // 初始化和清理
            void Init(const SceneConfig &config = {});
            void Shutdown();

            // 场景构建接口
            void BeginSceneUpdate();
            void EndSceneUpdate();
            bool IsBuilding() const { return is_building_; }

            // 节点管理
            std::shared_ptr<SceneNode> CreateNode(const std::string &name);
            std::shared_ptr<SceneNode> FindNode(const std::string &name) const;
            void RemoveNode(const std::string &name);
            std::shared_ptr<SceneNode> GetRootNode() { return root_node_; }

            // 资源管理
            void AddMesh(std::shared_ptr<MeshAsset> mesh);
            void RemoveMesh(const std::string &name);
            void RemoveMaterial(const std::string &name);
            uint32_t GetMaterialIndex(const MaterialInstance *material) const;

            // GLTF场景
            void AddGLTFScene(std::shared_ptr<LoadedGLTF> gltf);
            void RemoveGLTFScene(const std::string &name);

            // 遍历和查询
            template <typename Filter, typename Func>
            void TraverseFiltered(Filter &&filter, Func &&func) const
            {
                if (!root_node_)
                    return;
                root_node_->Traverse([&](SceneNode *node)
                                     {
                    if (filter(node)) {
                        func(node);
                    } });
            }

            // 更新
            void Update(); // 每帧调用

            // 场景查询
            const Bounds &GetSceneBounds() const { return scene_bounds_; }
            size_t GetNodeCount() const { return node_lookup_.size(); }

            // 获取GPU资源
            GPUResourcePool &GetGPUResourcePool() { return gpu_resources_; }

            void ReadyCullingData(CommandBuffer *cmd);

        private:
            // GPU资源
            GpuDevice *device_{nullptr};

            // 场景数据
            std::shared_ptr<SceneNode> root_node_;
            std::unordered_map<std::string, std::shared_ptr<SceneNode>> node_lookup_;
            std::unordered_map<const MaterialInstance *, uint32_t> material_indices_;
            uint32_t next_material_index_{0};
            
            // 场景状态
            Bounds scene_bounds_;
            bool is_building_{false};
            bool needs_update_{false};

            /**
             * @brief 待上传数据
             * 包含网格数据和实例数据
             */
            struct PendingData
            {
                // 网格数据
                std::vector<Vertex> vertices;
                std::vector<uint32_t> indices;
                size_t total_vertex_size{0};
                size_t total_index_size{0};

                // 实例数据
                std::vector<ObjectData> instance_data; // 对象数据数组

                // 材质数据
                std::vector<MaterialInstance> materials;

                // 绘制间接缓冲
                std::vector<DrawCommand> draw_commands;

                void Reset();

                void Clear()
                {
                    vertices.clear();
                    indices.clear();
                    instance_data.clear();
                    draw_commands.clear();
                    materials.clear();
                    total_vertex_size = 0;
                    total_index_size = 0;
                }
            } pending_;

            GPUResourcePool gpu_resources_;

            void UpdateTransforms();
            void UpdateBounds();
            void UploadPendingData();
            void MergeGLTFScene(const LoadedGLTF &gltf);
            void CollectInstanceData(const std::shared_ptr<SceneNode> &node, const glm::mat4 &parent_transform);
            void AddNodeToLookup(const std::shared_ptr<SceneNode> &node);

            friend class SceneNode;
        };
    } // namespace scene
} // namespace lincore