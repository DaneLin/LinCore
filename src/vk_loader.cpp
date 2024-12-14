
#include <vk_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>

#include "vk_device.h"
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
	std::optional<AllocatedImage> LoadImage(fastgltf::Asset &asset, fastgltf::Image &image, const std::filesystem::path &gltf_parent_path)
	{
		VulkanEngine* engine = &VulkanEngine::Get();
		auto promise = std::make_shared<std::promise<std::optional<AllocatedImage>>>();
		auto future = promise->get_future();

		std::visit(
			fastgltf::visitor{
				[](auto &arg) {},
				[&](fastgltf::sources::URI &file_path)
				{
					assert(file_path.fileByteOffset == 0);
					assert(file_path.uri.isLocalPath());

					const std::string path(file_path.uri.path().begin(), file_path.uri.path().end());

					std::filesystem::path relative_path(file_path.uri.path().begin(), file_path.uri.path().end());
					std::filesystem::path texture_path = gltf_parent_path / relative_path;

					engine->async_loader_.RequestFileLoad(texture_path.string().c_str(),
														  [promise](AllocatedImage image)
														  {
															  if (image.image != VK_NULL_HANDLE)
															  {
																  promise->set_value(std::move(image));
															  }
															  else
															  {
																  promise->set_value(std::nullopt);
															  }
														  });
				},
				[&](fastgltf::sources::Vector &vector)
				{
					engine->async_loader_.RequestVectorLoad(
						vector.bytes.data(),
						vector.bytes.size(),
						[promise](AllocatedImage image)
						{
							if (image.image != VK_NULL_HANDLE)
							{
								promise->set_value(std::move(image));
							}
							else
							{
								promise->set_value(std::nullopt);
							}
						});
				},
				[&](fastgltf::sources::BufferView &view)
				{
					auto &bufferView = asset.bufferViews[view.bufferViewIndex];
					auto &buffer = asset.buffers[bufferView.bufferIndex];

					std::visit(fastgltf::visitor{[](auto &arg) {},
												 [&](fastgltf::sources::Array &vector)
												 {
													 engine->async_loader_.RequestBufferViewLoad(
														 vector.bytes.data(),
														 bufferView.byteLength,
														 bufferView.byteOffset,
														 [promise](AllocatedImage image)
														 {
															 if (image.image != VK_NULL_HANDLE)
															 {
																 promise->set_value(std::move(image));
															 }
															 else
															 {
																 promise->set_value(std::nullopt);
															 }
														 });
												 }},
							   buffer.data);
				}},
			image.data);

		return future.get();
	}

	VkFilter ExtractFilter(fastgltf::Filter filter)
	{
		switch (filter)
		{
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
		switch (filter)
		{
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
		VulkanEngine *engine = &VulkanEngine::Get();

		uint32_t vertex_offset = static_cast<uint32_t>(engine->global_mesh_buffer_.vertex_data.size());
		uint32_t index_offset = static_cast<uint32_t>(engine->global_mesh_buffer_.index_data.size());

		engine->global_mesh_buffer_.vertex_data.insert(engine->global_mesh_buffer_.vertex_data.end(), vertices.begin(), vertices.end());
		engine->global_mesh_buffer_.index_data.insert(engine->global_mesh_buffer_.index_data.end(), indices.begin(), indices.end());
	}

	std::optional<std::shared_ptr<LoadedGLTF>> LoadGltf(GPUDevice* gpu_device, std::string_view file_path)
	{
		LOGI("Loading GLTF: {}", file_path);

		std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
		scene->gpu_device = gpu_device;
		LoadedGLTF &file = *scene.get();

		fastgltf::Parser parser{};

		constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;

		auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
		fastgltf::Asset gltf;
		std::filesystem::path path = file_path;

		auto type = fastgltf::determineGltfFileType(data.get());
		if (type == fastgltf::GltfType::glTF)
		{
			auto load = parser.loadGltf(data.get(), path.parent_path(), gltf_options);
			if (load)
			{
				gltf = std::move(load.get());
			}
			else
			{
				LOGE("Failed to load gltf: {}", fastgltf::to_underlying(load.error()));
				return {};
			}
		}
		else if (type == fastgltf::GltfType::GLB)
		{
			auto load = parser.loadGltfBinary(data.get(), path.parent_path(), gltf_options);
			if (load)
			{
				gltf = std::move(load.get());
			}
			else
			{
				LOGE("Failed to load gltf: {}", fastgltf::to_underlying(load.error()));
				return {};
			}
		}
		else
		{
			LOGE("Failed to determine glTF container");
			return {};
		}

		// we can stimate the descriptors we will need accurately
		std::vector<lc::DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
																			 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
																			 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

		file.descriptor_pool.Init(gpu_device->device_, static_cast<uint32_t>(gltf.materials.size()), sizes);

		// load sampler
		for (fastgltf::Sampler &sampler : gltf.samplers)
		{
			VkSamplerCreateInfo sampler_create_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
			sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
			sampler_create_info.minLod = 0;

			sampler_create_info.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
			sampler_create_info.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			sampler_create_info.mipmapMode = ExtractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			VkSampler new_sampler;
			vkCreateSampler(gpu_device->device_, &sampler_create_info, nullptr, &new_sampler);

			file.samplers.push_back(new_sampler);
		}

		// temporal arrays for all the objects to use while creating the GLTF data
		std::vector<std::shared_ptr<MeshAsset>> meshes;
		std::vector<std::shared_ptr<Node>> nodes;
		std::vector<AllocatedImage> images;
		std::vector<TextureID> imageIDs;
		std::vector<std::shared_ptr<GLTFMaterial>> materials;

		// load all textures
		for (fastgltf::Image &image : gltf.images)
		{
			std::optional<AllocatedImage> img = LoadImage( gltf, image, path.parent_path());

			if (img.has_value())
			{
				images.push_back(*img);
				if (image.name.empty())
				{
					image.name = "image" + std::to_string(images.size());
				}
				file.images[image.name.c_str()] = *img;
			}
			else
			{
				// we failed to load, so lets give the slot a default white texture to not completely break loading
				images.push_back(gpu_device->resource_manager_.GetTexture(gpu_device->default_resources_.images.error_checker_board_image));
				LOGW("gltf failed to load texture {}, fallint to default error texture", image.name);
			}
		}
		BufferCreationInfo buffer_info{};
		buffer_info.Set(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size());
		file.material_data_buffer_handle = gpu_device->resource_manager_.CreateBuffer(buffer_info);

		AllocatedBufferUntyped material_data_buffer = gpu_device->resource_manager_.GetBuffer(file.material_data_buffer_handle);
		int data_index = 0;
		GLTFMetallic_Roughness::MaterialConstants *sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants *)material_data_buffer.info.pMappedData;

		for (fastgltf::Material &mat : gltf.materials)
		{
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
			if (mat.alphaMode == fastgltf::AlphaMode::Blend)
			{
				pass_type = MeshPassType::kTransparent;
			}

			GLTFMetallic_Roughness::MaterialResources material_resources;
			// TODO: Replace with real images
			material_resources.color_image = gpu_device->resource_manager_.GetTexture(gpu_device->default_resources_.images.white_image);
			material_resources.color_sampler = gpu_device->default_resources_.samplers.linear;
			material_resources.metal_rough_image = gpu_device->resource_manager_.GetTexture(gpu_device->default_resources_.images.white_image);
			material_resources.metal_rought_sampler = gpu_device->default_resources_.samplers.linear;

			material_resources.data_buffer = material_data_buffer.buffer;
			material_resources.data_buffer_offset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);

			// grab textures from gltf file
			if (mat.pbrData.baseColorTexture.has_value())
			{
				size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

				material_resources.color_image = images[img];
				material_resources.color_sampler = file.samplers[sampler];
			}

			constants.color_tex_id =gpu_device->texture_cache_.AddTexture(gpu_device->device_, material_resources.color_image.view, material_resources.color_sampler).index;
			constants.metal_rought_tex_id = gpu_device->texture_cache_.AddTexture(gpu_device->device_, material_resources.metal_rough_image.view, material_resources.metal_rought_sampler).index;

			// write material parameter to buffer
			sceneMaterialConstants[data_index] = constants;
			// Build material
			new_mat->data = VulkanEngine::Get().metal_rough_material_.WriteMaterial(gpu_device->device_, pass_type, material_resources, file.descriptor_pool);

			data_index++;
		}

		// use the same vectors for all meshes so that the memory doesnt reallocate as often
		std::vector<uint32_t> indices;
		std::vector<Vertex> vertices;
		int mesh_index = 0;

		for (fastgltf::Mesh &mesh : gltf.meshes)
		{
			std::shared_ptr<MeshAsset> new_mesh = std::make_shared<MeshAsset>();
			meshes.push_back(new_mesh);
			file.meshes[mesh.name.c_str()] = new_mesh;
			new_mesh->name = mesh.name;

			// clear the mesh arrays each mesh, we dont want to merge them by error
			indices.clear();
			vertices.clear();
			size_t global_index_offset = VulkanEngine::Get().global_mesh_buffer_.index_data.size();
			size_t global_vertex_offset = VulkanEngine::Get().global_mesh_buffer_.vertex_data.size();

			for (auto &&p : mesh.primitives)
			{
				GeoSurface new_surface;
				new_surface.start_index = (uint32_t)indices.size();
				new_surface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

				size_t initial_vtx = vertices.size();

				// load indexes
				{
					fastgltf::Accessor &indexaccessor = gltf.accessors[p.indicesAccessor.value()];
					indices.reserve(indices.size() + indexaccessor.count);

					fastgltf::iterateAccessor<std::uint32_t>(gltf, 
						indexaccessor,
						[&](std::uint32_t idx)
						{
							indices.push_back(static_cast<uint32_t>(idx + initial_vtx));
						});
				}

				// load vertex positions
				{
					fastgltf::Accessor &posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
					vertices.resize(vertices.size() + posAccessor.count);

					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, 
						posAccessor,
						[&](glm::vec3 v, size_t index)
						{
							Vertex newvtx;
							newvtx.position = v;
							newvtx.normal = {1, 0, 0};
							newvtx.color = glm::vec4{1.f};
							newvtx.uv_x = 0;
							newvtx.uv_y = 0;
							vertices[initial_vtx + index] = newvtx;
						});
				}

				// load vertex normals
				auto normals = p.findAttribute("NORMAL");
				if (normals != p.attributes.end())
				{

					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, 
						gltf.accessors[(*normals).accessorIndex],
						[&](glm::vec3 v, size_t index)
						{
							vertices[initial_vtx + index].normal = v;
						});
				}

				// load UVs
				auto uv = p.findAttribute("TEXCOORD_0");
				if (uv != p.attributes.end())
				{

					fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, 
						gltf.accessors[(*uv).accessorIndex],
						[&](glm::vec2 v, size_t index)
						{
							vertices[initial_vtx + index].uv_x = v.x;
							vertices[initial_vtx + index].uv_y = v.y;
						});
				}

				// load vertex colors
				auto colors = p.findAttribute("COLOR_0");
				if (colors != p.attributes.end())
				{

					fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, 
						gltf.accessors[(*colors).accessorIndex],
						[&](glm::vec4 v, size_t index)
						{
							vertices[initial_vtx + index].color = v;
						});
				}

				if (p.materialIndex.has_value())
				{
					new_surface.material = materials[p.materialIndex.value()];
				}
				else
				{
					new_surface.material = materials[0];
				}

				// loop the vertice of this surface. find min/max bounds
				glm::vec3 min_pos = vertices[initial_vtx].position;
				glm::vec3 max_pos = vertices[initial_vtx].position;
				for (size_t i = initial_vtx; i < vertices.size(); i++)
				{
					min_pos = glm::min(min_pos, vertices[i].position);
					max_pos = glm::max(max_pos, vertices[i].position);
				}

				// calculate origin and extents from the min/max, use extent length for radius
				new_surface.bounds.origin = (min_pos + max_pos) * 0.5f;
				new_surface.bounds.extents = (max_pos - min_pos) * 0.5f;
				auto halfExtents = new_surface.bounds.extents;
				new_surface.bounds.sphere_radius = glm::max(glm::max(halfExtents.x, halfExtents.y), halfExtents.z);

				VkDrawIndexedIndirectCommand cmd = {};
				cmd.firstIndex = static_cast<uint32_t>(new_surface.start_index + global_index_offset);
				cmd.indexCount = new_surface.count;
				cmd.vertexOffset = static_cast<int32_t>(global_vertex_offset);
				cmd.instanceCount = 1;
				cmd.firstInstance = 0;

				new_surface.indirect_offset = static_cast<uint32_t>(VulkanEngine::Get().global_mesh_buffer_.indirect_commands.size());
				VulkanEngine::Get().global_mesh_buffer_.indirect_commands.push_back(cmd);

				new_mesh->surfaces.push_back(new_surface);
			}
			new_mesh->mesh_buffers = VulkanEngine::Get().UploadMesh(indices, vertices);
			AddMeshBufferToGlobalBuffers(indices, vertices);
		}

		// load all nodes and their meshes
		for (fastgltf::Node &node : gltf.nodes)
		{
			std::shared_ptr<Node> new_node;

			if (node.meshIndex.has_value())
			{
				new_node = std::make_shared<MeshNode>();
				static_cast<MeshNode *>(new_node.get())->mesh = meshes[*node.meshIndex];
			}
			else
			{
				new_node = std::make_shared<MeshNode>();
			}

			nodes.push_back(new_node);

			file.nodes[node.name.c_str()];

			std::visit(fastgltf::visitor{[&](fastgltf::math::fmat4x4 matrix)
										 {
											 memcpy(&new_node->local_transform, matrix.data(), sizeof(matrix));
										 },
										 [&](fastgltf::TRS transform)
										 {
											 glm::vec3 tl(transform.translation[0], transform.translation[1],
														  transform.translation[2]);
											 glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
														   transform.rotation[2]);
											 glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

											 glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
											 glm::mat4 rm = glm::toMat4(rot);
											 glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

											 new_node->local_transform = tm * rm * sm;
										 }},
					   node.transform);
		}

		// run loop again to setup transform hierarchy
		for (int i = 0; i < gltf.nodes.size(); ++i)
		{
			fastgltf::Node &node = gltf.nodes[i];
			std::shared_ptr<Node> &scene_node = nodes[i];

			for (auto &c : node.children)
			{
				scene_node->children.push_back(nodes[c]);
				nodes[c]->parent = scene_node;
			}
		}

		// find the top nodes, with no parents
		for (auto &node : nodes)
		{
			if (node->parent.lock() == nullptr)
			{
				file.top_nodes.push_back(node);
				node->RefreshTransform(glm::mat4{1.f});
			}
		}
		return scene;
	}

	void LoadedGLTF::Draw(const glm::mat4 &top_matrix, DrawContext &ctx)
	{
		for (auto &node : top_nodes)
		{
			node->Draw(top_matrix, ctx);
		}
	}

	void LoadedGLTF::ClearAll()
	{
		VkDevice dv = gpu_device->device_;

		descriptor_pool.DestroyPools(dv);

		/*for (auto &[k, v] : meshes)
		{

			creator->DestroyBuffer(v->mesh_buffers.index_buffer);
			creator->DestroyBuffer(v->mesh_buffers.vertex_buffer);
		}*/

		//for (auto &[k, v] : images)
		//{

		//	if (v.image == creator->default_images_.error_checker_board_image.image)
		//	{
		//		// dont destroy the default images
		//		continue;
		//	}
		//	creator->DestroyImage(v);
		//}

		for (auto &sampler : samplers)
		{
			vkDestroySampler(dv, sampler, nullptr);
		}
	}

	void AsyncLoader::Init(enki::TaskScheduler *task_scheduler)
	{
		task_scheduler_ = task_scheduler;
		state_->state = AsyncLoaderState::State::Running;
	}

	void AsyncLoader::Shutdown()
	{
		{
			std::unique_lock<std::mutex> lock(state_->mutex);
			if (state_->state != AsyncLoaderState::State::Running) return;
			state_->state = AsyncLoaderState::State::Stopping;
		}
		state_->cv.notify_all();

		while (true) {
			std::unique_lock<std::mutex> lock(request_mutex_);
			if (upload_requests_.empty() && file_load_requests_.empty()) {
				state_->state = AsyncLoaderState::State::Stopped;
				break;
			}
			state_->cv.wait_for(lock, std::chrono::milliseconds(100));
		}
	}

	void AsyncLoader::Update()
	{
		if (state_->state != AsyncLoaderState::State::Running) return;

		ProcessFileRequests();
		ProcessUploadRequests();

		if (file_load_requests_.empty() && upload_requests_.empty()) {
			std::unique_lock<std::mutex> lock(state_->mutex);
			state_->cv.wait_for(lock, std::chrono::milliseconds(16),
				[this] { return state_->state != AsyncLoaderState::State::Running; });
		}
	}

	void AsyncLoader::RequestFileLoad(const char *path, std::function<void(AllocatedImage)> callback)
	{
		if (state_->state != AsyncLoaderState::State::Running) return;

		std::lock_guard<std::mutex> lock(request_mutex_);
		FileLoadRequest request;
		request.type = FileLoadRequestType::kURI;
		strcpy(request.path, path);
		request.callback = std::move(callback);
		file_load_requests_.push_back(request);
		state_->cv.notify_one();
	}

	void AsyncLoader::RequestImageUpload(void *data, VkExtent3D extent, VkFormat format, std::function<void(AllocatedImage)> callback)
	{
		UploadRequest request{};
		request.data = data;
		request.size = extent.width * extent.height * 4; // Assuming 4 bytes per pixel
		request.format = format;
		request.extent = extent;
		request.callback = std::move(callback);

		upload_requests_.push_back(request);
	}

	void AsyncLoader::RequestVectorLoad(const void *data, size_t size, std::function<void(AllocatedImage)> callback)
	{
		if (state_->state != AsyncLoaderState::State::Running) return;

		std::lock_guard<std::mutex> lock(request_mutex_);
		FileLoadRequest request;
		request.type = FileLoadRequestType::kVector;
		request.memory_data = data;
		request.memory_size = size;
		request.callback = std::move(callback);
		file_load_requests_.push_back(request);
		state_->cv.notify_one();
	}

	void AsyncLoader::RequestBufferViewLoad(const void *data, size_t size, size_t offset, std::function<void(AllocatedImage)> callback)
	{
		if (state_->state != AsyncLoaderState::State::Running) return;

		std::lock_guard<std::mutex> lock(request_mutex_);
		FileLoadRequest request;
		request.type = FileLoadRequestType::kBufferView;
		request.memory_data = data;
		request.memory_size = size;
		request.buffer_offset = offset;
		request.callback = std::move(callback);
		file_load_requests_.push_back(request);
		state_->cv.notify_one();
	}

	void AsyncLoader::ProcessFileRequests()
	{
		std::vector<FileLoadRequest> requests;
		{
			std::lock_guard<std::mutex> lock(request_mutex_);
			requests = std::move(file_load_requests_);
		}

		for (auto& request : requests) {
			if (state_->state != AsyncLoaderState::State::Running) break;

			int width, height, channels;
			unsigned char* data = nullptr;

			if (request.type == FileLoadRequestType::kURI) {
				data = stbi_load(request.path, &width, &height, &channels, 4);
			}
			else if (request.type == FileLoadRequestType::kVector) {
				data = stbi_load_from_memory(
					static_cast<const stbi_uc*>(request.memory_data),
					static_cast<int>(request.memory_size),
					&width, &height, &channels, 4);
			}
			else if (request.type == FileLoadRequestType::kBufferView) {
				const auto* buffer_data = static_cast<const stbi_uc*>(request.memory_data) + request.buffer_offset;
				data = stbi_load_from_memory(
					buffer_data,
					static_cast<int>(request.memory_size),
					&width, &height, &channels, 4);
			}

			if (data && state_->state == AsyncLoaderState::State::Running) {
				std::lock_guard<std::mutex> lock(request_mutex_);

				UploadRequest upload_request{};
				upload_request.data = data;
				upload_request.extent = {
					static_cast<uint32_t>(width),
					static_cast<uint32_t>(height),
					1 };
				upload_request.size = width * height * 4;
				upload_request.format = VK_FORMAT_R8G8B8A8_UNORM;
				upload_request.enable_mips = false;

				upload_request.callback = [data, callback = std::move(request.callback)](AllocatedImage image) {
					callback(image);
					stbi_image_free(data);
					};

				upload_requests_.push_back(std::move(upload_request));
			}
			else {
				LOGE("Failed to load image. Type: {}", static_cast<int>(request.type));
			}
		}
	}

	void AsyncLoader::ProcessUploadRequests()
	{
		if (upload_requests_.empty() || state_->state != AsyncLoaderState::State::Running) return;

		std::vector<UploadRequest> requests;
		{
			std::lock_guard<std::mutex> lock(request_mutex_);
			requests = std::move(upload_requests_);
		}

		for (auto& request : requests) {
			if (state_->state != AsyncLoaderState::State::Running) break;

			// Process upload as before...
			size_t data_size = request.extent.depth * request.extent.width * request.extent.height * 4;
			BufferCreationInfo buffer_info{};
			buffer_info.Set(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, data_size);
			BufferHandle upload_buffer_handle = VulkanEngine::Get().gpu_device_.resource_manager_.CreateBuffer(buffer_info);
			AllocatedBufferUntyped& upload_buffer = VulkanEngine::Get().gpu_device_.resource_manager_.GetBuffer(upload_buffer_handle);// (data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			memcpy(upload_buffer.info.pMappedData, request.data, data_size);

			TextureCreationInfo texture_info{};
			texture_info.SetSize(request.extent)
				.SetFormatType(request.format, TextureType::Enum::Texture2D)
				.SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			TextureHandle texture_handle = VulkanEngine::Get().gpu_device_.resource_manager_.CreateTexture(texture_info);
			AllocatedImage& new_image = VulkanEngine::Get().gpu_device_.resource_manager_.GetTexture(texture_handle);
			
			VulkanEngine::Get().gpu_device_.command_buffer_manager_.ImmediateSubmit(
				[&](CommandBuffer* cmd) {
					// Command buffer operations...
					cmd->TransitionImage(new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

					VkBufferImageCopy copy_region{};
					copy_region.bufferOffset = 0;
					copy_region.bufferRowLength = 0;
					copy_region.bufferImageHeight = 0;
					copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					copy_region.imageSubresource.mipLevel = 0;
					copy_region.imageSubresource.baseArrayLayer = 0;
					copy_region.imageSubresource.layerCount = 1;
					copy_region.imageExtent = request.extent;

					vkCmdCopyBufferToImage(cmd->command_buffer_, upload_buffer.buffer, new_image.image,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

					if (request.enable_mips) {
						cmd->GenerateMipmaps(new_image.image, VkExtent2D{ new_image.extent.width, new_image.extent.height });
					}
					else {
						cmd->TransitionImage(new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
							VulkanEngine::Get().gpu_device_.queue_indices_.transfer_family,
							VulkanEngine::Get().gpu_device_.queue_indices_.graphics_family);
					}
				},
				VulkanEngine::Get().gpu_device_.transfer_queue_);

			request.callback(new_image);
		}
	}

}
