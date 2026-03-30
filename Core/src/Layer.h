#ifndef LAYER_H
#define LAYER_H
#include <string>
#include "Events/Event.h"

// -------------------------------------------------------------------------
// Layer — a slice of the application update and event stack.
//
// Override only the hooks you need; every hook is optional (no pure virtuals
// on the hooks themselves so subclasses stay lean).
//
// Lifetime:
//   OnAttach  — called once when pushed onto the LayerStack.
//   OnDetach  — called once when popped from the LayerStack.
//   OnUpdate  — called every frame while the layer is active.
//   OnEvent   — called for each event, top-down through the stack.
//               Set event.Handled = true to stop further propagation.
// -------------------------------------------------------------------------
class Layer
{
public:
    explicit Layer(std::string name = "Layer") : m_DebugName(std::move(name)) {}
    virtual ~Layer() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnEvent(Event& event) {}

    const std::string& GetName() const { return m_DebugName; }

protected:
    std::string m_DebugName;
};

#endif // LAYER_H
