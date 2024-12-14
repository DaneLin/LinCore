//#include "vk_types.h"
//
//#include <vk_engine.h>
//#include <vk_shaders.h>
//
//
//
//void GLTFMetallic_Roughness::BuildPipelines(VulkanEngine* engine)
//{
//	lc::ShaderEffect* meshEffect = engine->shader_cache_.GetShaderEffect();
//#if LC_DRAW_INDIRECT
//	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh_indirect.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
//#else
//	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.vert.spv"), VK_SHADER_STAGE_VERTEX_BIT);
//#endif // LC_DRAW_INDIRECT
//	meshEffect->AddStage(engine->shader_cache_.GetShader("shaders/mesh.frag.spv"), VK_SHADER_STAGE_FRAGMENT_BIT);
//
//	lc::DescriptorLayoutBuilder layoutBuilder;
//	layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//
//	material_layout = layoutBuilder.Build(engine->gpu_device_.GetDevice(), VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
//
//	VkDescriptorSetLayout layouts[] = {
//		engine->gpu_scene_data_descriptor_layout_,
//		material_layout,
//		engine->bindless_texture_layout_ };
//
//	VkPushConstantRange matrixRange{};
//	matrixRange.offset = 0;
//#if LC_DRAW_INDIRECT
//	matrixRange.size = sizeof(GPUDrawIndirectPushConstants);
//#else
//	matrixRange.size = sizeof(GPUDrawPushConstants);
//#endif // LC_DRAW_INDIRECT
//	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//
//	VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::PipelineLayoutCreateInfo();
//	meshLayoutInfo.setLayoutCount = 3;
//	meshLayoutInfo.pSetLayouts = layouts;
//	meshLayoutInfo.pPushConstantRanges = &matrixRange;
//	meshLayoutInfo.pushConstantRangeCount = 1;
//
//	VK_CHECK(vkCreatePipelineLayout(engine->gpu_device_.GetDevice(), &meshLayoutInfo, nullptr, &engine->mesh_pipeline_layout_));
//
//	// build the stage-create-info for both vertx and fragment stages
//	lc::PipelineBuilder pipelineBuilder;
//	pipelineBuilder.SetShaders(meshEffect);
//	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
//	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
//	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
//	pipelineBuilder.SetMultisamplingNone();
//	pipelineBuilder.DisableBlending();
//	pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
//
//	// render format
//	pipelineBuilder.SetColorAttachmentFormat(engine->gpu_device_.GetDrawImage().format);
//	pipelineBuilder.SetDepthFormat(engine->gpu_device_.GetDepthImage().format);
//	pipelineBuilder.pipeline_layout_ = engine->mesh_pipeline_layout_;
//
//	opaque_pipeline.layout = engine->mesh_pipeline_layout_;
//	opaque_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->gpu_device_.GetDevice(), engine->global_pipeline_cache_->GetCache());
//
//	// create the transparent variant
//	pipelineBuilder.EnableBlendingAdditive();
//
//	pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
//
//	transparent_pipeline.layout = engine->mesh_pipeline_layout_;
//	transparent_pipeline.pipeline = pipelineBuilder.BuildPipeline(engine->gpu_device_.GetDevice(), engine->global_pipeline_cache_->GetCache());
//}
//
//void GLTFMetallic_Roughness::ClearResources(VkDevice device)
//{
//	vkDestroyDescriptorSetLayout(device, material_layout, nullptr);
//	vkDestroyPipelineLayout(device, transparent_pipeline.layout, nullptr);
//
//	vkDestroyPipeline(device, transparent_pipeline.pipeline, nullptr);
//	vkDestroyPipeline(device, opaque_pipeline.pipeline, nullptr);
//}
//
//MaterialInstance GLTFMetallic_Roughness::WriteMaterial(VkDevice device, MeshPassType pass, const MaterialResources& resources, lc::DescriptorAllocatorGrowable& descriptor_allocator)
//{
//	MaterialInstance matData;
//	matData.pass_type = pass;
//	if (pass == MeshPassType::kTransparent)
//	{
//		matData.pipeline = &transparent_pipeline;
//	}
//	else
//	{
//		matData.pipeline = &opaque_pipeline;
//	}
//
//	matData.set = descriptor_allocator.Allocate(device, material_layout);
//
//	writer.Clear();
//	writer.WriteBuffer(0, resources.data_buffer, sizeof(MaterialConstants), resources.data_buffer_offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//	writer.UpdateSet(device, matData.set);
//
//	return matData;
//}