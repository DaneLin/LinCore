#include "graphics/scene/scene_types.h"
#include "scene_types.h"
#include <memory>
#include <glm/gtx/matrix_decompose.hpp>
#include "graphics/vk_device.h"
namespace lincore
{
    namespace scene
    {
        //------------------------ Transform 实现 ------------------------//
        glm::mat4 Transform::GetMatrix() const
        {
            return glm::translate(glm::mat4(1.0f), position) *
                   glm::toMat4(rotation) *
                   glm::scale(glm::mat4(1.0f), scale);
        }

        void Transform::Decompose(const glm::mat4 &matrix)
        {
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(matrix, scale, rotation, position, skew, perspective);
        }

        //------------------------ Bounds 实现 ------------------------//
        void Bounds::SetMinMax(const glm::vec3 &min, const glm::vec3 &max)
        {
            this->min = min;
            this->max = max;
            this->center = (max + min) * 0.5f;
            this->extents = (max - min) * 0.5f;
            this->radius = glm::length(extents);
        }

        void Bounds::Merge(const Bounds &other)
        {
            if (other.min == other.max)
            {
                return; // 忽略无效包围盒}
            }
            if (min == max)
            { // 当前包围盒无效，直接使用other
                min = other.min;
                max = other.max;
            }
            else
            {
                min = glm::min(min, other.min);
                max = glm::max(max, other.max);
            }

            SetMinMax(min, max);
        }

        void Bounds::Transform(const glm::mat4 &matrix)
        {
            // 处理特殊情况
            if (min == max)
            {
                return;
            }

            // 变换8个角点，重新计算包围盒
            glm::vec3 corners[8] = {
                glm::vec3(min.x, min.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, max.y, max.z)};

            // 变换第一个点
            glm::vec4 transformed = matrix * glm::vec4(corners[0], 1.0f);
            min = max = glm::vec3(transformed) / transformed.w;

            // 变换其余点并更新包围盒
            for (int i = 1; i < 8; ++i)
            {
                transformed = matrix * glm::vec4(corners[i], 1.0f);
                glm::vec3 point = glm::vec3(transformed) / transformed.w;
                min = glm::min(min, point);
                max = glm::max(max, point);
            }

			SetMinMax(min, max);
        }

        bool Bounds::Contains(const glm::vec3 &point) const
        {
            // AABB包围盒
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y &&
                   point.z >= min.z && point.z <= max.z;
        }

        glm::vec4 Bounds::GetSphere() const
        {
            return glm::vec4(center, radius);
        }

        glm::vec4 Bounds::GetExtents() const
        {
            return glm::vec4(extents, 0.0f);
        }

        size_t MeshAsset::CalculateMemoryUsage() const
        {
            size_t vertex_size = sizeof(Vertex);
            size_t index_size = sizeof(uint32_t);
            return vertex_size * mesh_data.vertices.size() + index_size * mesh_data.indices.size();
        }

        void MeshAsset::ReleaseCPUData()
        {

            mesh_data.vertices.clear();
            mesh_data.indices.clear();
        }

        void Frustum::ExtractPlanes(const glm::mat4 &view_proj)
        {
            // 从视图投影矩阵提取视锥体平面
            // 每个平面由 ax + by + cz + d = 0 定义，存储为 (a,b,c,d)

            // 左平面
            planes[Left].x = view_proj[0][3] + view_proj[0][0];
            planes[Left].y = view_proj[1][3] + view_proj[1][0];
            planes[Left].z = view_proj[2][3] + view_proj[2][0];
            planes[Left].w = view_proj[3][3] + view_proj[3][0];

            // 右平面
            planes[Right].x = view_proj[0][3] - view_proj[0][0];
            planes[Right].y = view_proj[1][3] - view_proj[1][0];
            planes[Right].z = view_proj[2][3] - view_proj[2][0];
            planes[Right].w = view_proj[3][3] - view_proj[3][0];

            // 底平面
            planes[Bottom].x = view_proj[0][3] + view_proj[0][1];
            planes[Bottom].y = view_proj[1][3] + view_proj[1][1];
            planes[Bottom].z = view_proj[2][3] + view_proj[2][1];
            planes[Bottom].w = view_proj[3][3] + view_proj[3][1];

            // 顶平面
            planes[Top].x = view_proj[0][3] - view_proj[0][1];
            planes[Top].y = view_proj[1][3] - view_proj[1][1];
            planes[Top].z = view_proj[2][3] - view_proj[2][1];
            planes[Top].w = view_proj[3][3] - view_proj[3][1];

            // 近平面
            planes[Near].x = view_proj[0][3] + view_proj[0][2];
            planes[Near].y = view_proj[1][3] + view_proj[1][2];
            planes[Near].z = view_proj[2][3] + view_proj[2][2];
            planes[Near].w = view_proj[3][3] + view_proj[3][2];

            // 远平面
            planes[Far].x = view_proj[0][3] - view_proj[0][2];
            planes[Far].y = view_proj[1][3] - view_proj[1][2];
            planes[Far].z = view_proj[2][3] - view_proj[2][2];
            planes[Far].w = view_proj[3][3] - view_proj[3][2];

            // 归一化所有平面
            for (auto &plane : planes)
            {
                float length = glm::length(glm::vec3(plane));
                plane /= length;
            }
        }

        bool Frustum::ContainsSphere(const glm::vec3 &center, float radius) const
        {
            // 检查球体是否在所有平面的正面
            for (const auto &plane : planes)
            {
                float distance = glm::dot(plane, glm::vec4(center, 1.0f));

                if (distance < -radius)
                    return false; // 球体完全在平面的背面
            }

            return true; // 球体至少部分在所有平面的正面
        }

        bool Frustum::ContainsBox(const Bounds &bounds) const
        {
            // 获取AABB的8个顶点
            std::array<glm::vec3, 8> corners = {
                glm::vec3(bounds.min.x, bounds.min.y, bounds.min.z),
                glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z),
                glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z),
                glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z),
                glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z),
                glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z),
                glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z),
                glm::vec3(bounds.max.x, bounds.max.y, bounds.max.z)};

            // 检查每个平面
            for (const auto &plane : planes)
            {
                int out = 0;
                for (const auto &corner : corners)
                {
                    float distance = glm::dot(plane, glm::vec4(corner, 1.0f));
                    if (distance < 0)
                        out++;
                }

                if (out == 8) // 所有顶点都在平面背面
                    return false;
            }

            return true; // 至少部分在视锥体内
        }

    }
}
