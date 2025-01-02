#pragma once
#include "event.h"
namespace lincore
{
    class WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : width_(width), height_(height) {}
        unsigned int GetWidth() const { return width_; }
        unsigned int GetHeight() const { return height_; }
        std::string ToString() const override
        {
            return "WindowResizeEvent: " + std::to_string(width_) + ", " + std::to_string(height_);
        }
        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    private:
        unsigned int width_, height_;
    };
    
    class WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() = default;
        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowFocusEvent : public Event
    {
    public:
        WindowFocusEvent() = default;
        EVENT_CLASS_TYPE(WindowFocus)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowLostFocusEvent : public Event
    {
    public:
        WindowLostFocusEvent() = default;
        EVENT_CLASS_TYPE(WindowLostFocus)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowMovedEvent : public Event
    {
    public:
        WindowMovedEvent(int x, int y)
            : x_(x), y_(y) {}
        int GetX() const { return x_; }
        int GetY() const { return y_; }
        EVENT_CLASS_TYPE(WindowMoved)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    private:
        int x_, y_;
    };
} // namespace lincore