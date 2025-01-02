#include "foundation/window.h"
#include "events/event.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdexcept>
namespace lincore
{
    static bool s_sdl_initialized = false;
    Window *Window::Create(const WindowProps &props)
    {
        return new Window(props);
    }
    Window::Window(const WindowProps &props)
    {
        Init(props);
    }
    Window::~Window()
    {
        Shutdown();
    }
    void Window::Init(const WindowProps &props)
    {
        data_.title = props.title;
        data_.width = props.width;
        data_.height = props.height;
        data_.vsync = props.vsync;
        if (!s_sdl_initialized)
        {
            int success = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);
            if (success != 0)
            {
                throw std::runtime_error("Could not initialize SDL!");
            }
            s_sdl_initialized = true;
        }
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN |
                                                         SDL_WINDOW_RESIZABLE |
                                                         SDL_WINDOW_ALLOW_HIGHDPI);
        window_ = SDL_CreateWindow(
            data_.title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            data_.width,
            data_.height,
            window_flags);
        if (!window_)
        {
            throw std::runtime_error("Failed to create SDL window!");
        }
        SetVSync(data_.vsync);
    }
    void Window::Shutdown()
    {
        if (window_)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        if (s_sdl_initialized)
        {
            SDL_Quit();
            s_sdl_initialized = false;
        }
    }
    void Window::OnUpdate()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (Event *e = Event::FromSDLEvent(event))
            {
                data_.event_callback(*e);
                delete e;
            }
        }
    }
    void Window::SetVSync(bool enabled)
    {
        // Note: SDL with Vulkan doesn't directly support VSync
        // This will need to be handled in the Vulkan swapchain
        data_.vsync = enabled;
    }
    void Window::OnResize(uint32_t width, uint32_t height)
    {
        data_.width = width;
        data_.height = height;
    }
} // namespace lincore
