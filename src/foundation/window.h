#pragma once

#include <string>
#include <functional>
#include "events/event.h"
struct SDL_Window;
namespace lincore
{
    struct WindowProps
    {
        std::string title;
        uint32_t width;
        uint32_t height;
        bool vsync;
        WindowProps(const std::string &title = "LinCore Engine",
                    uint32_t width = 1280,
                    uint32_t height = 720,
                    bool vsync = true)
            : title(title), width(width), height(height), vsync(vsync)
        {
        }
    };
    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event &)>;
        Window(const WindowProps &props);
        virtual ~Window();
        void OnUpdate();

        // Getters
        uint32_t GetWidth() const { return data_.width; }
        uint32_t GetHeight() const { return data_.height; }
        SDL_Window *GetNativeWindow() const { return window_; }
        // Window attributes
        void SetEventCallback(const EventCallbackFn &callback) { data_.event_callback = callback; }
        void SetVSync(bool enabled);
        bool IsVSync() const { return data_.vsync; }
        void OnResize(uint32_t width, uint32_t height);
        static Window *Create(const WindowProps &props = WindowProps());

    private:
        void Init(const WindowProps &props);
        void Shutdown();

    private:
        SDL_Window *window_{nullptr};
        struct WindowData
        {
            std::string title;
            uint32_t width{0};
            uint32_t height{0};
            bool vsync{true};
            EventCallbackFn event_callback;
        };
        WindowData data_;
    };
} // namespace lincore