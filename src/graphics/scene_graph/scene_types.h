#pragma once
// std
#include <array>
// external
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/quaternion.hpp>
// lincore
#include "graphics/backend/vk_resources.h"
#include "graphics/backend/vk_descriptors.h"
namespace lincore
{
    class GpuDevice;
    namespace scene
    {
        /**
         * @brief GPU场景数据
         * 包含视图、投影、视图投影矩阵和太阳方向、颜色、相机位置
         */
        struct alignas(16) GPUSceneData
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 viewproj;
            glm::vec4 sunlight_direction; // w for sun power
            glm::vec4 sunlight_color;
            glm::vec3 camera_position;
            float pad0;
        };
        /**
         * @brief 3D变换组件
         * 包含位置、旋转和缩放信息
         * 提供矩阵转换和分解功能
         */
        struct Transform
        {
            glm::vec3 position{0.0f};
            glm::quat rotation{0.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 scale{1.0f};

            glm::mat4 GetMatrix() const;
            void Decompose(const glm::mat4 &matrix);
        };

        /**
         * @brief 3D包围体
         * 用于视锥体剔除和碰撞检测
         * 包含AABB和包围球信息
         */
        struct Bounds
        {
            glm::vec3 center{0.0f};  // 中心点
            glm::vec3 extents{0.0f}; // 范围（半长度）
            glm::vec3 min{0.0f};     // 最小点
            glm::vec3 max{0.0f};     // 最大点
            float radius{0.0f};      // 包围球半径

            void SetMinMax(const glm::vec3 &min, const glm::vec3 &max);
            void Merge(const Bounds &other);
            void Transform(const glm::mat4 &matrix);
            bool Contains(const glm::vec3 &point) const;
            glm::vec4 GetSphere() const;
            glm::vec4 GetExtents() const;
        };

        /**
         * @brief 混合模式
         * 定义材质的混合方式
         */
        enum class BlendMode
        {
            Opaque,      // 不透明
            Transparent, // 透明混合
            Additive,    // 加法混合
            Multiply     // 乘法混合
        };

        /**
         * @brief 材质标志位
         */
        enum MaterialFlags
        {
            MATERIAL_FLAG_DOUBLE_SIDED = 1 << 0, // 双面渲染
            MATERIAL_FLAG_BLEND = 1 << 1,        // 混合模式
            MATERIAL_FLAG_ALPHA_TEST = 1 << 2,   // Alpha测试
        };

        /**
         * @brief 材质实例
         * 包含材质参数和资源引用
         */
        struct alignas(16) MaterialInstance
        {
            glm::vec4 base_color_factor{1.0f};
            glm::vec3 emissive_factor{0.0f};
            float metallic_factor{1.0f};
            float roughness_factor{1.0f};
            float normal_scale{1.0f};
            float reflectance_factor{0.5f};
            float padding;

            uint32_t base_color_index{0};
            uint32_t metallic_roughness_index{0};
            uint32_t normal_index{0};
            uint32_t emissive_index{0};
        };

        /**
         * @brief 顶点数据
         * 包含位置、UV、法线和颜色信息
         */
        struct alignas(16) Vertex
        {
            glm::vec3 position; // offset 0  (12 bytes, padded to 16)
            float uv_x;
            glm::vec3 normal; // offset 16 (12 bytes, padded to 16)
            float uv_y;
            glm::vec4 color; // offset 32 (16 bytes)
        };

        /**
         * @brief 网格数据
         * 存储原始的顶点和索引数据
         */
        struct MeshData
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            Bounds bounds;
        };

        /**
         * @brief 几何表面
         * 表示一个子网格及其材质
         */
        struct GeoSurface
        {
            uint32_t first_index{0};                    // 索引起始位置
            uint32_t index_count{0};                    // 索引数量
            std::shared_ptr<MaterialInstance> material; // 材质实例
            Bounds bounds;                              // 局部空间包围盒
        };

        /**
         * @brief 网格资产
         * 包含完整的网格数据和GPU资源
         */
        struct MeshAsset
        {
            std::string name;
            MeshData mesh_data;               // 原始网格数据
            std::vector<GeoSurface> surfaces; // 几何表面列表
            Bounds bounds;                    // 整体包围盒
            // GPU资源
            size_t vertex_offset{0};
            size_t index_offset{0};
            bool is_static{true};

            // 计算总内存使用
            size_t CalculateMemoryUsage() const;

            // 释放CPU端数据
            void ReleaseCPUData();
        };

