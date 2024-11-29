
#include <vk_loader.h>

#include "stb_image.h"
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>


#include "logging.h"

namespace lc
{
	std::optional<AllocatedImage> LoadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image) {
		AllocatedImage new_image{};
		int width, height, channels;

		std::visit(
			fastgltf::visitor{
				[](auto& arg) {},
				[&](fastgltf::sources::URI& file_path) {
					assert(file_path.fileByteOffset == 0); // 不支持偏移
					assert(file_path.uri.isLocalPath()); // 只支持本地文件

					const std::string path(file_path.uri.path().begin(), file_path.uri.path().end());


					unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
					if (data) {
						VkExtent3D image_size;
						image_size.width = width;
						image_size.height = height;
						image_size.depth = 1;

						new_image = engine->CreateImage(data, image_size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
						stbi_image_free(data);
					}
					 else {
						LOGE("Failed to load image from file.");
					}
				},
				[&](fastgltf::sources::Vector& vector) {
					unsigned char* data = stbi_load_from_memory(
						reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
						static_cast<int>(vector.bytes.size()),
						&width, &height, &channels, 4
					);
					if (data) {
						VkExtent3D image_size;
						image_size.width = width;
						image_size.height = height;
						image_size.depth = 1;

						new_image = engine->CreateImage(data, image_size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
						stbi_image_free(data);
					}
					else {
						LOGE("Failed to load image from memory vector.");
					}
				},
				[&](fastgltf::sources::BufferView& view) {
					auto& bufferView = asset.bufferViews[view.bufferViewIndex];
					auto& buffer = asset.buffers[bufferView.bufferIndex];

					std::visit(fastgltf::visitor {
						[](auto& arg) {},
						[&](fastgltf::sources::Array& vector) {
							unsigned char* data = stbi_load_from_memory(
								reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
								static_cast<int>(bufferView.byteLength),
								&width, &height, &channels, 4
							);
							if (data) {

								VkExtent3D image_size;
								image_size.width = width;
								image_size.height = height;
								image_size.depth = 1;

								new_image = engine->CreateImage(data, image_size, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
								stbi_image_free(data);
							}
							else {
								LOGE("Failed to load image from buffer view.");
							}
						}
					}, buffer.data);
				}
			},
			image.data
		);

		if (new_image.image == VK_NULL_HANDLE) {
			LOGE("{} Image load failed, returning empty optional.", image.name);
			return {};
		}
		else {
			return new_image;
		}
	}


	VkFilter ExtractFilter(fastgltf::Filter filter) {
		switch (filter) {
			// nearest samplers
		case fastgltf::Filter::Nearest:
		case fastgltf::Filter::NearestMipMapNearest:
		case fastgltf::Filter::NearestMipMapLinear:
			return VK_FILTER_NEAREST;

			// linear samplers
		case fastgltf::Filter::Linear:
		case fastgltf::Filter::LinearMipMapNearest:
		case fastgltf::Filter::LinearMipMapLinear:
		default:
			return VK_FILTER_LINEAR;
		}
	}

	VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter)
	{
		switch (filter) {
		case fastgltf::Filter::NearestMipMapNearest:
		case fastgltf::Filter::LinearMipMapNearest:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;

		case fastgltf::Filter::NearestMipMapLinear:
		case fastgltf::Filter::LinearMipMapLinear:
		default:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
	}

	void AddMeshBufferToGlobalBuffers(std::span<uint32_t> indices, std::span<Vertex> vertices)
	{
		VulkanEngine* engine = &VulkanEngine::Get();

		uint32_t vertex_offset = static_cast<uint32_t>(engine->global_mesh_buffer_.vertex_data.size());
		uint32_t index_offset = static_cast<uint32_t>(engine->global_mesh_buffer_.index_data.size());

		engine->global_mesh_buffer_.vertex_data.insert(engine->global_mesh_buffer_.vertex_data.end(), vertices.begin(), vertices.end());
		engine->global_mesh_buffer_.index_data.insert(engine->global_mesh_buffer_.index_data.end(), indices.begin(), indices.end());

		VkDrawIndexedIndirectCommand cmd = {};
		cmd.firstIndex = index_offset;
		cmd.indexCount = static_cast<uint32_t>(indices.size());
		cmd.vertexOffset = vertex_offset;
		cmd.instanceCount = 1;
		cmd.firstInstance = 0;

		engine->global_mesh_buffer_.indirect_commands.push_back(cmd);
	}

	std::optional<std::shared_ptr<LoadedGLTF>> LoadGltf(VulkanEngine* engine, std::string_view file_path)
	{
		LOGI("Loading GLTF: {}", file_path);

		std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
		scene->creator = engine;
		LoadedGLTF& file = *scene.get();

		fastgltf::Parser parser{};

		constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;

		auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
		fastgltf::Asset gltf;
		std::filesystem::path path = file_path;

		auto type = fastgltf::determineGltfFileType(data.get());
		if (type == fastgltf::GltfType::glTF) {
			auto load = parser.loadGltf(data.get(), path.parent_path(), gltf_options);
			if (load) {
				gltf = std::move(load.get());
			}
			else {
				LOGE("Failed to load gltf: {}", fastgltf::to_underlying(load.error()));
				return {};
			}
		}
		else if (type == fastgltf::GltfType::GLB) {
			auto load = parser.loadGltfBinary(data.get(), path.parent_path(), gltf_options);
			if (load) {
				gltf = std::move(load.get());
			}
			else {
				LOGE("Failed to load gltf: {}", fastgltf::to_underlying(load.error()));
				return {};
			}
		}
		else {
			LOGE("Failed to determine glTF container");
			return {};
		}

		// we can stimate the descriptors we will need accurately
		std::vector<lc::DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };

		file.descriptor_pool.Init(engine->device_, static_cast<uint32_t>(gltf.materials.size()), sizes);

		// load sampler
		for (fastgltf::Sampler& sampler : gltf.samplers) {
			VkSamplerCreateInfo sampler_create_info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
			sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
			sampler_create_info.minLod = 0;

			sampler_create_info.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
			sampler_create_info.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			sampler_create_info.mipmapMode = ExtractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			VkSampler new_sampler;
			vkCreateSampler(engine->device_, &sampler_create_info, nullptr, &new_sampler);

			file.samplers.push_back(new_sampler);
		}

		// temporal arrays for all the objects to use while creating the GLTF data
		std::vector<std::shared_ptr<MeshAsset>> meshes;
		std::vector<std::shared_ptr<Node>> nodes;
		std::vector<AllocatedImage> images;
		std::vector<TextureID> imageIDs;
		std::vector<std::shared_ptr<GLTFMaterial>> materials;

		// load all textures
		for (fastgltf::Image& image : gltf.images) {
			std::optional<AllocatedImage> img = LoadImage(engine, gltf, image);

			if (img.has_value()) {
				images.push_back(*img);
				file.images[image.name.c_str()] = *img;
			}
			else {
				// we failed to load, so lets give the slot a default white texture to not completely break loading
				images.push_back(engine->default_images_.error_checker_board_image);
				LOGW("gltf failed to load texture {}, fallint to default error texture", image.name);
			}
		}

		file.material_data_buffer = engine->CreateBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU);
		int data_index = 0;
		GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*)file.material_data_buffer.info.pMappedData;

		for (fastgltf::Material& mat : gltf.materials) {
			std::shared_ptr<GLTFMaterial> new_mat = std::make_shared<GLTFMaterial>();
			materials.push_back(new_mat);
			file.materials[mat.name.c_str()] = new_mat;

			GLTFMetallic_Roughness::MaterialConstants constants;
			constants.color_factors.x = mat.pbrData.baseColorFactor[0];
			constants.color_factors.y = mat.pbrData.baseColorFactor[1];
			constants.color_factors.z = mat.pbrData.baseColorFactor[2];
			constants.color_factors.w = mat.pbrData.baseColorFactor[3];

			constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
			constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

			MeshPassType pass_type = MeshPassType::kMainColor;
			if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
				pass_type = MeshPassType::kTransparent;
			}

			GLTFMetallic_Roughness::MaterialResources material_resources;
			// TODO: Replace with real images
			material_resources.color_image = engine->default_images_.white_image;
			material_resources.color_sampler = engine->default_samplers_.linear;
			material_resources.metal_rough_image = engine->default_images_.white_image;
			material_resources.metal_rought_sampler = engine->default_samplers_.linear;

			material_resources.data_buffer = file.material_data_buffer.buffer;
			material_resources.data_buffer_offset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);

			// grab textures from gltf file
			if (mat.pbrData.baseColorTexture.has_value()) {
				size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

				material_resources.color_image = images[img];
				material_resources.color_sampler = file.samplers[sampler];
			}

			constants.color_tex_id = engine->texture_cache_.AddTexture(engine->device_, material_resources.color_image.view, material_resources.color_sampler).index;
			constants.metal_rought_tex_id = engine->texture_cache_.AddTexture(engine->device_, material_resources.metal_rough_image.view, material_resources.metal_rought_sampler).index;

			// write material parameter to buffer
			sceneMaterialConstants[data_index] = constants;
			// Build material
			new_mat->data = engine->metal_rough_material_.WriteMaterial(engine->device_, pass_type, material_resources, file.descriptor_pool);

			data_index++;
		}

		// use the same vectors for all meshes so that the memory doesnt reallocate as often
		std::vector<uint32_t> indices;
		std::vector<Vertex> vertices;
		int mesh_index = 0;

		for (fastgltf::Mesh& mesh : gltf.meshes) {
			std::shared_ptr<MeshAsset> new_mesh = std::make_shared<MeshAsset>();
			meshes.push_back(new_mesh);
			file.meshes[mesh.name.c_str()] = new_mesh;
			new_mesh->name = mesh.name;

			// clear the mesh arrays each mesh, we dont want to merge them by error
			indices.clear();
			vertices.clear();

			for (auto&& p : mesh.primitives) {
				GeoSurface new_surface;
				new_surface.start_index = (uint32_t)indices.size();
				new_surface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

				size_t initial_vtx = vertices.size();

				// load indexes
				{
					fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
					indices.reserve(indices.size() + indexaccessor.count);

					fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
						[&](std::uint32_t idx) {
							indices.push_back(static_cast<uint32_t>(idx + initial_vtx));
						});
				}

				// load vertex positions
				{
					fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
					vertices.resize(vertices.size() + posAccessor.count);

					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
						[&](glm::vec3 v, size_t index) {
							Vertex newvtx;
							newvtx.position = v;
							newvtx.normal = { 1, 0, 0 };
							newvtx.color = glm::vec4{ 1.f };
							newvtx.uv_x = 0;
							newvtx.uv_y = 0;
							vertices[initial_vtx + index] = newvtx;
						});
				}

				// load vertex normals
				auto normals = p.findAttribute("NORMAL");
				if (normals != p.attributes.end()) {

					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
						[&](glm::vec3 v, size_t index) {
							vertices[initial_vtx + index].normal = v;
						});
				}

				// load UVs
				auto uv = p.findAttribute("TEXCOORD_0");
				if (uv != p.attributes.end()) {

					fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
						[&](glm::vec2 v, size_t index) {
							vertices[initial_vtx + index].uv_x = v.x;
							vertices[initial_vtx + index].uv_y = v.y;
						});
				}

				// load vertex colors
				auto colors = p.findAttribute("COLOR_0");
				if (colors != p.attributes.end()) {

					fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
						[&](glm::vec4 v, size_t index) {
							vertices[initial_vtx + index].color = v;
						});
				}

				if (p.materialIndex.has_value()) {
					new_surface.material = materials[p.materialIndex.value()];
				}
				else {
					new_surface.material = materials[0];
				}

				// loop the vertice of this surface. find min/max bounds
				glm::vec3 min_pos = vertices[initial_vtx].position;
				glm::vec3 max_pos = vertices[initial_vtx].position;
				for (size_t i = initial_vtx; i < vertices.size(); i++) {
					min_pos = glm::min(min_pos, vertices[i].position);
					max_pos = glm::max(max_pos, vertices[i].position);
				}

				// calculate origin and extents from the min/max, use extent length for radius
				new_surface.bounds.origin = (min_pos + max_pos) * 0.5f;
				new_surface.bounds.extents = (max_pos - min_pos) * 0.5f;
				auto halfExtents = new_surface.bounds.extents;
				new_surface.bounds.sphere_radius = glm::max(glm::max(halfExtents.x, halfExtents.y), halfExtents.z);

				new_mesh->surfaces.push_back(new_surface);
			}
			new_mesh->mesh_buffers = engine->UploadMesh(indices, vertices);
			new_mesh->mesh_buffers.indirect_index = engine->global_mesh_buffer_.indirect_commands.size();
			AddMeshBufferToGlobalBuffers(indices, vertices);
		}

		// load all nodes and their meshes
		for (fastgltf::Node& node : gltf.nodes) {
			std::shared_ptr<Node> new_node;

			if (node.meshIndex.has_value()) {
				new_node = std::make_shared<MeshNode>();
				static_cast<MeshNode*>(new_node.get())->mesh = meshes[*node.meshIndex];
			}
			else {
				new_node = std::make_shared<MeshNode>();
			}

			nodes.push_back(new_node);

			file.nodes[node.name.c_str()];

			std::visit(fastgltf::visitor{ [&](fastgltf::math::fmat4x4 matrix) {
											  memcpy(&new_node->local_transform, matrix.data(), sizeof(matrix));
										  },
						   [&](fastgltf::TRS transform) {
							   glm::vec3 tl(transform.translation[0], transform.translation[1],
								   transform.translation[2]);
							   glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
								   transform.rotation[2]);
							   glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

							   glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
							   glm::mat4 rm = glm::toMat4(rot);
							   glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

							   new_node->local_transform = tm * rm * sm;
						   } },
				node.transform);
		}

		// run loop again to setup transform hierarchy
		for (int i = 0; i < gltf.nodes.size(); ++i) {
			fastgltf::Node& node = gltf.nodes[i];
			std::shared_ptr<Node>& scene_node = nodes[i];

			for (auto& c : node.children) {
				scene_node->children.push_back(nodes[c]);
				nodes[c]->parent = scene_node;
			}
		}

		// find the top nodes, with no parents
		for (auto& node : nodes) {
			if (node->parent.lock() == nullptr) {
				file.top_nodes.push_back(node);
				node->RefreshTransform(glm::mat4{ 1.f });
			}
		}
		return scene;
	}

	void LoadedGLTF::Draw(const glm::mat4& top_matrix, DrawContext& ctx)
	{
		for (auto& node : top_nodes) {
			node->Draw(top_matrix, ctx);
		}
	}

	void LoadedGLTF::ClearAll()
	{
		VkDevice dv = creator->device_;

		descriptor_pool.DestroyPools(dv);
		creator->DestroyBuffer(material_data_buffer);

		for (auto& [k, v] : meshes) {

			creator->DestroyBuffer(v->mesh_buffers.index_buffer);
			creator->DestroyBuffer(v->mesh_buffers.vertex_buffer);
		}

		for (auto& [k, v] : images) {

			if (v.image == creator->default_images_.error_checker_board_image.image) {
				//dont destroy the default images
				continue;
			}
			creator->DestroyImage(v);
		}

		for (auto& sampler : samplers) {
			vkDestroySampler(dv, sampler, nullptr);
		}

	}
	void RunPinnedTaskLoopTask::Execute()
	{
		task_scheduler->WaitForNewPinnedTasks();
		// this thread will 'sleep' until there are new pinned tasks
		task_scheduler->RunPinnedTasks();
	}
	void AsynchronousLoadTask::Execute()
	{
		while (execute)
		{
			async_loader->Update();
		}
	}
	void AsynchronousLoader::Init()
	{

		for (uint32_t idx = 0; idx < kFRAME_OVERLAP; ++idx)
		{
			VkCommandPoolCreateInfo cmd_pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmd_pool_info.queueFamilyIndex = VulkanEngine::Get().transfer_queue_family_;
			cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			vkCreateCommandPool(VulkanEngine::Get().device_, &cmd_pool_info, nullptr, &command_pool[idx]);

			VkCommandBufferAllocateInfo cmd_buffer_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cmd_buffer_info.commandPool = command_pool[idx];
			cmd_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmd_buffer_info.commandBufferCount = 1;

			vkAllocateCommandBuffers(VulkanEngine::Get().device_, &cmd_buffer_info, &command_buffer[idx]);
		}

		staging_buffer = VulkanEngine::Get().CreateBuffer(kSTAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		VkSemaphoreCreateInfo semaphore_create_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(VulkanEngine::Get().device_, &semaphore_create_info, nullptr, &transfer_complete_semaphore);

		VkFenceCreateInfo fence_create_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(VulkanEngine::Get().device_, &fence_create_info, nullptr, &transfer_fence);
	}
	void AsynchronousLoader::Update()
	{
		if (cpu_buffer_ready.index != kInvalidIndex && gpu_buffer_ready.index != kInvalidIndex)
		{
			assert(completed != nullptr);
			(*completed)++;

			cpu_buffer_ready = kInvalidBufferHandle;
			gpu_buffer_ready = kInvalidBufferHandle;
			completed = nullptr;
		}

		texture_ready.index = kInvalidTextureHandle.index;

		// Process upload request
		if (upload_requests.size())
		{

		}
	}
}


