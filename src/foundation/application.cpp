#include "foundation/application.h"
// external
#include <SDL.h>
// lincore
#include "foundation/timestep.h"
#include "events/event.h"
#include "events/window_event.h"
#include "application.h"

namespace lincore
{
    Application *Application::s_Instance = nullptr;
    Application::Application(const std::string &name)
    {
        // Ensure single instance
        assert(!s_Instance && "Application already exists!");
        s_Instance = this;
        // Create window
        WindowProps props;
        props.title = name;
        window_ = std::make_unique<Window>(props);
        window_->SetEventCallback([this](Event &e)
                                  { OnEvent(e); });
        // Create ImGui layer
        imgui_layer_ = new ImGuiLayer();
        PushOverlay(imgui_layer_);
    }
    Application::~Application()
    {
        Shutdown();
        s_Instance = nullptr;
    }
    void Application::Init()
    {
        // Virtual function for derived classes
    }
    void Application::Shutdown()
    {
        // Virtual function for derived classes
    }
    void Application::Run()
    {
        while (is_running_)
        {
            float time = SDL_GetTicks() * 0.001f; // Convert to seconds
            Timestep timestep(time - last_frame_time_);
            last_frame_time_ = time;
            // Process events
            ProcessEvents();
            if (!is_minimized_)
            {
                // Update layers
                for (Layer *layer : layer_stack_)
                {
                    layer->OnUpdate(timestep);
                }


                // ImGui update
                imgui_layer_->NewFrame();
                for (Layer *layer : layer_stack_)
                {
                    layer->OnImGuiRender();
                }
                imgui_layer_->EndFrame();
            }
            window_->OnUpdate();
        }
    }
    void Application::ProcessEvents()
    {
        SDL_Event sdl_event;
        while (SDL_PollEvent(&sdl_event))
        {
            if (Event *event = Event::FromSDLEvent(sdl_event))
            {
                OnEvent(*event);
                delete event;
            }
        }
    }
    float Application::CalculateDeltaTime()
    {
        return 0.0f;
    }
    void Application::OnEvent(Event &e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));
        // Handle events in reverse order for layers
        for (auto it = layer_stack_.rbegin(); it != layer_stack_.rend(); ++it)
        {
            if (e.handled)
            {
                break;
            }
            (*it)->OnEvent(e);
        }
    }

    void Application::PushLayer(Layer *layer)
    {
        layer_stack_.PushLayer(layer);
        layer->OnAttach();
    }
    void Application::PushOverlay(Layer *overlay)
    {
        layer_stack_.PushOverlay(overlay);
        overlay->OnAttach();
    }
    bool Application::OnWindowClose(WindowCloseEvent &e)
    {
        is_running_ = false;
        return true;
    }
    bool Application::OnWindowResize(WindowResizeEvent &e)
    {
        if (e.GetWidth() == 0 || e.GetHeight() == 0)
        {
            is_minimized_ = true;
            return false;
        }
        is_minimized_ = false;
        window_->OnResize(e.GetWidth(), e.GetHeight());
        return false;
    }
    void Application::Close()
    {
        is_running_ = false;
    }
} // namespace lincore