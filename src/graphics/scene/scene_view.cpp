#include "scene_view.h"
#include "scene_graph.h"
#include "scene_node.h"
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include "foundation/logging.h"

namespace lincore
{
    namespace scene
    {
        void SceneView::SetCamera(Camera* camera)
        {
			camera_ = camera;
			view_matrix_ = camera->GetViewMatrix();
			projection_matrix_ = camera->GetProjectionMatrix();
			view_projection_matrix_ = projection_matrix_ * view_matrix_;

			// 更新视锥体平面
			frustum_.ExtractPlanes(view_projection_matrix_);
        }

        void SceneView::SetupShadowView(const glm::vec3 &light_dir, const Bounds &scene_bounds)
        {
            // 计算光源位置（从场景中心沿光照方向反向移动）
            glm::vec3 normalized_light_dir = glm::normalize(light_dir);
            glm::vec3 light_pos = scene_bounds.center - normalized_light_dir * scene_bounds.radius;

            // 创建光源视图矩阵（看向场景中心）
            view_matrix_ = glm::lookAt(
                light_pos,
                scene_bounds.center,
                glm::vec3(0.0f, 1.0f, 0.0f));

            // 创建正交投影矩阵（基于场景包围盒）
            float radius = scene_bounds.radius;
            projection_matrix_ = glm::ortho(
                -radius, radius,
                -radius, radius,
                0.1f, radius * 2.0f);

            // 更新视图投影矩阵
            view_projection_matrix_ = projection_matrix_ * view_matrix_;

            // 更新视锥体平面
            frustum_.ExtractPlanes(view_projection_matrix_);

            // 更新视图包围盒
            view_bounds_ = scene_bounds;
        }

        DrawCullData SceneView::GetCullData() const
        {
            DrawCullData cull_data{};

            // 设置视图矩阵
            cull_data.view = view_matrix_;

            // 从投影矩阵获取参数
            cull_data.P00 = projection_matrix_[0][0];
            cull_data.P11 = projection_matrix_[1][1];
            
            // 设置近远平面
            cull_data.znear = 0.1f;
            cull_data.zfar = config_.cull_distance;
            
            // 计算视锥体平面参数
            glm::mat4 projectionT = glm::transpose(projection_matrix_);
            
            // 计算并归一化左右平面 (x + w < 0)
            glm::vec4 frustumX = projectionT[3] + projectionT[0];
            float lenX = glm::length(glm::vec3(frustumX));
            frustumX /= lenX;
            
            // 计算并归一化上下平面 (y + w < 0)
            glm::vec4 frustumY = projectionT[3] + projectionT[1];
            float lenY = glm::length(glm::vec3(frustumY));
            frustumY /= lenY;
            
            // 设置视锥体参数
            cull_data.frustum[0] = frustumX.x;  // 左右平面的x分量
            cull_data.frustum[1] = frustumX.z;  // 左右平面的z分量
            cull_data.frustum[2] = frustumY.y;  // 上下平面的y分量
            cull_data.frustum[3] = frustumY.z;  // 上下平面的z分量
            
            // 设置剔除标志
            cull_data.culling_enabled = config_.enable_frustum_culling ? 1 : 0;
            cull_data.dist_cull = config_.enable_distance_culling ? 1 : 0;
            cull_data.aabb_check = config_.enable_aabb_check ? 1 : 0;

            // 如果启用AABB检查，设置AABB数据
            if (config_.enable_aabb_check)
            {
                cull_data.aabb_min[0] = view_bounds_.min.x;
                cull_data.aabb_min[1] = view_bounds_.min.y;
                cull_data.aabb_min[2] = view_bounds_.min.z;
                cull_data.aabb_max[0] = view_bounds_.max.x;
                cull_data.aabb_max[1] = view_bounds_.max.y;
                cull_data.aabb_max[2] = view_bounds_.max.z;
            }
            return cull_data;
        }

    } // namespace scene

} // namespace lincore
