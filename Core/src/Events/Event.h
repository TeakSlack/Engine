#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <functional>

// -------------------------------------------------------------------------
// EventType — one entry per concrete event class.
// -------------------------------------------------------------------------
enum class EventType
{
    None = 0,

    // Application / window
    WindowClose, WindowResize,

    // Keyboard
    KeyPressed, KeyReleased, KeyTyped,

    // Mouse
    MouseButtonPressed, MouseButtonReleased,
    MouseMoved, MouseScrolled,
};

// -------------------------------------------------------------------------
// EventCategory — bit-flag set.
// Plain int enum (not enum class) so OR-combinations need no casts.
// -------------------------------------------------------------------------
enum EventCategory : int
{
    EventCategoryNone        = 0,
    EventCategoryApplication = 1 << 0,
    EventCategoryInput       = 1 << 1,
    EventCategoryKeyboard    = 1 << 2,
    EventCategoryMouse       = 1 << 3,
    EventCategoryMouseButton = 1 << 4,
};

// -------------------------------------------------------------------------
// Boilerplate helpers — paste into every concrete event class body.
// EVENT_CLASS_TYPE  provides GetStaticType() (used by EventDispatcher without
//                  an instance) and the two virtual overrides.
// EVENT_CLASS_CATEGORY  implements GetCategoryFlags().
// -------------------------------------------------------------------------
#define EVENT_CLASS_TYPE(type)                                              \
    static  EventType GetStaticType()         { return EventType::type; }  \
    EventType   GetEventType() const override { return GetStaticType(); }  \
    const char* GetName()      const override { return #type; }

#define EVENT_CLASS_CATEGORY(category)                                      \
    int GetCategoryFlags() const override { return (category); }

// -------------------------------------------------------------------------
// Event — abstract base for all events.
// `Handled` is a plain public bool so the layer stack can inspect it
// without a getter on the hot path.
// -------------------------------------------------------------------------
class Event
{
public:
    bool Handled = false;

    virtual ~Event() = default;

    virtual EventType   GetEventType()     const = 0;
    virtual const char* GetName()          const = 0;
    virtual int         GetCategoryFlags() const = 0;

    virtual std::string ToString() const { return GetName(); }

    bool IsInCategory(EventCategory category) const
    {
        return GetCategoryFlags() & category;
    }
};

// -------------------------------------------------------------------------
// EventDispatcher — type-safe, stack-allocated single-event dispatch.
//
// Usage (inside a Layer::OnEvent override):
//
//   EventDispatcher d(event);
//   d.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
//       OnResize(e.GetWidth(), e.GetHeight());
//       return true;  // mark handled — stops propagation
//   });
//
// The callback returns bool: true = mark event handled, false = don't.
// Multiple Dispatch calls on the same dispatcher are additive — all
// matching handlers run and any true return ORs into Handled.
// -------------------------------------------------------------------------
class EventDispatcher
{
public:
    explicit EventDispatcher(Event& event) : m_Event(event) {}

    template<typename T, typename F>
    bool Dispatch(const F& func)
    {
        if (m_Event.GetEventType() == T::GetStaticType())
        {
            m_Event.Handled |= func(static_cast<T&>(m_Event));
            return true;
        }
        return false;
    }

private:
    Event& m_Event;
};

#endif // EVENT_H