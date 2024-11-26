//#pragma once
//
//#include <vk_types.h>
//#include <vk_loader.h>
//
//template <typename T>
//struct Handle
//{
//    uint32_t handle;
//};
//
//struct MeshObject;
//struct Mesh;
//struct GPUObjectData;
//namespace vkutils
//{
//    struct Material;
//}
//namespace vkutils
//{
//    struct ShaderPass;
//}
//
//struct GPUIndirectObject
//{
//    VkDrawIndexedIndirectCommand command;
//    uint32_t object_id;
//    uint32_t batch_id;
//};
//
//struct DrawMesh
//{
//    uint32_t first_vertex;
//    uint32_t first_index;
//    uint32_t index_count;
//    uint32_t vertex_count;
//    bool is_merged;
//
//    Mesh *original;
//};
//
//struct RenderObject
//{
//    Handle<DrawMesh> mesh_id;
//    Handle<vkutils::Material> material_id;
//
//    uint32_t update_index;
//    uint32_t custom_sort_key{0};
//
//    lc::PerPassData<int32_t> pass_indices;
//
//    glm::mat4 transform;
//
//    lc::Bounds bounds;
//};
//
//struct GPUInstance
//{
//    uint32_t object_id;
//    uint32_t batch_id;
//};
//
//class RenderScene
//{
//public:
//    struct PassMaterial
//    {
//        VkDescriptorSet material_set;
//        vkutils::ShaderPass* shader_pass;
//
//        bool operator==(const PassMaterial& other) const
//        {
//            return material_set == other.material_set && shader_pass == other.shader_pass;
//        }
//    };
//
//    struct PassObject
//    {
//        PassMaterial material;
//        Handle<DrawMesh> mesh_id;
//        Handle<RenderObject> original;
//        int32_t built_batch;
//        uint32_t custom_key;
//    };
//
//    struct RenderBatch
//    {
//        Handle<PassObject> object;
//        uint64_t sort_key;
//
//        bool operator==(const RenderBatch& other) const
//        {
//            return object.handle == other.object.handle && sort_key == other.sort_key;
//        }
//    };
//
//    struct IndirectBatch
//    {
//        Handle<DrawMesh> mesh_id;
//        PassMaterial material;
//        uint32_t first;
//        uint32_t count;
//    };
//
//    struct MultiBatch
//    {
//        uint32_t first;
//        uint32_t count;
//    };
//
//    struct MeshPass
//    {
//        std::vector<RenderScene::MultiBatch> multi_batches;
//        std::vector<RenderScene::IndirectBatch> batches;
//        std::vector<Handle<RenderObject>> unbatched_objects;
//        std::vector<RenderScene::RenderBatch> flat_batches;
//
//        std::vector<PassObject> objects;
//
//        std::vector<Handle<PassObject>> reusable_objects;
//        std::vector<Handle<PassObject>> objects_to_delete;  
//
//        AllocatedBuffer compacted_instance_buffer;
//    };
//};