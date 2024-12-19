#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include "volk.h"
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include "vk_resources.h"
#include "resources.h"

namespace lincore
{
	struct DeletionQueue
	{
		std::deque<std::function<void()>> deletors;

		void PushFunction(std::function<void()>&& function)
		{
			deletors.push_back(function);
		};

		void Flush()
		{
			// reverse iterate the deletion queue to execute all the functions
			for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
			{
				(*it)();
			}

			deletors.clear();
		}
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

	struct GPUSceneData
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewproj;
		glm::vec4 ambient_color;
		glm::vec4 sunlight_direction; // w for sun power
		glm::vec4 sunlight_color;
	};

	// holds the resources needed for a mesh
	struct GPUMeshBuffers
	{
		/*AllocatedBuffer<uint32_t> index_buffer;
		AllocatedBuffer<Vertex> vertex_buffer;*/
		lincore::BufferHandle index_buffer_handle;
		lincore::BufferHandle vertex_buffer_handle;
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

	/// <summary>
	/// This is the structs we need for the material data.
	/// MaterialInstance will hold a raw pointer (non owning) into its MaterialPipeline which contains the real pipeline.
	/// It holds a descriptor set too.
	/// </summary>
	enum class MeshPassType : uint8_t
	{
		kMainColor,
		kTransparent,
		kDirectionalShadow,
		kOther
	};

	struct MaterialPipeline
	{
		VkPipeline pipeline;
		VkPipelineLayout layout;
	};

	struct MaterialInstance
	{
		MaterialPipeline* pipeline;
		VkDescriptorSet set;
		MeshPassType pass_type;
	};

	struct DrawContext;

	class IRenderable
	{
		virtual void Draw(const glm::mat4& top_matrix, DrawContext& ctx) = 0;
	};

	struct Node : public IRenderable
	{
		std::weak_ptr<Node> parent;
		std::vector<std::shared_ptr<Node>> children;

		glm::mat4 local_transform;
		glm::mat4 world_transform;

		void RefreshTransform(const glm::mat4& parent_matrix)
		{
			world_transform = parent_matrix * local_transform;
			for (auto child : children)
			{
				child->RefreshTransform(world_transform);
			}
		}

		virtual void Draw(const glm::mat4& top_matrix, DrawContext& ctx)
		{
			// draw children
			for (auto& c : children)
			{
				c->Draw(top_matrix, ctx);
			}
		}
	};

}
