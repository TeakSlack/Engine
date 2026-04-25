#ifndef I_COMMAND_CONTEXT_H
#define I_COMMAND_CONTEXT_H

// Command recording interface — no NVRHI, Vulkan, or D3D12 headers exposed.
// Implementations accumulate Set* calls into backend state and flush it
// lazily before each draw or dispatch.

#include "GpuTypes.h"

class ICommandContext
{
public:
    virtual ~ICommandContext() = default;

    // -----------------------------------------------------------------------
    // Lifetime
    // -----------------------------------------------------------------------
    virtual void Open()  = 0;
    virtual void Close() = 0;

    // -----------------------------------------------------------------------
    // Upload
    // -----------------------------------------------------------------------
    virtual void WriteBuffer(GpuBuffer dst, const void* data, size_t byteSize,
                              size_t dstOffset = 0) = 0;

    virtual void WriteTexture(GpuTexture dst, uint32_t arraySlice, uint32_t mipLevel,
                               const void* data, size_t rowPitch,
                               size_t depthPitch = 0) = 0;

    // -----------------------------------------------------------------------
    // Copy
    // -----------------------------------------------------------------------
    virtual void CopyBuffer(GpuBuffer dst, GpuBuffer src, size_t byteSize,
                             size_t dstOffset = 0, size_t srcOffset = 0) = 0;

    virtual void CopyTexture(GpuTexture dst, GpuTexture src) = 0;

    // -----------------------------------------------------------------------
    // Explicit clears (outside of a render pass, e.g. shadow maps, UAVs)
    // -----------------------------------------------------------------------
    virtual void ClearColor(GpuTexture texture, const ClearValue& color) = 0;
    virtual void ClearDepth(GpuTexture texture, float depth = 1.0f) = 0;
    virtual void ClearDepthStencil(GpuTexture texture, float depth, uint8_t stencil) = 0;

    // -----------------------------------------------------------------------
    // Render pass
    // BeginRenderPass binds the framebuffer and issues any requested clears.
    // All Set* and Draw* calls must occur between Begin and End.
    // -----------------------------------------------------------------------
    virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void EndRenderPass() = 0;

    // -----------------------------------------------------------------------
    // Graphics state
    // All calls are deferred; state is submitted on the next Draw call.
    // -----------------------------------------------------------------------
    virtual void SetGraphicsPipeline(GpuGraphicsPipeline pipeline) = 0;
    virtual void SetViewport(const Viewport& vp) = 0;
    virtual void SetViewport(float x, float y, float width, float height,
        float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
    virtual void SetScissor(const ScissorRect& rect) = 0;
	virtual void SetScissor(int x, int y, int width, int height) = 0;
    virtual void SetVertexBuffer(uint32_t slot, GpuBuffer buffer,
                                  uint64_t byteOffset = 0) = 0;
    virtual void SetIndexBuffer(GpuBuffer buffer, GpuFormat indexFormat,
                                 uint64_t byteOffset = 0) = 0;
    virtual void SetBindingSet(GpuBindingSet set, uint32_t slot = 0) = 0;

    // -----------------------------------------------------------------------
    // Draw
    // -----------------------------------------------------------------------
    virtual void Draw(const DrawArgs& args) = 0;
    virtual void DrawIndexed(const DrawIndexedArgs& args) = 0;
    virtual void DrawIndirect(GpuBuffer argsBuffer, uint64_t byteOffset = 0) = 0;
    virtual void DrawIndexedIndirect(GpuBuffer argsBuffer,
                                      uint64_t byteOffset = 0) = 0;

    // -----------------------------------------------------------------------
    // Compute state and dispatch
    // -----------------------------------------------------------------------
    virtual void SetComputePipeline(GpuComputePipeline pipeline) = 0;
    virtual void SetComputeBindingSet(GpuBindingSet set, uint32_t slot = 0) = 0;
    virtual void Dispatch(const DispatchArgs& args) = 0;
    virtual void DispatchIndirect(GpuBuffer argsBuffer,
                                   uint64_t byteOffset = 0) = 0;

    // -----------------------------------------------------------------------
    // Resource transitions
    // -----------------------------------------------------------------------
    virtual void TransitionTexture(GpuTexture texture, ResourceLayout layout) = 0;
    virtual void TransitionBuffer(GpuBuffer buffer, ResourceLayout layout)    = 0;

    // -----------------------------------------------------------------------
    // Debug markers (no-ops in release)
    // -----------------------------------------------------------------------
    virtual void BeginMarker(const char* name) = 0;
    virtual void EndMarker() = 0;
};

#endif // I_COMMAND_CONTEXT_H
