#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

// Backend identifier used for runtime selection and hot-swap.
// Only values whose corresponding CORE_* macro is defined are valid at
// any given build. If both CORE_VULKAN and CORE_DX12 are defined the
// application can switch between them without restarting.
enum class RenderBackend
{
    None,
    Vulkan,
    D3D12,
};

#endif // RENDER_BACKEND_H
