#ifndef RENDER_DEVICE_H
#define RENDER_DEVICE_H

#include <Render/IGpuDevice.h>
#include <cstdint>

// -------------------------------------------------------------------------
// IRenderDevice
// Window/swapchain lifecycle layer. Owns the OS surface and present queue.
// CreateDevice() returns the IGpuDevice that App code uses for all GPU work.
// -------------------------------------------------------------------------
class IRenderDevice
{
public:
    virtual ~IRenderDevice() = default;

    virtual IGpuDevice* CreateDevice()                               = 0;
    virtual void        DestroyDevice()                              = 0;

    virtual void CreateSwapchain(uint32_t width, uint32_t height)    = 0;
    virtual void RecreateSwapchain(uint32_t width, uint32_t height)  = 0;
    virtual void BeginFrame()                                        = 0;
    virtual void Present()                                           = 0;

    virtual uint32_t GetCurrentImageIndex() const = 0;

    // Both Vulkan and D3D12 (via DXC/NVRHI) require a Y flip relative to
    // GLM's OpenGL-convention output. Multiply projection[1][1] by this
    // value to correct clip-space Y without any #ifdef at the call site.
    virtual float GetClipSpaceYSign() const = 0;
};

#endif // RENDER_DEVICE_H
