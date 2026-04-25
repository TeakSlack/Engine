# Engine

> *A simple render engine supporting both Vulkan and D3D12.*

This project only officially supports Windows at the moment.

## ⚙️ Requirements
* **Python:** Version 3.3 or higher
* **Windows:** Visual Studio 18 2026, Windows SDK
* **CMake:** Version 3.20 or higher
* **Vulkan SDK:** Version 1.4 or higher, installed by setup script


## 🛠️ Built Using
* [EnTT](https://github.com/skypjack/entt) - ECS
* [NVRHI](https://github.com/NVIDIA-RTX/NVRHI) - Render Hardware Abstraction
* [spdlog](https://github.com/gabime/spdlog) - Logging
* [fastgltf](https://github.com/spnda/fastgltf) - GLTF loading
* [nlohmann/json](https://github.com/nlohmann/json) - JSON

## 🌟 Highlights

### FrameGraph
  The render graph is probably the most substantial piece. Versioned resource handles that catch stale reads at assert time, automatic pass culling via reference counting, barrier inference from declared access types, and a transient resource system that creates and destroys GPU allocations purely from lifetime analysis. The blackboard gives passes implicit data sharing without coupling them directly.

  ### Backend Abstraction
  `IRenderDevice`, `IGpuDevice`, and `ICommandContext` fully abstract Vulkan and D3D12 implementation details using the PIMPL pattern. Switching backends simply requires a constructor swap.

  ### RenderPacket Design
  Plain data, no virtual dispatch, sort key that encodes layer, translucency, material, and depth in a single `uint64_t`. Arvo's AABB transform method for world bounds. Previous-frame transform for TAA velocity. Clean separation of submission policy (`SceneRenderSystem`) from rendering policy (passes).

  ## Setting Up

  To setup the project environment, run the appropriate setup script:

  ```
  cd scripts
  .\Setup-Win.bat
  ```

  This script will install required dependencies and generate project files. For regenerating project files later, use:

* **Windows:** `GenProjects-Win.bat`

Select the appropriate startup project (App or your own), build, and run. Ensure your system has the correct GPU drivers installed.

## Starting a New Project with Core

### Project Structure

Project is a separate executable linked against `Core`. The only file strictly necessary is a `main.cpp` and a Layer subclass.

```
MyProject/
    src/
        main.cpp
        MyLayer.h
        MyLayer.cpp
```
---
### 1. Entry Point

`main.cpp` wires together the window system, your layer, and the engine loop. Additional modules may be brought in using further submodules

```cpp
#include "MyLayer.h"
#include <Engine.h>
#include <Window/GLFWWindow.h>

int main()
{
    GLFWWindowSystem windowSystem;
    MyLayer layer;

    Engine::Get().RegisterSubmodule(&windowSystem);
    Engine::Get().PushLayer(&layer);
    Engine::Get().Run();
}
```

`RegisterSubmodule` must be called before `Run`. The engine calls `Init` on every registered submodule at startup, mitigating the static initialization order dilemma, and `Shutdown` in reverse order upon exit.

### 2. Implementing a Layer

A `Layer` is the primary unit of application object. Override only what you need - all hooks have a default no-op.

```cpp
class MyLayer : public Layer
{
  public:
      explicit MyLayer() : Layer("MyLayer") {}

      void OnAttach() override;
      void OnDetach() override;
      void OnUpdate(float deltaTime) override;
      void OnEvent(Event& event) override;

  private:
      IWindowSystem                    m_WS = nullptr;
      WindowHandle                     m_Window;
      std::unique_ptr<IRenderDevice>   m_RenderDevice;
      IGpuDevice*                      m_GpuDevice = nullptr;
      std::unique_ptr<ICommandContext> m_Cmd;
      std::unique_ptr<FrameGraph>      m_FG;
      // ... framebuffers, resources
}
```

### 3. `OnAttach` - Device and Swapchain Setup

```cpp
void MyLayer::OnAttach()
{
    // Get the window system
    m_WS = Engine::Get().GetSubmodule<GLFWWindowSystem>();

    // Open a window
    WindowDesc desc;
    desc.title = "My App";
    desc.width = 1280;
    desc.height = 720;
    m_Window = m_WS->OpenWindow(desc);

    auto* glfwWS = dynamic_cast<GLFWWindowSystem*>(m_WS);
#ifdef COMPILE_WITH_VULKAN
    m_RenderDevice = std::make_unique<VulkanDevice>(glfwWS->GetGLFWWindow());
#else
    m_RenderDevice = std::make_unique<D3D12Device>(glfwWS->GetNativeHandle());
#endif

    m_GpuDevice = m_RenderDevice->CreateDevice();

    auto extent = m_WS->GetExtent(m_Window);

    m_Cmd = m_GpuDevice->CreateCommandContext();
    m_FG = std::make_unique<FrameGraph>;

    // Create framebuffers from swapchain back-buffers + a depth buffer (if desired)
    // See Examples/Clear for an example
}
```

### 4. `OnUpdate` - The Frame Loop

While this project is still in early development, the current contract is `BeginFrame` -> build the FrameGraph -> compile -> execute -> present.

```cpp
void MyLayer::OnUpdate(float /*deltaTime*/)
{
    if (m_WS.ShouldClose(m_Window)) { Engine::Get().RequestStop(); return; }

    m_RenderDevice->BeginFrame();
    m_GpuDevice->RunGarbageCollection();

    uint32_t idx = m_RenderDevice->GetCurrentImageIndex();

    // --- FrameGraph ---
    m_FG->Reset();

    // Import the current back-buffer as a writable graph resource
    TextureDesc bbDesc;
    bbDesc.width = m_Width; bbDesc.height = m_Height;
    bbDesc.format = GpuFormat::BGRA8_UNORM;
    auto bb = m_FG->ImportMutableTexture(
        m_GpuDevice->GetBackBufferTextures()[idx], bbDesc,
        m_FrameCount++ == 0 ? ResourceLayout::Undefined : ResourceLayout::Present);

    // Add passes ...
    struct MyPassData { RGMutableTextureHandle target; };
    m_FG->AddCallbackPass<MyPassData>("MyPass",
        [&](PassBuilder& builder, MyPassData& data) {
            data.target = builder.WriteTexture(bb);
        },
        [this, idx](const MyPassData& data, const RenderPassResources& res, ICommandContext*cmd) {
            // record commands using cmd
            // transition back to Present when done
            cmd->TransitionTexture(res.GetTexture(data.target), ResourceLayout::Present);
        });

    m_FG->Compile();

    m_Cmd->Open();
    m_FG->Execute(m_GpuDevice, m_Cmd.get());
    m_Cmd->Close();

    m_GpuDevice->ExecuteCommandContext(*m_Cmd);
    m_RenderDevice->Present();
}
```
### 5. Handling Events

EventDispatcher is a stack-allocated, type-safe dispatcher. Return true from a handler to mark the event handled and stop it propagating to lower layers.

```cpp
void MyLayer::OnEvent(Event& event)
{
    EventDispatcher d(event);

    d.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        // recreate swapchain
        return false; // don't consume — let other layers see it too
    });

    d.Dispatch<KeyPressedEvent>([](KeyPressedEvent& e) {
        return false;
});
```

### 6. Cleanup

```cpp
void MMyLayer::OnDetach()
{
    m_GpuDevice->WaitForIdle();
    // destroy framebuffers, GPU resources
    m_Cmd.reset();
    m_FG.reset();
    m_GpuDevice = nullptr;
    m_RenderDevice.reset();
    m_WS->CloseWindow(m_Window);
}
```

`WaitForIdle` before destroying anything else is critical - the GPU may still be executing commands if not called.