#ifndef RENDER_DEVICE_H
#define RENDER_DEVICE_H

#include <nvrhi/nvrhi.h>
#include <cstdint>

// -------------------------------------------------------------------------
// IRenderDevice
// Backend-agnostic interface covering the lifetime of the GPU device and
// swapchain. Each backend (Vulkan, D3D11, D3D12) provides one concrete
// implementation.
//
// Call order:
//   CreateDevice()  — creates instance/device/queues; returns nvrhi::IDevice*
//   CreateSwapchain(w, h)
//   ... render loop: Present() each frame ...
//   DestroyDevice() — device must be idle before calling
// -------------------------------------------------------------------------
class IRenderDevice
{
public:
    virtual ~IRenderDevice() = default;

    virtual nvrhi::IDevice* CreateDevice()                           = 0;
    virtual void            DestroyDevice()                          = 0;

    virtual void CreateSwapchain(uint32_t width, uint32_t height)    = 0;
    virtual void RecreateSwapchain(uint32_t width, uint32_t height)  = 0;
    virtual void BeginFrame()                                        = 0;
    virtual void Present()                                           = 0;
};

#endif // RENDER_DEVICE_H
