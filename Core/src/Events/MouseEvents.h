#ifndef MOUSE_EVENTS_H
#define MOUSE_EVENTS_H

#include "Event.h"

// -------------------------------------------------------------------------
class MouseMovedEvent : public Event
{
public:
    MouseMovedEvent(float x, float y) : m_MouseX(x), m_MouseY(y) {}

    float GetX() const { return m_MouseX; }
    float GetY() const { return m_MouseY; }

    std::string ToString() const override
    {
        return std::string("MouseMovedEvent: ")
             + std::to_string(m_MouseX) + ", " + std::to_string(m_MouseY);
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

private:
    float m_MouseX, m_MouseY;
};

// -------------------------------------------------------------------------
class MouseScrolledEvent : public Event
{
public:
    MouseScrolledEvent(float xOffset, float yOffset)
        : m_XOffset(xOffset), m_YOffset(yOffset) {}

    float GetXOffset() const { return m_XOffset; }
    float GetYOffset() const { return m_YOffset; }

    std::string ToString() const override
    {
        return std::string("MouseScrolledEvent: ")
             + std::to_string(m_XOffset) + ", " + std::to_string(m_YOffset);
    }

    EVENT_CLASS_TYPE(MouseScrolled)
    EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

private:
    float m_XOffset, m_YOffset;
};

// -------------------------------------------------------------------------
// MouseButtonEvent — shared base for button pressed/released.
// Uses int for the button code so GLFW constants need no casts.
// -------------------------------------------------------------------------
class MouseButtonEvent : public Event
{
public:
    int GetMouseButton() const { return m_Button; }

    EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)

protected:
    explicit MouseButtonEvent(int button) : m_Button(button) {}
    int m_Button;
};

// -------------------------------------------------------------------------
class MouseButtonPressedEvent : public MouseButtonEvent
{
public:
    explicit MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

    std::string ToString() const override
    {
        return std::string("MouseButtonPressedEvent: ") + std::to_string(m_Button);
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
};

// -------------------------------------------------------------------------
class MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
    explicit MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

    std::string ToString() const override
    {
        return std::string("MouseButtonReleasedEvent: ") + std::to_string(m_Button);
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
};

#endif // MOUSE_EVENTS_H