        struct DrawContext;
        /**
         * @brief 可绘制接口
         * 所有可渲染对象的基类
         */
        class IDrawable
        {
        public:
            virtual ~IDrawable() = default;

            // 绘制接口
            virtual void Draw(const glm::mat4 &transform, DrawContext &context) = 0;

            // 获取包围盒
            virtual Bounds GetBounds() const = 0;

            // 视锥体检测
            virtual bool IsVisible(const DrawContext &context) const = 0;
        };

        /**
         * @brief 加载配置
         */
        struct LoadConfig
        {
            std::string debug_name;       // 调试名称
            bool force_static{false};     // 强制静态
            bool generate_tangents{true}; // 生成切线
            bool optimize_mesh{true};     // 优化网格
            bool keep_cpu_data{false};    // 保留CPU数据
        };

        /**
         * @brief 网格上传数据
         * 包含顶点和索引数据
         */
        struct MeshUploadData
        {
            std::string mesh_name;
            size_t vertex_offset;
            size_t index_offset;
        };

        /**
         * @brief 对象数据
         * 包含模型矩阵和包围盒信息,包含所有GPU需要的实例数据
         */
        struct alignas(16) ObjectData
        {
            glm::mat4 model;            // 模型矩阵
            glm::vec4 sphere_bounds;    // 包围球 (xyz = center, w = radius)
            glm::vec4 extents;          // AABB范围
            uint32_t material_index{0}; // 材质索引
            uint32_t padding[3];        // 填充
        };

        /**
         * @brief 渲染对象
         * 表示一个CPU侧的渲染对象（用于收集和组织渲染数据）
         */
        struct RenderObject
        {
            uint32_t object_id;              // 对应SceneNode的ID
            uint32_t draw_command_index;     // 指向DrawCommand的索引
            const ObjectData *instance_data; // 指向对应的ObjectData
            bool is_static{true};            // 是否是静态对象
        };

        class SceneView;

        /**
         * @brief 渲染上下文
         * 包含渲染过程中需要的上下文信息
         */
        struct DrawContext
        {
            SceneView *view{nullptr}; // 渲染视图

            // AABB裁剪
            struct
            {
                bool enable{false};  // 是否启用AABB裁剪
                glm::vec3 min{0.0f}; // AABB最小点
                glm::vec3 max{0.0f}; // AABB最大点
            } aabb;

            // 渲染对象收集
            std::vector<RenderObject> *render_objects{nullptr};
            glm::mat4 parent_transform{1.0f};
            bool force_static{false};
        };

        /**
         * @brief GPU实例数据
         * 包含对象ID
         */
        struct GPUInstance
        {
            uint32_t object_id;
        };

        /**
         * @brief 间接绘制命令
         * 包含绘制参数
         */
        struct DrawCommand
        {
            uint32_t index_count{0};
            uint32_t instance_count{0}; // 由culling shader计算
            uint32_t first_index{0};
            uint32_t vertex_offset{0};
            uint32_t first_instance{0}; // 用来索引ObjectID
            uint32_t object_id{0};
            uint32_t padding[2]; // 填充
        };

        /**
         * @brief 剔除参数
         * 包含视图矩阵和投影矩阵参数
         */
        struct DrawCullData
        {
            glm::mat4 view;
            float P00, P11; // 投影矩阵参数
            float znear, zfar;
            float frustum[4]; // 视锥体平面数据

            uint32_t draw_count;
            int32_t culling_enabled;
            int32_t dist_cull;
            int32_t aabb_check;
            float aabb_min[3];
            float aabb_max[3];
        };

        /**
         * @brief 场景配置
         * 包含场景配置参数
         */
        struct SceneConfig
        {
            bool enable_frustum_culling{true};
            bool enable_occlusion_culling{false};
            bool enable_lod{false};
            float near_plane{0.1f};
            float far_plane{1000.0f};
            uint32_t max_objects{100000};
            uint32_t max_materials{100000};
            uint32_t max_vertices{1000000};
            uint32_t max_indices{3000000};
            uint32_t max_draw_commands{100000};
        };

        /**
         * @brief 视锥体
         * 包含视锥体平面数据
         */
        struct Frustum
        {
            enum Planes
            {
                Left = 0,
                Right,
                Bottom,
                Top,
                Near,
                Far,
                Count
            };
            std::array<glm::vec4, Count> planes;

            void ExtractPlanes(const glm::mat4 &view_proj);
            bool ContainsSphere(const glm::vec3 &center, float radius) const;
            bool ContainsBox(const Bounds &bounds) const;
        };
    } // namespace scene
} // namespace lincore