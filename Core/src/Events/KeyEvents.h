#ifndef KEY_EVENTS_H
#define KEY_EVENTS_H

#include "Event.h"

// -------------------------------------------------------------------------
// KeyEvent — shared base for keyboard events.
// Uses int for the keycode so GLFW constants can be passed without casts.
// -------------------------------------------------------------------------
class KeyEvent : public Event
{
public:
    int GetKeyCode() const { return m_KeyCode; }

    EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryKeyboard)

protected:
    explicit KeyEvent(int keycode) : m_KeyCode(keycode) {}
    int m_KeyCode;
};

// -------------------------------------------------------------------------
class KeyPressedEvent : public KeyEvent
{
public:
    // repeatCount: 0 on the initial press, >0 on held-key repeats.
    KeyPressedEvent(int keycode, int repeatCount)
        : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

    int GetRepeatCount() const { return m_RepeatCount; }

    std::string ToString() const override
    {
        return std::string("KeyPressedEvent: ") + std::to_string(m_KeyCode)
             + " (" + std::to_string(m_RepeatCount) + " repeats)";
    }

    EVENT_CLASS_TYPE(KeyPressed)

private:
    int m_RepeatCount;
};

// -------------------------------------------------------------------------
class KeyReleasedEvent : public KeyEvent
{
public:
    explicit KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

    std::string ToString() const override
    {
        return std::string("KeyReleasedEvent: ") + std::to_string(m_KeyCode);
    }

    EVENT_CLASS_TYPE(KeyReleased)
};

// -------------------------------------------------------------------------
// KeyTypedEvent — carries a Unicode codepoint from GLFW's char callback.
// Use this for text input; use KeyPressedEvent for action bindings.
// -------------------------------------------------------------------------
class KeyTypedEvent : public KeyEvent
{
public:
    explicit KeyTypedEvent(int codepoint) : KeyEvent(codepoint) {}

    std::string ToString() const override
    {
        return std::string("KeyTypedEvent: ") + std::to_string(m_KeyCode);
    }

    EVENT_CLASS_TYPE(KeyTyped)
};

#endif // KEY_EVENTS_H