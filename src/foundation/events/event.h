#pragma once

#include <string>
#include <functional>
#include <iostream>
// external
#include <SDL_events.h>

namespace lincore
{
	// 在event.h中添加这个宏定义
#define BIND_EVENT_FN(fn) [this](auto &&...args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
	// Event Types
	enum class EventType
	{
		None = 0,
		// Window events
		WindowClose,
		WindowResize,
		WindowFocus,
		WindowLostFocus,
		WindowMoved,
		// Application events
		AppTick,
		AppUpdate,
		AppRender,
		// Input events
		KeyPressed,
		KeyReleased,
		KeyTyped,
		MouseButtonPressed,
		MouseButtonReleased,
		MouseMoved,
		MouseScrolled
	};
	// Event Categories
	enum EventCategory
	{
		None = 0,
		EventCategoryApplication = 1 << 0,
		EventCategoryInput = 1 << 1,
		EventCategoryKeyboard = 1 << 2,
		EventCategoryMouse = 1 << 3,
		EventCategoryMouseButton = 1 << 4
	};
#define EVENT_CLASS_TYPE(type)                                                  \
    static EventType GetStaticType() { return EventType::##type; }              \
    virtual EventType GetEventType() const override { return GetStaticType(); } \
    virtual const char *GetName() const override { return #type; }
#define EVENT_CLASS_CATEGORY(category) \
    virtual int GetCategoryFlags() const override { return category; }
	// Base Event class
	class Event
	{
		friend class EventDispatcher;

	public:
		virtual ~Event() = default;
		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }
		inline bool IsInCategory(EventCategory category)
		{
			return GetCategoryFlags() & category;
		}
		bool handled = false;
		// SDL event conversion
		static Event* FromSDLEvent(const SDL_Event& sdl_event);
	};

	// Event Dispatcher
	class EventDispatcher
	{
		template <typename T>
		using EventFn = std::function<bool(T&)>;

	public:
		EventDispatcher(Event& event)
			: event_(event) {}
		template <typename T>
		bool Dispatch(EventFn<T> func)
		{
			if (event_.GetEventType() == T::GetStaticType())
			{
				event_.handled = func(*(T*)&event_);
				return true;
			}
			return false;
		}

	private:
		Event& event_;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}
}