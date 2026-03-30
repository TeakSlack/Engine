#ifndef APPLICATION_EVENTS_H
#define APPLICATION_EVENTS_H

#include "Event.h"
#include "../IWindowSystem.h"

class WindowCloseEvent : public Event
{
public:
	explicit WindowCloseEvent(WindowHandle window) : m_Window(window) {}

	WindowHandle GetWindow() const { return m_Window; }

	EVENT_CLASS_TYPE(WindowClose)
	EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
	WindowHandle m_Window;
};

class WindowResizeEvent : public Event
{
public:
	WindowResizeEvent(WindowHandle window, unsigned int width, unsigned int height)
		: m_Window(window), m_Width(width), m_Height(height) {}

	WindowHandle GetWindow() const { return m_Window; }
	unsigned int GetWidth()  const { return m_Width;  }
	unsigned int GetHeight() const { return m_Height; }

	std::string ToString() const override
	{
		return std::string("WindowResizeEvent: ")
			 + std::to_string(m_Width) + ", " + std::to_string(m_Height);
	}

	EVENT_CLASS_TYPE(WindowResize)
	EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
	WindowHandle m_Window;
	unsigned int m_Width, m_Height;
};

#endif // APPLICATION_EVENTS_H
