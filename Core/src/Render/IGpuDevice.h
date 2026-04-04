#ifndef I_GPU_DEVICE_H
#define I_GPU_DEVICE_H

// Resource creation interface and command context factory.
// No NVRHI, Vulkan, or D3D12 headers exposed.

#include "GpuTypes.h"
#include "ICommandContext.h"
#include <memory>
#include <utility>
#include <vector>

class IGpuDevice
{
public:
    virtual ~IGpuDevice() = default;

    // -----------------------------------------------------------------------
    // Buffers
    // -----------------------------------------------------------------------
    virtual GpuBuffer  CreateBuffer(const BufferDesc& desc)  = 0;
    virtual void       DestroyBuffer(GpuBuffer handle)       = 0;

    // -----------------------------------------------------------------------
    // Textures
    // -----------------------------------------------------------------------
    virtual GpuTexture CreateTexture(const TextureDesc& desc) = 0;
    virtual void       DestroyTexture(GpuTexture handle)      = 0;

    // -----------------------------------------------------------------------
    // Samplers
    // -----------------------------------------------------------------------
    virtual GpuSampler CreateSampler(const SamplerDesc& desc) = 0;
    virtual void       DestroySampler(GpuSampler handle)      = 0;

    // -----------------------------------------------------------------------
    // Shaders
    // -----------------------------------------------------------------------
    virtual GpuShader  CreateShader(const ShaderDesc& desc)   = 0;
    virtual void       DestroyShader(GpuShader handle)        = 0;

    // -----------------------------------------------------------------------
    // Input layout (tied to a vertex shader)
    // -----------------------------------------------------------------------
    virtual GpuInputLayout CreateInputLayout(
        const std::vector<VertexAttributeDesc>& attribs,
        GpuShader vertexShader) = 0;
    virtual void DestroyInputLayout(GpuInputLayout handle) = 0;

    // -----------------------------------------------------------------------
    // Binding layout
    // -----------------------------------------------------------------------
    virtual GpuBindingLayout CreateBindingLayout(const BindingLayoutDesc& desc) = 0;
    virtual void             DestroyBindingLayout(GpuBindingLayout handle)      = 0;

    // -----------------------------------------------------------------------
    // Binding set
    // -----------------------------------------------------------------------
    virtual GpuBindingSet CreateBindingSet(const BindingSetDesc& desc,
                                            GpuBindingLayout layout) = 0;
    virtual void          DestroyBindingSet(GpuBindingSet handle)    = 0;

    // -----------------------------------------------------------------------
    // Framebuffer
    // -----------------------------------------------------------------------
    virtual GpuFramebuffer CreateFramebuffer(const FramebufferDesc& desc) = 0;
    virtual void           DestroyFramebuffer(GpuFramebuffer handle)      = 0;

    // Returns the pixel dimensions of the first colour attachment.
    virtual std::pair<uint32_t, uint32_t> GetFramebufferSize(
        GpuFramebuffer handle) const = 0;

    // -----------------------------------------------------------------------
    // Pipelines
    // -----------------------------------------------------------------------
    virtual GpuGraphicsPipeline CreateGraphicsPipeline(
        const GraphicsPipelineDesc& desc, GpuFramebuffer framebuffer) = 0;
    virtual void DestroyGraphicsPipeline(GpuGraphicsPipeline handle)  = 0;

    virtual GpuComputePipeline CreateComputePipeline(
        const ComputePipelineDesc& desc) = 0;
    virtual void DestroyComputePipeline(GpuComputePipeline handle)    = 0;

    // -----------------------------------------------------------------------
    // Command context
    // -----------------------------------------------------------------------
    virtual std::unique_ptr<ICommandContext> CreateCommandContext() = 0;
    virtual void ExecuteCommandContext(ICommandContext& ctx)        = 0;

    // -----------------------------------------------------------------------
    // Device management
    // -----------------------------------------------------------------------
    virtual void WaitForIdle()          = 0;
    virtual void RunGarbageCollection() = 0;

    // -----------------------------------------------------------------------
    // Swapchain back-buffer access (set by IRenderDevice after swapchain init)
    // -----------------------------------------------------------------------
    virtual const std::vector<GpuTexture>& GetBackBufferTextures() const = 0;
};

#endif // I_GPU_DEVICE_H
