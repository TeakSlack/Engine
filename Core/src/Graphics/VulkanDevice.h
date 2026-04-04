#ifndef VULKAN_DEVICE_H
#define VULKAN_DEVICE_H

#include "RenderDevice.h"
#include <memory>
#include <vector>
#include <cstdint>

struct GLFWwindow; // forward declaration — avoids pulling in glfw3.h

// -------------------------------------------------------------------------
// VulkanDevice
// Vulkan + NVRHI-Vulkan backend. All Vulkan and NVRHI-Vulkan types are
// hidden behind an Impl (PIMPL) so this header is free of vulkan.h and
// nvrhi/vulkan.h. Callers that need raw Vulkan objects must include
// vulkan.h themselves; this header does not pull it in transitively.
//
// VulkanDevice.cpp is intended to be conditionally compiled; other
// translation units can omit it entirely when the Vulkan backend is
// disabled without causing missing-symbol errors.
// -------------------------------------------------------------------------
class VulkanDevice : public IRenderDevice
{
public:
    explicit VulkanDevice(GLFWwindow* window);
    ~VulkanDevice() override;

    // IRenderDevice --------------------------------------------------------
    IGpuDevice* CreateDevice()                                         override;
    void        DestroyDevice()                                        override;
    void        CreateSwapchain(uint32_t w, uint32_t h)                override;
    void        RecreateSwapchain(uint32_t width, uint32_t height)     override;
    void        BeginFrame()                                           override;
    void        Present()                                              override;

    // Accessors ------------------------------------------------------------
    uint32_t GetCurrentImageIndex() const override;
    // Vulkan NDC Y is inverted vs GLM (+1 = bottom), so flip the sign.
    float    GetClipSpaceYSign()    const override { return -1.f; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // VULKAN_DEVICE_H
