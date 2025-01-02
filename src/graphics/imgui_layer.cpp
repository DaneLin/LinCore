#include "graphics/imgui_layer.h"

// external
#include <SDL.h>
#include <SDL_vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
// lincore
#include "foundation/events/event.h"
#include "foundation/events/key_event.h"
#include "foundation/events/mouse_event.h"
#include "graphics/vk_device.h"

namespace lincore
{
	PFN_vkVoidFunction imgui_load_func(const char *funtion_name, void *user_data)
	{
		return vkGetInstanceProcAddr(static_cast<VkInstance>(user_data), funtion_name);
	}

	ImGuiLayer::ImGuiLayer()
		: Layer("ImGuiLayer")
	{
	}

	void ImGuiLayer::OnAttach()
	{
	}

	void ImGuiLayer::OnDetach()
	{
		Shutdown();
	}

	void ImGuiLayer::OnEvent(Event &event)
	{
		if (block_events_)
		{
			ImGuiIO &io = ImGui::GetIO();
			event.handled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			event.handled |= event.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<KeyEvent>(BIND_EVENT_FN(ImGuiLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseButtonPressed));
		dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseMoved));
		dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseScrolled));
	}

	void ImGuiLayer::Init(GpuDevice *gpu_device)
	{
		gpu_device_ = gpu_device;
		window_ = gpu_device->window_;
		InitImGui();
	}

	void ImGuiLayer::Shutdown()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		vkDestroyDescriptorPool(gpu_device_->device_, imgui_pool_, nullptr);
	}

	void ImGuiLayer::NewFrame()
	{
		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::EndFrame()
	{
		ImGui::Render();
	}

	void ImGuiLayer::Draw(CommandBuffer *cmd, uint32_t swapchain_image_index)
	{
		UtilAddImageBarrier(gpu_device_, cmd->vk_command_buffer_,gpu_device_->GetDrawImage(), ResourceState::RESOURCE_STATE_COPY_SOURCE,0,1,false);
		//cmd->TransitionImage(gpu_device_->GetDrawImage()->vk_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		cmd->TransitionImage(gpu_device_->swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		

		// execute a copy from the draw image into the swapchain
		cmd->CopyImageToImage(gpu_device_->GetDrawImage()->vk_image, gpu_device_->swapchain_images_[swapchain_image_index], gpu_device_->draw_extent_, gpu_device_->swapchain_extent_);
		cmd->TransitionImage(gpu_device_->swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		VkRenderingAttachmentInfo color_attachment = vkinit::AttachmentInfo(gpu_device_->swapchain_image_views_[swapchain_image_index], nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingInfo render_info = vkinit::RenderingInfo(gpu_device_->swapchain_extent_, &color_attachment, nullptr);

		cmd->BeginRendering(render_info);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->vk_command_buffer_);

		cmd->EndRendering();

		cmd->TransitionImage(gpu_device_->swapchain_images_[swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	void ImGuiLayer::InitImGui()
	{
		// 1: create descriptor pool for IMGUI
		//  the size of the pool is very oversize, but it's copied from imgui demo itself.
		VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
											 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
											 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
											 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
											 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
											 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
											 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000;
		pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;

		VK_CHECK(vkCreateDescriptorPool(gpu_device_->device_, &pool_info, nullptr, &imgui_pool_));
		gpu_device_->SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64_t>(imgui_pool_), "imgui_pool");

		// 2: initialize imgui library

		// this initializes the core structures of imgui
		ImGui::CreateContext();

		// this initializes imgui for SDL
		ImGui_ImplSDL2_InitForVulkan(window_);

		// this initializes imgui for Vulkan
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = gpu_device_->instance_;
		init_info.PhysicalDevice = gpu_device_->physical_device_;
		init_info.Device = gpu_device_->device_;
		init_info.Queue = gpu_device_->graphics_queue_;
		init_info.DescriptorPool = imgui_pool_;
		init_info.MinImageCount = 3;
		init_info.ImageCount = 3;
		init_info.UseDynamicRendering = true;

		// dynamic rendering parameters for imgui to use
		init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
		init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &gpu_device_->swapchain_image_format_;

		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		if (!ImGui_ImplVulkan_LoadFunctions(imgui_load_func, gpu_device_->instance_))
		{
			throw std::runtime_error("Failed to load imgui functions");
		}

		ImGui_ImplVulkan_Init(&init_info);

		ImGui_ImplVulkan_CreateFontsTexture();
	}

	bool ImGuiLayer::OnKeyPressed(KeyEvent &e)
	{
		return false;
	}

	bool ImGuiLayer::OnMouseButtonPressed(MouseButtonEvent &e)
	{
		return false;
	}

	bool ImGuiLayer::OnMouseMoved(MouseMovedEvent &e)
	{
		return false;
	}

	bool ImGuiLayer::OnMouseScrolled(MouseScrolledEvent &e)
	{
		return false;
	}

} // namespace LinCore
