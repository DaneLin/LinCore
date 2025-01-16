#pragma once
// external
#include "VkBootstrap.h"
#include "TaskScheduler.h"
// lincore
#include "foundation/config.h"
#include "graphics/vk_descriptors.h"
#include "graphics/vk_device.h"
#include "graphics/camera.h"
#include "graphics/vk_shaders.h"
#include "graphics/vk_types.h"
#include "graphics/vk_initializers.h"
#include "graphics/imgui_layer.h"


#include "graphics/scene_graph/scene_graph.h"
#include "graphics/render_pass/passes/sky_pass.h"
#include "graphics/render_pass/passes/culling_pass.h"
#include "graphics/render_pass/passes/mesh_pass.h"
#include "graphics/render_pass/passes/gbuffer_pass.h"
#include "graphics/render_pass/passes/light_pass.h"


namespace lincore
{
	// forward declarations
	class GpuDevice;
	class CommandBuffer;
	class ImGuiLayer;
	class BackgroundRenderer;

	/**
	 * @brief 引擎统计信息
	 * 包含帧时间、三角形计数、绘制调用计数和场景更新时间
	 */
	struct EngineStats {
		float frame_time;
		int triangle_count;
		int drawcall_count;
		float scene_update_time;
		float mesh_draw_time;
	};

	class VulkanEngine {
	public:

		bool is_initialized_{ false };

		bool freeze_rendering_{ false };
		bool resize_requested_{ false };

		VkExtent2D window_extent_{ 1920 , 1080 };

		struct SDL_Window* window_{ nullptr };

		DeletionQueue main_deletion_queue_;

		int current_background_effect_{ 0 };
		int current_scene_{ 0 };


		Camera main_camera_;

		std::shared_ptr<scene::SceneGraph> scene_graph_;

		EngineStats engine_stats_;

		SkyBackgroundPass sky_background_pass_;
		CullingPass culling_pass_;
		MeshPass mesh_pass_;
		GBufferPass gbuffer_pass_;
		LightPass light_pass_;

		// gbuffer
		TextureHandle gbuffer_position_handle_;
		TextureHandle gbuffer_normal_handle_;
		TextureHandle gbuffer_albedo_spec_handle_;
		TextureHandle gbuffer_arm_handle_;
		TextureHandle gbuffer_emission_handle_;

		ImGuiLayer imgui_layer_;
		GpuDevice gpu_device_;

	public:
		static VulkanEngine& Get();
		//initializes everything in the engine
		void Init();
		//shuts down the engine
		void CleanUp();
		//draw loop
		void Draw();
		//run main loop
		void Run();

		void UpdateScene();

		void DrawImGui();
	};

}