#include "event.h"
#include "window_event.h"
#include "key_event.h"
#include "mouse_event.h"

namespace lincore
{
    Event *Event::FromSDLEvent(const SDL_Event &sdl_event)
    {
        switch (sdl_event.type)
        {
        case SDL_QUIT:
            return new WindowCloseEvent();
        case SDL_WINDOWEVENT:
            switch (sdl_event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                return new WindowResizeEvent(sdl_event.window.data1, sdl_event.window.data2);
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                return new WindowFocusEvent();
            case SDL_WINDOWEVENT_FOCUS_LOST:
                return new WindowLostFocusEvent();
            case SDL_WINDOWEVENT_MOVED:
                return new WindowMovedEvent(sdl_event.window.data1, sdl_event.window.data2);
            }
            break;
        case SDL_KEYDOWN:
            return new KeyPressedEvent(sdl_event.key.keysym.sym, sdl_event.key.repeat != 0);
        case SDL_KEYUP:
            return new KeyReleasedEvent(sdl_event.key.keysym.sym);
        case SDL_TEXTINPUT:
            return new KeyTypedEvent(sdl_event.text.text[0]);
        case SDL_MOUSEMOTION:
            return new MouseMovedEvent(static_cast<float>(sdl_event.motion.x), static_cast<float>(sdl_event.motion.y));
        case SDL_MOUSEWHEEL:
            return new MouseScrolledEvent(static_cast<float>(sdl_event.wheel.x), static_cast<float>(sdl_event.wheel.y));
        case SDL_MOUSEBUTTONDOWN:
            return new MouseButtonPressedEvent(sdl_event.button.button);
        case SDL_MOUSEBUTTONUP:
            return new MouseButtonReleasedEvent(sdl_event.button.button);
        }

        return nullptr;
    }

} // namespace lincore
