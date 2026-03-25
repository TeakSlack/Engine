# ✨ DeferredRender

> a tiny vulkan renderer, learning as it goes 🌱

a from-scratch vulkan rendering project built to learn the API from the ground up — starting with a spinning, checkerboard-textured cube and working toward a full deferred rendering pipeline.

---

## 🎮 what it does (so far)

- spins a colorful cube on screen (very important)
- per-face colors with a procedural checkerboard pattern
- proper MVP transforms with perspective projection
- depth-tested rendering so faces sort correctly
- double-buffered frames-in-flight for smooth presents
- validation layer support with nicely formatted log output

---

## 🛠️ built with

| thing | what for |
|---|---|
| **Vulkan** | graphics API |
| **GLFW** | window + input |
| **GLM** | math (vectors, matrices) |
| **VulkanMemoryAllocator** | gpu memory management |
| **spdlog** | logging |
| **Premake5** | build system |

---

## 🚀 building

requires the **Vulkan SDK** to be installed and `VULKAN_SDK` set in your environment.

```bash
# generate project files (visual studio 2022)
scripts/gen-vs2022.bat

# then open DeferredRender.sln and build!
```

---

## 🏃 running

```
DeferredRender.exe [options]

  -v, --verbose           show trace/debug messages + source locations
  -l, --log-file <path>   mirror all output to a log file
  -h, --help              print this message
```

verbose mode is automatically enabled in debug builds. 🐛

---

## 🗂️ project layout

```
DeferredRender/src/
  main.cpp              — app entry, main loop, render pass, framebuffers
  vk_context.{h,cpp}    — instance, physical device, logical device
  vk_swapchain.{h,cpp}  — swapchain, sync objects, acquire + present
  vk_buffer.{h,cpp}     — buffers, images, staging uploads (via VMA)
  vk_mesh.{h,cpp}       — vertex layout, cube geometry, mesh upload
  vk_uniforms.{h,cpp}   — UBO struct, per-frame mapped uniform buffers
  vk_descriptors.{h,cpp}— descriptor pool, set layout, set allocation
  vk_debug.{h,cpp}      — validation layer callback → spdlog
  pipeline_builder.{h,cpp} — fluent pipeline construction API
  logger.{h,cpp}        — named spdlog sub-loggers (core/vulkan/render/...)
  shader/
    cube.vert.spv        — compiled vertex shader
    cube.frag.spv        — compiled fragment shader
```

---

## 🗺️ roadmap

- [x] vulkan context + swapchain
- [x] vertex/index buffers via staging
- [x] descriptor sets + uniform buffer objects
- [x] spinning cube with per-face color + checkerboard UVs
- [x] depth testing
- [ ] texture sampling
- [ ] multiple objects / scene graph
- [ ] g-buffer pass (albedo, normal, position)
- [ ] deferred lighting pass
- [ ] shadow mapping
- [ ] post-processing (bloom? tonemapping? 🤩)

---

## 📝 notes

- shaders must be pre-compiled to `.spv` before running — use `glslc` from the vulkan sdk
- the renderer expects to be run from the project root so relative shader paths resolve correctly

---

*made with curiosity and too many vulkan spec tabs open* 💙
