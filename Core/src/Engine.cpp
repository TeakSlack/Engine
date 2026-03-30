#include "Engine.h"
#include "Events/ApplicationEvents.h"
#include "IWindowSystem.h"

Engine& Engine::Get()
{
    static Engine s_Instance;
    return s_Instance;
}

// -------------------------------------------------------------------------
// Submodule registration
// -------------------------------------------------------------------------
void Engine::RegisterSubmodule(IEngineSubmodule* submodule)
{
    m_Submodules.push_back(submodule);
}

// -------------------------------------------------------------------------
// Layer delegation
// -------------------------------------------------------------------------
void Engine::PushLayer(Layer* layer)      { m_LayerStack.PushLayer(layer);     }
void Engine::PopLayer(Layer* layer)       { m_LayerStack.PopLayer(layer);      }
void Engine::PushOverlay(Layer* overlay)  { m_LayerStack.PushOverlay(overlay); }
void Engine::PopOverlay(Layer* overlay)   { m_LayerStack.PopOverlay(overlay);  }

// -------------------------------------------------------------------------
// Subsystem lifecycle
// -------------------------------------------------------------------------
void Engine::InitSubmodules()
{
    for (auto* sub : m_Submodules)
        sub->Init();
}

void Engine::ShutdownSubmodules()
{
    // Reverse order: the last registered subsystem shuts down first,
    // mirroring the dependency inversion principle from Gregory §15.4.
    for (auto it = m_Submodules.rbegin(); it != m_Submodules.rend(); ++it)
        (*it)->Shutdown();
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void Engine::Run()
{
    InitSubmodules();
    m_Running = true;

    for (auto* layer : m_LayerStack)
        layer->OnAttach();

    // Find the first registered submodule that implements IWindowSystem
    // and use it as the clock source for deltaTime.
    IWindowSystem* clock = nullptr;
    for (auto* sub : m_Submodules)
    {
        if ((clock = dynamic_cast<IWindowSystem*>(sub)))
            break;
    }

    float lastFrameTime = clock ? static_cast<float>(clock->GetTime()) : 0.0f;

    while (m_Running)
    {
        float time      = clock ? static_cast<float>(clock->GetTime()) : 0.0f;
        float deltaTime = time - lastFrameTime;
        lastFrameTime   = time;

        // Tick all submodules (input, audio, physics, etc.)
        for (auto* sub : m_Submodules)
            sub->Tick(deltaTime);

        // Update layers front-to-back: base layers before overlays.
        for (auto* layer : m_LayerStack)
            layer->OnUpdate(deltaTime);
    }

    ShutdownSubmodules();
}

// -------------------------------------------------------------------------
// Event propagation
// -------------------------------------------------------------------------
void Engine::OnEvent(Event& event)
{
    // The engine handles WindowClose centrally so the loop always exits,
    // even if no layer intercepts it. Returns false so layers still observe
    // the event and can do their own cleanup (e.g. prompt to save).
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent>(
        [this](WindowCloseEvent&) { RequestStop(); return false; }
    );

    // Propagate top-down: overlays (back of vector) first.
    for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
    {
        if (event.Handled)
            break;
        (*it)->OnEvent(event);
    }
}
