#include "Engine.h"
#include "Events/ApplicationEvents.h"
#include "Util/Log.h"
#include <chrono>

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
    Log::Get().Init();
    InitSubmodules();
    m_Running = true;

    for (auto* layer : m_LayerStack)
        layer->OnAttach();

    auto lastFrameTime = std::chrono::steady_clock::now();

    while (m_Running)
    {
        auto  now       = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime   = now;

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
// Editor mode lifecycle
// -------------------------------------------------------------------------
void Engine::EditorInit()
{
    Log::Get().Init();
    InitSubmodules();
    m_Running = true;
}

void Engine::EditorTick(float deltaTime)
{
    for (auto* sub : m_Submodules)
        sub->Tick(deltaTime);
}

void Engine::EditorShutdown()
{
    ShutdownSubmodules();
    m_Running = false;
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
