#pragma once
// std
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
// external
#include "volk.h"
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <fmt/core.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
// lincore
#include "foundation/resources.h"
#include "graphics/vk_resources.h"

namespace lincore
{

	// 全局描述符结构
	struct GlobalDescriptorSets
	{
		VkDescriptorSet scene_data_set;	   // 场景数据（相机、灯光等）
		VkDescriptorSet material_data_set; // 材质数据
		VkDescriptorSet texture_array_set; // bindless纹理数组
	};

	// Mesh buffers on GPU
	struct Vertex
	{
		glm::vec3 position;
		float uv_x;
		glm::vec3 normal;
		float uv_y;
		glm::vec4 color;
	};

	struct RenderBounds
	{
		glm::vec3 origin;
		float radius;
		glm::vec3 extents;
		bool valid;
	};

	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		BufferHandle vertex_buffer_handle;
		BufferHandle index_buffer_handle;

		RenderBounds render_bounds;
	};

	enum class PassType : uint8_t
	{
		kCompute,
		kRaster,
		kMesh
	};


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

	// holds the resources needed for a mesh
	struct GPUMeshBuffers
	{
		BufferHandle index_buffer_handle;
		BufferHandle vertex_buffer_handle;
		size_t indirect_index;
		VkDeviceAddress vertex_buffer_address;
	};

	// push constants for our mesh object draws
	struct alignas(16) GPUDrawPushConstants
	{
		glm::mat4 world_matrix;
		VkDeviceAddress vertex_buffer_address;
	};

	struct alignas(16) GPUDrawIndirectPushConstants
	{
		glm::mat4 world_matrix;
	};

	enum class MeshPassType : uint8_t
	{
		kMainColor,
		kTransparent,
		kDirectionalShadow,
		kOther
	};

	template <typename T>
	struct PerPassData
	{
	public:
		T &operator[](MeshPassType pass)
		{
			switch (pass)
			{
			case MeshPassType::kMainColor:
				return data[0];
			case MeshPassType::kTransparent:
				return data[1];
			case MeshPassType::kDirectionalShadow:
				return data[2];
			}
			assert(false);
			return data[0];
		}

		void Clear(T &&val)
		{
			for (int i = 0; i < 3; i++)
			{
				data[i] = val;
			}
		}

	private:
		std::array<T, 3> data;
	};

	struct MaterialPipeline
	{
		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	struct MaterialInstance
	{
		MaterialPipeline *pipeline;
		VkDescriptorSet set;
		MeshPassType pass_type;
	};

	struct Bounds
	{
		glm::vec3 origin;
		float sphere_radius;
		glm::vec3 extents;
	};

	struct RenderObject
	{
		uint32_t index_count;
		uint32_t first_index;
		uint32_t indirect_draw_index;
		VkDeviceAddress vertex_buffer_address;
		// VkBuffer index_buffer;
		BufferHandle vertex_buffer_handle;
		BufferHandle index_buffer_handle;

		MaterialInstance *material;
		Bounds bounds;

		bool is_static = false; // 是否是静态物体
		// 动态物体使用transform
		glm::mat4 transform;
	};

	static Vertex TransformVertex(const Vertex &vertex, const glm::mat4 &transform)
	{
		Vertex transformed = vertex;

		// 变换位置
		glm::vec4 transformed_pos = transform * glm::vec4(vertex.position, 1.0f);
		transformed.position = glm::vec3(transformed_pos) / transformed_pos.w;

		// 变换法线 (使用法线矩阵)
		glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));
		transformed.normal = normal_matrix * vertex.normal;
		transformed.normal = glm::normalize(transformed.normal);

		return transformed;
	}

	struct DrawContext
	{
		std::vector<RenderObject> opaque_surfaces;

		std::vector<RenderObject> transparent_surfaces;
	};

	class IRenderable
	{
		virtual void Draw(const glm::mat4 &top_matrix, DrawContext &ctx) = 0;
	};

	struct Node : public IRenderable
	{
		std::weak_ptr<Node> parent;
		std::vector<std::shared_ptr<Node>> children;

		glm::mat4 local_transform;
		glm::mat4 world_transform;

		void RefreshTransform(const glm::mat4 &parent_matrix)
		{
			world_transform = parent_matrix * local_transform;
			for (auto child : children)
			{
				child->RefreshTransform(world_transform);
			}
		}

		virtual void Draw(const glm::mat4 &top_matrix, DrawContext &ctx)
		{
			// draw children
			for (auto &c : children)
			{
				c->Draw(top_matrix, ctx);
			}
		}
	};

}
