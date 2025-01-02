#pragma once
#include "graphics/scene/scene_types.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace lincore
{
    namespace scene
    {
        /**
         * @brief 场景节点类
         * 表示场景图中的一个节点，管理节点的层级关系和变换
         */
        class SceneNode : public IDrawable, 
                         public std::enable_shared_from_this<SceneNode>
        {
        public:
            // 构造和标识
            explicit SceneNode(const std::string &name);
            const std::string &GetName() const { return name_; }
            uint32_t GetId() const { return id_; }

            // 节点层级
            void AddChild(std::shared_ptr<SceneNode> child);
            void RemoveChild(const std::string &name);
            void RemoveFromParent();
            std::shared_ptr<SceneNode> GetParent() const { return parent_.lock(); }
            const std::vector<std::shared_ptr<SceneNode>> &GetChildren() const { return children_; }

            // 变换
            void SetLocalTransform(const Transform &transform);
            void SetWorldTransform(const Transform &transform);
            const Transform &GetLocalTransform() const { return local_transform_; }
            const glm::mat4 &GetWorldMatrix() const { return world_matrix_; }
            void RefreshTransform(const glm::mat4 &parent_matrix);

            // 包围盒
            void SetBounds(const Bounds &bounds);
            const Bounds &GetLocalBounds() const { return local_bounds_; }
            const Bounds &GetWorldBounds() const { return world_bounds_; }

            // 可见性
            void SetVisible(bool visible) { visible_ = visible; }
            bool IsVisible() const { return visible_; }

            // Mesh和Material
            void SetMesh(std::shared_ptr<MeshAsset> mesh) { mesh_ = mesh; }
            std::shared_ptr<MeshAsset> GetMesh() const { return mesh_; }

            // IDrawable接口实现
            virtual void Draw(const glm::mat4 &transform, scene::DrawContext &context) override;
            virtual Bounds GetBounds() const override { return world_bounds_; }
            virtual bool IsVisible(const scene::DrawContext &context) const override;

            // 遍历
            template <typename Func>
            void Traverse(Func &&func)
            {
                func(this);
                for (auto &child : children_)
                {
                    child->Traverse(std::forward<Func>(func));
                }
            }

        private:
            static uint32_t next_id_;

            std::string name_;
            uint32_t id_;
            bool visible_{true};

            // 层级关系
            std::weak_ptr<SceneNode> parent_;
            std::vector<std::shared_ptr<SceneNode>> children_;

            // 变换
            Transform local_transform_{};
            glm::mat4 world_matrix_{1.0f};

            // 包围盒
            Bounds local_bounds_;
            Bounds world_bounds_;

            // 渲染资源
            std::shared_ptr<MeshAsset> mesh_;

            void UpdateWorldTransform();
            void UpdateWorldBounds();
        };
    } // namespace scene
} // namespace lincore
