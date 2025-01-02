#pragma once

// lincore
#include "foundation/layer.h"
#include "graphics/vk_types.h"
#include "graphics/vk_resources.h"

struct SDL_Window;

namespace lincore
{
    // forward declarations
    class GpuDevice;
    class CommandBuffer;
    class Event;
    class KeyEvent;
    class MouseButtonEvent;
    class MouseMovedEvent;
    class MouseScrolledEvent;

    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        virtual ~ImGuiLayer() = default;
            // Layer interface
        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnEvent(Event& event) override;
        virtual void OnImGuiRender() override {}

         // ImGui specific
        void Init(GpuDevice* gpu_device);
        void Shutdown();
        // ui frame loop
        void NewFrame();
        void EndFrame();
        // render
        void Draw(CommandBuffer* cmd, uint32_t swapchain_image_index);

    private:
        void InitImGui();

        // Event handlers
        bool OnKeyPressed(KeyEvent& e);
        bool OnMouseButtonPressed(MouseButtonEvent& e);
        bool OnMouseMoved(MouseMovedEvent& e);
        bool OnMouseScrolled(MouseScrolledEvent& e);
    private:
        GpuDevice* gpu_device_{nullptr};
        SDL_Window* window_{nullptr};
        VkDescriptorPool imgui_pool_{VK_NULL_HANDLE};

        float time_{0.0f};
        bool block_events_{true};
    };
}
