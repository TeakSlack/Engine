#ifndef ENGINE_H
#define ENGINE_H
#include "Util/LayerStack.h"
#include "Events/Event.h"
#include <string>
#include <vector>

// -------------------------------------------------------------------------
// IEngineSubmodule — interface every engine subsystem implements.
// -------------------------------------------------------------------------
class IEngineSubmodule
{
public:
    explicit IEngineSubmodule(std::string name) : m_Name(std::move(name)) {}
    virtual ~IEngineSubmodule() = default;

    virtual void Init()                   = 0;
    virtual void Shutdown()               = 0;
    virtual void Tick(float /*deltaTime*/) {}

    const std::string& GetName() const { return m_Name; }

private:
    std::string m_Name;
};

// -------------------------------------------------------------------------
// Engine — Meyer's singleton manager.
// -------------------------------------------------------------------------
class Engine
{
public:
    static Engine& Get();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // Submodule registration — call before Run().
    void RegisterSubmodule(IEngineSubmodule* submodule);

    // Layer management — delegates to the LayerStack.
    void PushLayer(Layer* layer);
    void PopLayer(Layer* layer);
    void PushOverlay(Layer* overlay);
    void PopOverlay(Layer* overlay);

    // Main loop. Calls InitSubmodules, runs until RequestStop(), then
    // calls ShutdownSubmodules in reverse registration order.
    void Run();

    // Inject an event into the layer stack. Call this from platform
    // callbacks (e.g. GLFW window/input callbacks).
    void OnEvent(Event& event);

    // Signal a clean exit from the loop (safe to call from any layer).
    void RequestStop() { m_Running = false; }

    bool IsRunning() const { return m_Running; }

private:
    Engine() = default;

    void InitSubmodules();
    void ShutdownSubmodules();

    std::vector<IEngineSubmodule*> m_Submodules;
    LayerStack                     m_LayerStack;
    bool                           m_Running = false;
};

#endif // ENGINE_H
