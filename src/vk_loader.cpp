
#include <vk_loader.h>

#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/core.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath)
{
	std::cout << "Loading GLTF: " << filePath << std::endl;

	fastgltf::GltfDataBuffer data;

	auto ret = data.FromPath(filePath);

	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::Asset gltf;
	fastgltf::Parser parser{};

	auto load = parser.loadGltfBinary(ret.get(), filePath.parent_path(), gltfOptions);
	if (load) {
		gltf = std::move(load.get());
	}
	else {
		fmt::print("Failed to load gltf: {} \n", fastgltf::to_underlying(load.error()));
		return {};
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;

	// use the same vectors for all meshes so that the memory doesn't reallocate as often
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		MeshAsset newMesh;

		newMesh.name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) {
			GeoSurface newSurface;
			newSurface.startIndex = static_cast<uint32_t>(indices.size());
			newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

			size_t initialVtx = vertices.size();
			// load indexes
			{
				fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
					[&](std::uint32_t idx) {
						indices.push_back(idx + static_cast<uint32_t>(initialVtx));
					});
			}
			// load vextex positions
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
						vertices[initialVtx + index] = newvtx;
					});
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
					[&](glm::vec3 v, size_t index) {
						vertices[initialVtx + index].normal = v;
					});
			}

			// load UVs
			auto uvs = p.findAttribute("TEXCOORD_0");
			if (uvs != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uvs).accessorIndex],
					[&](glm::vec2 v, size_t index) {
						vertices[initialVtx + index].uv_x = v.x;
						vertices[initialVtx + index].uv_y = v.y;
					});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
					[&](glm::vec4 v, size_t index) {
						vertices[initialVtx + index].color = v;
					});
			}
			newMesh.surfaces.push_back(newSurface);
		}

		// display the vertex normals
		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}

		newMesh.meshBuffers = engine->upload_mesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}
	return meshes;
}
