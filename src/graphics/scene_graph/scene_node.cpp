#include "scene_node.h"

namespace lincore::scene
{

    uint32_t SceneNode::next_id_ = 0;

    SceneNode::SceneNode(const std::string &name)
        : name_(name), id_(++next_id_)
    {
    }

    void SceneNode::AddChild(std::shared_ptr<SceneNode> child)
    {
        if (!child)
            return;

        if (auto old_parent = child->parent_.lock())
        {
            old_parent->RemoveChild(child->name_);
        }

        child->parent_ = shared_from_this();
        child->UpdateWorldTransform();
        children_.push_back(std::move(child));
    }

    void SceneNode::RemoveChild(const std::string &name)
    {
        auto it = std::find_if(children_.begin(), children_.end(),
                               [&name](const auto &child)
                               { return child->name_ == name; });

        if (it != children_.end())
        {
            (*it)->parent_.reset();
            children_.erase(it);
        }
    }

    void SceneNode::RemoveFromParent()
    {
        if (auto parent = parent_.lock())
        {
            auto it = std::find_if(parent->children_.begin(), parent->children_.end(),
                                   [this](const auto &child)
                                   { return child.get() == this; });

            if (it != parent->children_.end())
            {
                parent->children_.erase(it);
            }
        }
        parent_.reset();
    }

    void SceneNode::SetLocalTransform(const Transform &transform)
    {
        local_transform_ = transform;

        if (auto parent = parent_.lock())
        {
            UpdateWorldTransform();
        }
    }

    void SceneNode::SetWorldTransform(const Transform &transform)
    {
        if (auto parent = parent_.lock())
        {
            // 计算本地变换 = 世界变换 * 父节点世界变换的逆
            glm::mat4 local = glm::inverse(parent->world_matrix_) * transform.GetMatrix();
            local_transform_.Decompose(local);
        }
        else
        {
            local_transform_ = transform;
        }
        UpdateWorldTransform();
    }

    void SceneNode::SetBounds(const Bounds &bounds)
    {
        local_bounds_ = bounds;
        UpdateWorldBounds();
    }
    void SceneNode::RefreshTransform(const glm::mat4 &parent_matrix)
    {
        world_matrix_ = parent_matrix * local_transform_.GetMatrix();
        // 先更新自身bounds
        world_bounds_ = local_bounds_;
        world_bounds_.Transform(world_matrix_);

        // 递归更新子节点
        for (auto &child : children_)
        {
            child->RefreshTransform(world_matrix_);
            world_bounds_.Merge(child->world_bounds_);
        }
    }

    void SceneNode::UpdateWorldTransform()
    {
        // 计算世界矩阵
        glm::mat4 local = local_transform_.GetMatrix();
        world_matrix_ = parent_.lock() ? parent_.lock()->world_matrix_ * local : local;

        // 更新包围盒
        UpdateWorldBounds();

        // 递归更新子节点
        for (auto &child : children_)
        {
            child->UpdateWorldTransform();
        }
    }

    void SceneNode::UpdateWorldBounds()
    {
        // 变换局部包围盒到世界空间
        world_bounds_ = local_bounds_;
        world_bounds_.Transform(world_matrix_);

        // 合并所有子节点的包围盒
        for (const auto &child : children_)
        {
            world_bounds_.Merge(child->world_bounds_);
        }
    }

    void SceneNode::Draw(const glm::mat4 &transform, DrawContext &context)
    {
        if (!visible_ || !mesh_)
            return;

        // 计算当前节点的世界变换
        glm::mat4 world_transform = transform * GetWorldMatrix();

        // 如果有渲染对象收集器且有网格
        if (context.render_objects && mesh_)
        {
            // 创建简化的渲染对象
            RenderObject render_object;
            render_object.object_id = id_;                  // 使用节点ID
            render_object.is_static = context.force_static; // 使用上下文的静态标志

            // draw_command_index 和 instance_data 会在后续的渲染过程中设置
            // 这里不需要设置，因为实际的实例数据已经在 SceneGraph 中收集

            context.render_objects->push_back(render_object);
        }

        // 递归绘制子节点
        for (auto &child : children_)
        {
            child->Draw(world_transform, context);
        }
    }

    bool SceneNode::IsVisible(const DrawContext &context) const
    {
        return true;
    }

} // namespace lincore::scene
