#pragma once

#include <vk_types.h>
#include <vk_loader.h>

template <typename T>
struct Handle
{
    uint32_t handle;
};

struct MeshObject;
struct Mesh;
struct GPUObjectData;
namespace vkutils
{
    struct Material;
}
namespace vkutils
{
    struct ShaderPass;
}

struct GPUIndirectObject
{
    VkDrawIndexedIndirectCommand command;
    uint32_t object_id;
    uint32_t batch_id;
};

struct DrawMesh
{
    uint32_t first_vertex;
    uint32_t first_index;
    uint32_t index_count;
    uint32_t vertex_count;
    bool is_merged;

    Mesh *original;
};

struct RenderObject
{
    Handle<DrawMesh> mesh_id;
    Handle<vkutils::Material> material_id;

    uint32_t update_index;
    uint32_t custom_sort_key{0};

    lc::PerPassData<int32_t> pass_indices;

    glm::mat4 transform;

    lc::Bounds bounds;
};

struct GPUInstance
{
    uint32_t object_id;
    uint32_t batch_id;
};

class RenderScene
{
public:
    struct PassMaterial
    {
        VkDescriptorSet material_set;
        vkutils::ShaderPass* shader_pass;

        bool operator==(const PassMaterial& other) const
        {
            return material_set == other.material_set && shader_pass == other.shader_pass;
        }
    };

    struct PassObject
    {
        PassMaterial material;
        Handle<DrawMesh> mesh_id;
        Handle<RenderObject> original;
        int32_t built_batch;
        uint32_t custom_key;
    };

    struct RenderBatch
    {
        Handle<PassObject> object;
        uint64_t sort_key;

        bool operator==(const RenderBatch& other) const
        {
            return object.handle == other.object.handle && sort_key == other.sort_key;
        }
    };

    struct IndirectBatch
    {
        Handle<DrawMesh> mesh_id;
        PassMaterial material;
        uint32_t first;
        uint32_t count;
    };

    struct MultiBatch
    {
        uint32_t first;
        uint32_t count;
    };

    struct MeshPass
    {
        std::vector<RenderScene::MultiBatch> multi_batches;
        std::vector<RenderScene::IndirectBatch> batches;
        std::vector<Handle<RenderObject>> unbatched_objects;
        std::vector<RenderScene::RenderBatch> flat_batches;

        std::vector<PassObject> objects;

        std::vector<Handle<PassObject>> reusable_objects;
        std::vector<Handle<PassObject>> objects_to_delete;  

        AllocatedBuffer<uint32_t> compacted_instance_buffer;
        AllocatedBuffer<GPUInstance> pass_objects_buffer;

        AllocatedBuffer<GPUIndirectObject> draw_indirect_buffer;
        AllocatedBuffer<GPUIndirectObject> clear_indirect_buffer;

        PassObject* Get(Handle<PassObject> handle);

        MeshPassType type;

        bool needs_indirect_refresh = true;
        bool needs_instance_refrest = true;
    };

    void Init();

    Handle<RenderObject> RegisterObject(MeshObject* object);

    void RegisterObjectBatch(MeshObject* first, uint32_t count);

    void UpdateTransfrom(Handle<RenderObject> object_id, const glm::mat4& local_to_world);
    void UpdateObject(Handle<RenderObject> object_id);

    void FillObjectData(GPUObjectData* data);
    void FillIndirectArray(GPUIndirectObject* data, MeshPass& pass);
	void FillInstanceArray(GPUInstance* data, MeshPass& pass);

    void WriteObject(GPUObjectData* target, Handle<RenderObject> object_id);

    void ClearDirtyObjects();

	void BuildBatches();

    void MergeMeshes(class VulkanEngine* engine);

    void RefreshPass(MeshPass* pass);

    void BuildIndirectBatches(MeshPass* pass, std::vector<IndirectBatch>& out_batches, std::vector<RenderScene::RenderBatch>& in_objects);

    RenderObject* GetObject(Handle<RenderObject> object_id);
    DrawMesh* GetMesh(Handle<DrawMesh> object_id);

    vkutils::Material* GetMaterial(Handle<vkutils::Material> object_id);

    std::vector<RenderObject> renderables;
    std::vector<DrawMesh> meshes;
    std::vector<vkutils::Material*> materials;

    std::vector<Handle<RenderObject>> dirty_objects;

    MeshPass* GetMeshPass(MeshPassType name);

    MeshPass forwars_pass;
    MeshPass transparent_forward_pass;
    MeshPass shadow_pass;

	std::unordered_map<vkutils::Material*, Handle<vkutils::Material>> material_convert;
    std::unordered_map<Mesh*, Handle<DrawMesh>> mesh_convert;


    Handle<vkutils::Material> GetMaterialHandle(vkutils::Material* m);
    Handle<DrawMesh> GetMeshHandle(Mesh* m);

    AllocatedBuffer<Vertex> merged_vertex_buffer;
    AllocatedBuffer<uint32_t> merged_index_buffer;

    AllocatedBuffer<GPUObjectData> object_data_buffer;
};