// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct AllocatedBufferUntyped {
	VkBuffer _buffer{};
	VmaAllocation _allocation{};
	VkDeviceSize _size{ 0 };
	VkDescriptorBufferInfo get_info(VkDeviceSize offset = 0);
};

inline VkDescriptorBufferInfo AllocatedBufferUntyped::get_info(VkDeviceSize offset)
{
	VkDescriptorBufferInfo info;
	info.buffer = _buffer;
	info.offset = offset;
	info.range = _size;
	return info;
}

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocations;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function)
	{
		deletors.push_back(function);
	};

	void flush()
	{
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			(*it)();
		}

		deletors.clear();
	}
};



/// <summary>
/// We will use this structure to hold the data for a given buffer. 
/// We have the VkBuffer which is the vulkan handle, 
/// and the VmaAllocation and VmaAllocationInfo wich contains metadata about the buffer and its allocation,
/// needed to be able to free the buffer.
/// </summary>
struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

// Mesh buffers on GPU
struct Vertex {
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
	glm::mat4 worldMatrix;
	VkDeviceAddress vertexBuffer;
};

/// <summary>
/// This is the structs we need for the material data. 
/// MaterialInstance will hold a raw pointer (non owning) into its MaterialPipeline which contains the real pipeline.
/// It holds a descriptor set too.
/// </summary>
enum class MaterialPass : uint8_t {
	MainColor,
	Transparent,
	Other
};

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
	MaterialPass passType;
};

struct DrawContext;

class IRenderable {
	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable {
	std::weak_ptr<Node> parent;
	std::vector<std::shared_ptr<Node>> children;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	void refresh_transform(const glm::mat4& parentMatrix) {
		worldTransform = parentMatrix * localTransform;
		for (auto child : children) {
			child->refresh_transform(worldTransform);
		}
	}

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) {
		// draw children
		for (auto& c : children) {
			c->draw(topMatrix, ctx);
		}
	}
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)