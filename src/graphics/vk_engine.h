#pragma once
// external
#include "VkBootstrap.h"
#include "TaskScheduler.h"
// lincore
#include "foundation/config.h"
#include "graphics/backend/vk_descriptors.h"
#include "graphics/backend/vk_device.h"
#include "graphics/backend/camera.h"
#include "graphics/backend/vk_shaders.h"
#include "graphics/backend/vk_types.h"
#include "graphics/backend/vk_initializers.h"
#include "graphics/backend/imgui_layer.h"


#include "graphics/scene_graph/scene_graph.h"
#include "graphics/render_pass/passes/sky_pass.h"
#include "graphics/render_pass/passes/culling_pass.h"
#include "graphics/render_pass/passes/mesh_pass.h"
#include "graphics/render_pass/passes/gbuffer_pass.h"
#include "graphics/render_pass/passes/light_pass.h"
#include "graphics/render_pass/passes/ssao_pass.h"
#include "graphics/render_pass/passes/blur_pass.h"
#include "graphics/render_pass/passes/sky_box_pass.h"
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

		int current_scene_{ 0 };

		Camera main_camera_;

		std::shared_ptr<scene::SceneGraph> scene_graph_;

		EngineStats engine_stats_;

		SkyBackgroundPass sky_background_pass_;
		CullingPass culling_pass_;
		MeshPass mesh_pass_;
		GBufferPass gbuffer_pass_;
		LightPass light_pass_;
		SSAOPass ssao_pass_;
		BlurPass blur_pass_;
		SkyBoxPass sky_box_pass_;

		// gbuffer
		TextureHandle gbuffer_normal_rough_handle_;
		TextureHandle gbuffer_albedo_spec_handle_;
		TextureHandle gbuffer_emission_handle_;

		// ssao
		BufferHandle ssao_kernel_buffer_handle_;
		TextureHandle ssao_noise_handle_;
		TextureHandle ssao_color_handle_;
		TextureHandle ssao_blur_handle_;

		// cubemap
		TextureHandle cubemap_handle_;

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

	private:
		void OnResize();
		void InitResources();
		void InitPasses();
	};

}