#ifndef D3D12_DEVICE_H
#define D3D12_DEVICE_H

#include "RenderDevice.h"
#include <memory>
#include <vector>
#include <cstdint>

// -------------------------------------------------------------------------
// D3D12Device
// D3D12 + NVRHI-D3D12 backend. All D3D12 and DXGI types are hidden behind
// an Impl (PIMPL) so this header is free of d3d12.h and dxgi.h. Callers
// that need raw D3D12 objects must include those headers themselves; this
// header does not pull them in transitively.
//
// D3D12Device.cpp is intended to be conditionally compiled; other
// translation units can omit it entirely when the D3D12 backend is
// disabled without causing missing-symbol errors.
// -------------------------------------------------------------------------
class D3D12Device : public IRenderDevice
{
public:
    explicit D3D12Device(void* hwnd);
    ~D3D12Device() override;

    // IRenderDevice --------------------------------------------------------
    IGpuDevice* CreateDevice()                                         override;
    void        DestroyDevice()                                        override;
    void        CreateSwapchain(uint32_t w, uint32_t h)                override;
    void        RecreateSwapchain(uint32_t width, uint32_t height)     override;
    void        BeginFrame()                                           override;
    void        Present()                                              override;

    // Accessors ------------------------------------------------------------
    uint32_t GetCurrentImageIndex() const override;
    // DXC/NVRHI D3D12 requires the same Y flip as Vulkan when using GLM.
    float    GetClipSpaceYSign()    const override { return -1.f; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // D3D12_DEVICE_H
