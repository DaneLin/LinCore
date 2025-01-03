#pragma once
// std
#include <vector>
// lincore
#include "graphics/scene_graph/scene_types.h"
#include "graphics/camera.h"

namespace lincore
{
    namespace scene
    {

        class SceneView
        {
        public:
            /**
             * @brief 视图类型
             * 包含主相机视图、阴影视图、反射视图、自定义视图
             */
            enum class ViewType
            {
                Main,       // 主相机视图
                Shadow,     // 阴影视图
                Reflection, // 反射视图
                Custom      // 自定义视图
            };

            /**
             * @brief 视图配置
             * 包含视图配置参数
             */
            struct ViewConfig
            {
                bool enable_frustum_culling{true};
                bool enable_distance_culling{true};
                bool enable_aabb_check{false};
                float cull_distance{ 100000.f };
            };

            // 移除 SceneGraph 依赖
            SceneView() : view_type_(ViewType::Main) {}

            // 视图设置
            void SetViewType(ViewType type) { view_type_ = type; }
            void SetConfig(const ViewConfig &config) { config_ = config; }

            // 相机设置
			void SetCamera(Camera* camera);
            // 阴影视图设置
            void SetupShadowView(const glm::vec3 &light_dir, const Bounds &scene_bounds);

            // 获取剔除数据
            DrawCullData GetCullData() const;

        private:
            ViewType view_type_;
            ViewConfig config_;
            Camera* camera_{ nullptr };
            Frustum frustum_;
            Bounds view_bounds_;

            // 添加矩阵成员
            glm::mat4 view_matrix_{1.0f};
            glm::mat4 projection_matrix_{1.0f};
            glm::mat4 view_projection_matrix_{1.0f};
        };
    }
} // namespace lincore
