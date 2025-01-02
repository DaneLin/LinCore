#pragma once
#include "event.h"
#include <SDL_keycode.h>
namespace lincore
{
    class KeyEvent : public Event
    {
    public:
        SDL_Keycode GetKeyCode() const { return key_code_; }
        virtual EventType GetEventType() const override = 0;
        virtual const char* GetName() const override = 0;
        static EventType GetStaticType() { return EventType::None; }
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
    protected:
        KeyEvent(SDL_Keycode keycode)
            : key_code_(keycode) {}
        SDL_Keycode key_code_;
    };


    class KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(SDL_Keycode keycode, bool repeat = false)
            : KeyEvent(keycode), repeat_(repeat) {}
        bool IsRepeat() const { return repeat_; }
        std::string ToString() const override
        {
            return "KeyPressedEvent: " + std::to_string(key_code_) + " (repeat = " + std::to_string(repeat_) + ")";
        }
        EVENT_CLASS_TYPE(KeyPressed)
    private:
        bool repeat_;
    };


    class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(SDL_Keycode keycode)
            : KeyEvent(keycode) {}
        EVENT_CLASS_TYPE(KeyReleased)
    };

    
    class KeyTypedEvent : public KeyEvent
    {
    public:
        KeyTypedEvent(SDL_Keycode keycode)
            : KeyEvent(keycode) {}
        EVENT_CLASS_TYPE(KeyTyped)
    };
} // namespace lincore