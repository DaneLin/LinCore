#include "vk_scene.h"

RenderScene::PassObject* RenderScene::MeshPass::Get(Handle<PassObject> handle)
{
	return nullptr;
}

void RenderScene::Init()
{
	forwars_pass.type = MeshPassType::kMainColor;
	shadow_pass.type = MeshPassType::kDirectionalShadow;
	transparent_forward_pass.type = MeshPassType::kTransparent;
}

Handle<RenderObject> RenderScene::RegisterObject(MeshObject* object)
{
	return Handle<RenderObject>();
}

void RenderScene::RegisterObjectBatch(MeshObject* first, uint32_t count)
{
}

void RenderScene::UpdateTransfrom(Handle<RenderObject> object_id, const glm::mat4& local_to_world)
{
}

void RenderScene::UpdateObject(Handle<RenderObject> object_id)
{
}

void RenderScene::FillObjectData(GPUObjectData* data)
{
}

void RenderScene::FillIndirectArray(GPUIndirectObject* data, MeshPass& pass)
{
}

void RenderScene::FillInstanceArray(GPUInstance* data, MeshPass& pass)
{
}

void RenderScene::WriteObject(GPUObjectData* target, Handle<RenderObject> object_id)
{
}

void RenderScene::ClearDirtyObjects()
{
}

void RenderScene::BuildBatches()
{
}

void RenderScene::MergeMeshes(VulkanEngine* engine)
{
}

void RenderScene::RefreshPass(MeshPass* pass)
{
}

void RenderScene::BuildIndirectBatches(MeshPass* pass, std::vector<IndirectBatch>& out_batches, std::vector<RenderScene::RenderBatch>& in_objects)
{
}

RenderObject* RenderScene::GetObject(Handle<RenderObject> object_id)
{
	return nullptr;
}

DrawMesh* RenderScene::GetMesh(Handle<DrawMesh> object_id)
{
	return nullptr;
}

vkutils::Material* RenderScene::GetMaterial(Handle<vkutils::Material> object_id)
{
	return nullptr;
}

RenderScene::MeshPass* RenderScene::GetMeshPass(MeshPassType name)
{
	return nullptr;
}

Handle<vkutils::Material> RenderScene::GetMaterialHandle(vkutils::Material* m)
{
	return Handle<vkutils::Material>();
}

Handle<DrawMesh> RenderScene::GetMeshHandle(Mesh* m)
{
	return Handle<DrawMesh>();
}
