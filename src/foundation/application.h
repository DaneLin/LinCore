#pragma once

// std
#include <string>
#include <vector>
#include <memory>
// lincore
#include "foundation/layer.h"
#include "foundation/window.h"
#include "graphics/imgui_layer.h"

namespace lincore
{
    class Event;
    class WindowCloseEvent;
    class WindowResizeEvent;

    class Application
    {
    public:
        Application(const std::string &name = "LinCore Engine");
        virtual ~Application();
        // Delete copy constructors
        Application(const Application &) = delete;
        Application &operator=(const Application &) = delete;
        void Run();
        void Close();
        void OnEvent(Event &e);
        // Layer management
        void PushLayer(Layer *layer);
        void PushOverlay(Layer *overlay);
        // Getters
        static Application &Get() { return *s_Instance; }
        Window &GetWindow() { return *window_; }
        ImGuiLayer *GetImGuiLayer() { return imgui_layer_; }
        float GetLastFrameTime() const { return last_frame_time_; }

    protected:
        virtual void Init();
        virtual void Shutdown();

    private:
        bool OnWindowClose(WindowCloseEvent &e);
        bool OnWindowResize(WindowResizeEvent &e);
        void ProcessEvents();
        float CalculateDeltaTime();

    private:
        std::unique_ptr<Window> window_;
        ImGuiLayer *imgui_layer_{nullptr};
        LayerStack layer_stack_;

        bool is_running_{true};
        bool is_minimized_{false};
        float last_frame_time_{0.0f};
        static Application *s_Instance;
    };
    // To be defined in CLIENT
    Application *CreateApplication();
}