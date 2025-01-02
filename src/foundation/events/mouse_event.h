#pragma once
#include "event.h"
namespace lincore
{
    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(float x, float y)
            : mouse_x_(x), mouse_y_(y) {}
        float GetX() const { return mouse_x_; }
        float GetY() const { return mouse_y_; }
        std::string ToString() const override
        {
            return "MouseMovedEvent: " + std::to_string(mouse_x_) + ", " + std::to_string(mouse_y_);
        }
        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    private:
        float mouse_x_, mouse_y_;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(float x_offset, float y_offset)
            : x_offset_(x_offset), y_offset_(y_offset) {}
        float GetXOffset() const { return x_offset_; }
        float GetYOffset() const { return y_offset_; }
        std::string ToString() const override
        {
            return "MouseScrolledEvent: " + std::to_string(x_offset_) + ", " + std::to_string(y_offset_);
        }
        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
    private:
        float x_offset_, y_offset_;
    };

    class MouseButtonEvent : public Event
    {
    public:
        int GetMouseButton() const { return button_; }
        virtual EventType GetEventType() const override = 0;
        virtual const char* GetName() const override = 0;
        static EventType GetStaticType() { return EventType::None; }  // Base class returns None
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    protected:
        MouseButtonEvent(int button)
            : button_(button) {}
        int button_;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(int button)
            : MouseButtonEvent(button) {}
        std::string ToString() const override
        {
            return "MouseButtonPressedEvent: " + std::to_string(button_);
        }
        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(int button)
            : MouseButtonEvent(button) {}
        std::string ToString() const override
        {
            return "MouseButtonReleasedEvent: " + std::to_string(button_);
        }
        EVENT_CLASS_TYPE(MouseButtonReleased)
    };
} // namespace lincore