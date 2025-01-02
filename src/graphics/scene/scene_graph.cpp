#include "graphics/scene/scene_graph.h"
#include "foundation/logging.h"
#include "graphics/vk_device.h"
#include "graphics/scene/gltf_loader.h"

namespace lincore
{
    namespace scene
    {

        // ----------------------- SceneGraph -----------------------
        void GPUResourcePool::Init(GpuDevice *device, const SceneConfig &config)
        {
            // 创建顶点缓冲
            BufferCreation buffer_creation{};
            buffer_creation.Reset()
                .Set(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     ResourceUsageType::Immutable,
                     config.max_vertices * sizeof(Vertex))
                .SetName("Scene_VertexBuffer")
                .SetDeviceOnly();
            vertex_buffer = device->CreateResource(buffer_creation);
            vertex_capacity = config.max_vertices;
            device->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)device->GetResource<Buffer>(vertex_buffer.index)->vk_buffer, "Scene_VertexBuffer");

            // 创建索引缓冲
            buffer_creation.Reset()
                .Set(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     ResourceUsageType::Immutable,
                     config.max_indices * sizeof(uint32_t))
                .SetName("Scene_IndexBuffer")
                .SetDeviceOnly();
            index_buffer = device->CreateResource(buffer_creation);
            index_capacity = config.max_indices;
            device->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)device->GetResource<Buffer>(index_buffer.index)->vk_buffer, "Scene_IndexBuffer");
            // 创建实例数据缓冲
            buffer_creation.Reset()
                .Set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     ResourceUsageType::Immutable,
                     config.max_objects * sizeof(ObjectData))
                .SetName("Scene_InstanceBuffer")
                .SetDeviceOnly();
            instance_data_buffer = device->CreateResource(buffer_creation);
            instance_capacity = config.max_objects;
            device->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)device->GetResource<Buffer>(instance_data_buffer.index)->vk_buffer, "Scene_InstanceBuffer");
            // 创建绘制间接缓冲
            buffer_creation.Reset()
                .Set(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     ResourceUsageType::Immutable,
                     config.max_objects * sizeof(DrawCommand))
                .SetName("Scene_DrawIndirectBuffer")
                .SetDeviceOnly();
            draw_indirect_buffer = device->CreateResource(buffer_creation);
            device->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)device->GetResource<Buffer>(draw_indirect_buffer.index)->vk_buffer, "Scene_DrawIndirectBuffer");

            // 创建材质缓冲区
            buffer_creation.Reset()
                .Set(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     ResourceUsageType::Immutable,
                     config.max_materials * sizeof(MaterialInstance))
                .SetName("Scene_MaterialBuffer")
                .SetDeviceOnly();
            material_buffer = device->CreateResource(buffer_creation);
            material_capacity = config.max_materials;
            device->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)device->GetResource<Buffer>(material_buffer.index)->vk_buffer, "Scene_MaterialBuffer");
            // 创建暂存缓冲
            buffer_creation.Reset()
                .Set(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     ResourceUsageType::Dynamic,
                     static_cast<uint32_t>(std::max({vertex_capacity * sizeof(Vertex), index_capacity * sizeof(uint32_t), instance_capacity * sizeof(ObjectData)})))
                .SetName("Scene_StagingBuffer")
                .SetPersistent();
            staging_buffer = device->CreateResource(buffer_creation);
            device->SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)device->GetResource<Buffer>(staging_buffer.index)->vk_buffer, "Scene_StagingBuffer");
        }

        void GPUResourcePool::Shutdown(GpuDevice *device)
        {
            if (vertex_buffer.IsValid())
                device->DestroyResource(vertex_buffer);
            if (index_buffer.IsValid())
                device->DestroyResource(index_buffer);
            if (instance_data_buffer.IsValid())
                device->DestroyResource(instance_data_buffer);
            if (draw_indirect_buffer.IsValid())
                device->DestroyResource(draw_indirect_buffer);
            if (staging_buffer.IsValid())
                device->DestroyResource(staging_buffer);
            if (material_buffer.IsValid())
                device->DestroyResource(material_buffer);
        }

        void GPUResourcePool::EnsureVertexCapacity(size_t size)
        {
            // TODO : 扩容顶点缓冲
        }

        void GPUResourcePool::EnsureIndexCapacity(size_t size)
        {
            // TODO : 扩容索引缓冲
        }

        void GPUResourcePool::EnsureInstanceCapacity(size_t size)
        {
            // TODO : 扩容实例数据缓冲
        }

        void GPUResourcePool::EnsureDrawCommandCapacity(size_t size)
        {
            // TODO : 扩容绘制间接缓冲
        }

        SceneGraph::SceneGraph(GpuDevice *device)
            : device_(device)
        {
            // 创建根节点
            root_node_ = std::make_shared<SceneNode>("root");
            node_lookup_["root"] = root_node_;
        }

        SceneGraph::~SceneGraph()
        {
            Shutdown();
        }

        void SceneGraph::Init(const SceneConfig &config)
        {
            // 初始化GPU资源
            gpu_resources_.Init(device_, config);

            pending_.Reset();
        }

        void SceneGraph::Shutdown()
        {
            // 1. 先清理查找表，确保没有其他引用
            node_lookup_.clear();

            // 2. 递归清理节点树
            root_node_ = nullptr;

            // 3. 清理其他资源
            materials_.clear();

            // 清理GPU资源
            gpu_resources_.Shutdown(device_);
        }

        void SceneGraph::BeginSceneUpdate()
        {
            // 清空pending_
            pending_.Clear();

            // 设置标志
            is_building_ = true;
            needs_update_ = true;

            // 3. 重置场景包围盒
            scene_bounds_ = Bounds();
        }

        void SceneGraph::EndSceneUpdate()
        {
            // 检查是否在构建中
            if (!is_building_)
            {
                LOGE("SceneGraph::EndSceneUpdate() called while not in building mode");
                return;
            }

            // 设置标志
            is_building_ = false;

            // 上传数据
            UploadPendingData();
        }

        std::shared_ptr<SceneNode> SceneGraph::CreateNode(const std::string &name)
        {
            // 检查是否已存在同名节点
            if (auto it = node_lookup_.find(name); it != node_lookup_.end())
            {
                return nullptr;
            }

            // 创建新节点
            auto node = std::make_shared<SceneNode>(name);
            node_lookup_[name] = node;
            return node;
        }

        void SceneGraph::RemoveNode(const std::string &name)
        {
            auto it = node_lookup_.find(name);
            if (it != node_lookup_.end())
            {
                auto node = it->second;
                node->RemoveFromParent();
                node_lookup_.erase(it);
            }
        }

        std::shared_ptr<SceneNode> SceneGraph::FindNode(const std::string &name) const
        {
            auto it = node_lookup_.find(name);
            return it != node_lookup_.end() ? it->second : nullptr;
        }

        void SceneGraph::AddMesh(std::shared_ptr<MeshAsset> mesh)
        {
            if (!mesh || mesh->name.empty())
            {
                LOGE("SceneGraph::AddMesh() called with null or empty mesh name");
                return;
            }
            if (!is_building_)
            {
                LOGE("SceneGraph::AddMesh() called while not in building mode");
                return;
            }

            // 记录当前顶点基址
            size_t vertex_base = pending_.total_vertex_size;
            mesh->vertex_offset = vertex_base;
            mesh->index_offset = pending_.total_index_size;

            // 2. 如果是静态网格，准备上传数据
            {
                // 2.1 确保GPU缓冲区容量足够
                gpu_resources_.EnsureVertexCapacity(mesh->mesh_data.vertices.size());
                gpu_resources_.EnsureIndexCapacity(mesh->mesh_data.indices.size());
                
                // 2.2 添加顶点数据
                pending_.vertices.insert(pending_.vertices.end(), 
                    mesh->mesh_data.vertices.begin(), 
                    mesh->mesh_data.vertices.end());

                // 2.3 添加索引数据，不需要调整索引值，因为DrawCommand中的vertex_offset会处理偏移
                pending_.indices.insert(pending_.indices.end(),
                    mesh->mesh_data.indices.begin(),
                    mesh->mesh_data.indices.end());

                pending_.total_vertex_size += mesh->mesh_data.vertices.size();
                pending_.total_index_size += mesh->mesh_data.indices.size();
            }

            needs_update_ = true;
        }

        void SceneGraph::AddMaterial(std::shared_ptr<MaterialInstance> material, const std::string &name)
        {
            if (!material)
                return;

            // 使用提供的名称或生成默认名称
            std::string material_name = name;
            if (material_name.empty())
            {
                material_name = "material_" + std::to_string(materials_.size());
            }

            // 添加到材质库中
            if (materials_.find(material_name) == materials_.end())
            {
                materials_[material_name] = material;
                material_indices_[material.get()] = next_material_index_++;
            }

            needs_update_ = true;
        }

        void SceneGraph::RemoveMesh(const std::string &name)
        {
            /*auto it = meshes_.find(name);
            if (it != meshes_.end())
            {
                meshes_.erase(it);
                needs_update_ = true;
            }*/
        }

        void SceneGraph::RemoveMaterial(const std::string &name)
        {
            auto it = materials_.find(name);
            if (it != materials_.end())
            {
                material_indices_.erase(it->second.get());
                materials_.erase(it);
                needs_update_ = true;
            }
        }

        uint32_t SceneGraph::GetMaterialIndex(const MaterialInstance *material) const
        {
            if (!material)
                return 0;
            auto it = material_indices_.find(material);
            return it != material_indices_.end() ? it->second : 0;
        }

        void SceneGraph::AddGLTFScene(std::shared_ptr<LoadedGLTF> gltf)
        {
            if (!gltf)
                return;

            // 合并GLTF场景
            MergeGLTFScene(*gltf);

            needs_update_ = true;
        }

        void SceneGraph::RemoveGLTFScene(const std::string &name)
        {
            // TODO : 移除GLTF场景
        }
        void SceneGraph::MergeGLTFScene(const LoadedGLTF &gltf)
        {
            // 1. 添加网格
            for (const auto &[name, mesh] : gltf.meshes_)
            {
                AddMesh(mesh);
            }

            // 2. 添加材质
            for (const auto &[name, material] : gltf.materials_)
            {
                AddMaterial(material, name);
            }

            // 3. 处理节点层级
            // 只需要将根节点添加到场景图的根节点下即可
            for (auto &root_node : gltf.root_nodes_)
            {
                root_node_->AddChild(root_node);
                CollectInstanceData(root_node, glm::mat4(1.0f));
                // 添加到查找表中（需要递归添加所有子节点）
                AddNodeToLookup(root_node);
            }

            needs_update_ = true;
        }

        void SceneGraph::CollectInstanceData(const std::shared_ptr<SceneNode> &node, const glm::mat4 &parent_transform)
        {
            // 计算当前节点的世界变换
            glm::mat4 world_transform = parent_transform * node->GetLocalTransform().GetMatrix();

            // 如果节点有网格，则收集实例数据
            if (node->GetMesh())
            {
                // 为每个surface创建一个实例
                for (size_t i = 0; i < node->GetMesh()->surfaces.size(); i++)
                {
                    // 创建实例数据
                    ObjectData instance_data;
                    instance_data.model = world_transform;
                    // 创建一个临时bounds并应用世界变换
                    Bounds world_bounds = node->GetMesh()->surfaces[i].bounds;
                    world_bounds.Transform(world_transform);

                    // 使用变换后的bounds
                    instance_data.sphere_bounds = world_bounds.GetSphere();
                    instance_data.extents = world_bounds.GetExtents();
                    if (node->GetMesh()->surfaces[i].material)
                    {
                        instance_data.material_index = GetMaterialIndex(node->GetMesh()->surfaces[i].material.get());
                        // 如果材质索引超出范围，则添加到待上传队列
                        if (instance_data.material_index >= gpu_resources_.current_material_count_)
                        {
                            pending_.materials.push_back(*node->GetMesh()->surfaces[i].material); // 将材质添加到待上传队列
                        }
                    }
                    instance_data.padding[0] = instance_data.padding[1] = instance_data.padding[2] = 0;

                    // 添加实例数据
                    pending_.instance_data.push_back(std::move(instance_data));

                    DrawCommand cmd{};
                    cmd.index_count = node->GetMesh()->surfaces[i].index_count;
                    cmd.instance_count = 1;
                    // 使用正确的索引和顶点偏移
                    cmd.first_index = static_cast<uint32_t>(node->GetMesh()->surfaces[i].first_index + node->GetMesh()->index_offset);
                    cmd.vertex_offset = static_cast<uint32_t>(node->GetMesh()->vertex_offset);  // 使用mesh的顶点偏移
                    cmd.first_instance = 0;
                    cmd.object_id = static_cast<uint32_t>(gpu_resources_.draw_count);
                    gpu_resources_.draw_count++;
                    pending_.draw_commands.push_back(cmd);
                }
            }

            // 递归收集子节点的实例数据
            for (const auto &child : node->GetChildren())
            {
                CollectInstanceData(child, world_transform);
            }
        }

        void SceneGraph::AddNodeToLookup(const std::shared_ptr<SceneNode> &node)
        {
            node_lookup_[node->GetName()] = node;
            for (const auto &child : node->GetChildren())
            {
                AddNodeToLookup(child);
            }
        }

        void SceneGraph::UploadPendingData()
        {
            // 更新顶点和索引数据
            if (!pending_.vertices.empty())
            {
                // 更新顶点数据
                device_->command_buffer_manager_.UploadBuffer(gpu_resources_.staging_buffer,
                                                              gpu_resources_.vertex_buffer,
                                                              pending_.vertices.data(),
                                                              pending_.vertices.size() * sizeof(Vertex));
                // 更新索引数据
                device_->command_buffer_manager_.UploadBuffer(gpu_resources_.staging_buffer,
                                                              gpu_resources_.index_buffer,
                                                              pending_.indices.data(),
                                                              pending_.indices.size() * sizeof(uint32_t));
            }

            // 更新实例数据
            if (!pending_.instance_data.empty())
            {
                device_->command_buffer_manager_.UploadBuffer(gpu_resources_.staging_buffer, gpu_resources_.instance_data_buffer, pending_.instance_data.data(), pending_.instance_data.size() * sizeof(ObjectData));
                LOGI("Upload instance data, structure size: {}", sizeof(ObjectData));
            }

            // 更新绘制间接缓冲
            if (!pending_.draw_commands.empty())
            {
                device_->command_buffer_manager_.UploadBuffer(gpu_resources_.staging_buffer, gpu_resources_.draw_indirect_buffer, pending_.draw_commands.data(), pending_.draw_commands.size() * sizeof(DrawCommand));
            }

            // 更新材质数据
            // if (!pending_.materials.empty())
            // {
            //     device_->command_buffer_manager_.UploadBuffer(gpu_resources_.staging_buffer, gpu_resources_.material_buffer, pending_.materials.data(), pending_.materials.size() * sizeof(MaterialInstance));
            //     gpu_resources_.current_material_count_ += pending_.materials.size();
            // }
        }

        void SceneGraph::Update()
        {
            if (!needs_update_)
                return;

            UpdateTransforms();
            UpdateBounds();
            UploadPendingData();

            needs_update_ = false;
        }

        void SceneGraph::ReadyCullingData(CommandBuffer *cmd)
        {
            // // copy from the cleared indirect buffer into the one we will use on rendering. This one happens every frame
            // vkCmdFillBuffer(cmd->vk_command_buffer_, device_->GetResource<Buffer>(gpu_resources_.draw_count_buffer.index)->vk_buffer, 0, sizeof(uint32_t), 0);
        }

        void SceneGraph::UpdateTransforms()
        {
            if (root_node_)
            {
                root_node_->RefreshTransform(glm::mat4(1.0f));
            }
        }

        void SceneGraph::UpdateBounds()
        {
            if (root_node_)
            {
                scene_bounds_ = root_node_->GetWorldBounds();
            }
        }
        void SceneGraph::PendingData::Reset()
        {
            Clear();
            total_vertex_size = 0;
            total_index_size = 0;
        }
    } // namespace scene
} // namespace lincore
