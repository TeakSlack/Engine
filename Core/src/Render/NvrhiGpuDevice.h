#ifndef NVRHI_GPU_DEVICE_H
#define NVRHI_GPU_DEVICE_H

// NVRHI implementation of IGpuDevice and ICommandContext.
// This header is internal to Core; App code never includes it.

#include "IGpuDevice.h"
#include <nvrhi/nvrhi.h>
#include <vector>

// ---------------------------------------------------------------------------
// Forward declaration — defined in the .cpp
// ---------------------------------------------------------------------------
class NvrhiCommandContext;

// ---------------------------------------------------------------------------
// Internal pool entry types
// ---------------------------------------------------------------------------
struct BindlessLayoutEntry
{
    nvrhi::BindingLayoutHandle handle;
    uint32_t                   maxCapacity = 0;
};

struct DescriptorTableEntry
{
    nvrhi::DescriptorTableHandle handle;
    uint32_t                     nextFreeSlot = 0;
    uint32_t                     capacity     = 0;
};

// ---------------------------------------------------------------------------
// Minimal generic pool: index 0 == null/invalid
// ---------------------------------------------------------------------------
template<typename T>
class ResourcePool
{
    std::vector<T> m_Items;
public:
    ResourcePool() { m_Items.emplace_back(); } // slot 0 is the null sentinel

    uint32_t Add(T item)
    {
        m_Items.push_back(std::move(item));
        return static_cast<uint32_t>(m_Items.size() - 1);
    }
    T&       Get(uint32_t id)       { return m_Items[id]; }
    const T& Get(uint32_t id) const { return m_Items[id]; }
    void     Release(uint32_t id)   { if (id) { T empty; m_Items[id] = std::move(empty); } }
};

// ---------------------------------------------------------------------------
// NvrhiGpuDevice
// ---------------------------------------------------------------------------
class NvrhiGpuDevice : public IGpuDevice
{
public:
    explicit NvrhiGpuDevice(nvrhi::IDevice* device);
    ~NvrhiGpuDevice() override = default;

    // IGpuDevice --------------------------------------------------------
    GpuBuffer  CreateBuffer(const BufferDesc& desc)  override;
    void       DestroyBuffer(GpuBuffer handle)       override;

    GpuTexture CreateTexture(const TextureDesc& desc) override;
    void       DestroyTexture(GpuTexture handle)      override;

    GpuSampler CreateSampler(const SamplerDesc& desc) override;
    void       DestroySampler(GpuSampler handle)      override;

    GpuShader  CreateShader(const ShaderDesc& desc)   override;
    void       DestroyShader(GpuShader handle)        override;

    GpuInputLayout CreateInputLayout(
        const std::vector<VertexAttributeDesc>& attribs,
        GpuShader vertexShader) override;
    void DestroyInputLayout(GpuInputLayout handle) override;

    GpuBindingLayout CreateBindingLayout(const BindingLayoutDesc& desc) override;
    void             DestroyBindingLayout(GpuBindingLayout handle)      override;

    GpuBindingSet CreateBindingSet(const BindingSetDesc& desc,
                                    GpuBindingLayout layout) override;
    void          DestroyBindingSet(GpuBindingSet handle)    override;

    // -----------------------------------------------------------------------
    // Bindless layout + descriptor table
    // -----------------------------------------------------------------------
    GpuBindlessLayout  CreateBindlessLayout(const BindlessLayoutDesc& desc) override;
    void               DestroyBindlessLayout(GpuBindlessLayout handle)      override;

    GpuDescriptorTable CreateDescriptorTable(GpuBindlessLayout layout)      override;
    void               DestroyDescriptorTable(GpuDescriptorTable handle)    override;

    DescriptorIndex    WriteTexture(GpuDescriptorTable table, GpuTexture texture,
                           DescriptorIndex slot = InvalidDescriptorIndex) override;
    DescriptorIndex    WriteBuffer(GpuDescriptorTable table, GpuBuffer buffer,
                           DescriptorIndex slot = InvalidDescriptorIndex) override;
    DescriptorIndex    WriteSampler(GpuDescriptorTable table, GpuSampler sampler,
                           DescriptorIndex slot = InvalidDescriptorIndex) override;

    GpuFramebuffer CreateFramebuffer(const FramebufferDesc& desc) override;
    void           DestroyFramebuffer(GpuFramebuffer handle)      override;
    std::pair<uint32_t, uint32_t> GetFramebufferSize(
        GpuFramebuffer handle) const override;

    GpuGraphicsPipeline CreateGraphicsPipeline(
        const GraphicsPipelineDesc& desc, GpuFramebuffer framebuffer) override;
    void DestroyGraphicsPipeline(GpuGraphicsPipeline handle) override;

    GpuComputePipeline CreateComputePipeline(
        const ComputePipelineDesc& desc) override;
    void DestroyComputePipeline(GpuComputePipeline handle) override;

    std::unique_ptr<ICommandContext> CreateCommandContext() override;
    void ExecuteCommandContext(ICommandContext& ctx)        override;

    void WaitForIdle()          override;
    void RunGarbageCollection() override;

    const std::vector<GpuTexture>& GetBackBufferTextures() const override;

    // ---- Called by IRenderDevice implementations ----
    void RegisterBackBuffers(const std::vector<nvrhi::TextureHandle>& nativeBuffers);
    void ClearBackBuffers();

    // ---- Accessors used by NvrhiCommandContext ----
    nvrhi::IDevice*              GetDevice()       const { return m_Device; }
    nvrhi::BufferHandle          GetBuffer        (uint32_t id) { return m_Buffers.Get(id); }
    nvrhi::TextureHandle         GetTexture       (uint32_t id) { return m_Textures.Get(id); }
    nvrhi::SamplerHandle         GetSampler       (uint32_t id) { return m_Samplers.Get(id); }
    nvrhi::InputLayoutHandle     GetInputLayout   (uint32_t id) { return m_InputLayouts.Get(id); }
    nvrhi::BindingLayoutHandle   GetBindingLayout (uint32_t id) { return m_BindingLayouts.Get(id); }
    nvrhi::BindingSetHandle      GetBindingSet    (uint32_t id) { return m_BindingSets.Get(id); }
    nvrhi::FramebufferHandle     GetFramebuffer   (uint32_t id) { return m_Framebuffers.Get(id); }
    nvrhi::GraphicsPipelineHandle GetGraphicsPipeline(uint32_t id) { return m_GraphicsPipelines.Get(id); }
    nvrhi::ComputePipelineHandle GetComputePipeline (uint32_t id) { return m_ComputePipelines.Get(id); }
    const FramebufferDesc&       GetFramebufferDesc (uint32_t id) const { return m_FramebufferDescs.Get(id); }
    nvrhi::IDescriptorTable*     GetDescriptorTableNative(uint32_t id) { return m_DescriptorTables.Get(id).handle.Get(); }

private:
    nvrhi::IDevice* m_Device = nullptr; // non-owning

    ResourcePool<nvrhi::BufferHandle>           m_Buffers;
    ResourcePool<nvrhi::TextureHandle>          m_Textures;
    ResourcePool<nvrhi::SamplerHandle>          m_Samplers;
    ResourcePool<nvrhi::ShaderHandle>           m_Shaders;
    ResourcePool<nvrhi::InputLayoutHandle>      m_InputLayouts;
    ResourcePool<nvrhi::BindingLayoutHandle>    m_BindingLayouts;
    ResourcePool<nvrhi::BindingSetHandle>       m_BindingSets;
    ResourcePool<BindlessLayoutEntry>           m_BindlessLayouts;
    ResourcePool<DescriptorTableEntry>          m_DescriptorTables;
    ResourcePool<nvrhi::FramebufferHandle>      m_Framebuffers;
    ResourcePool<nvrhi::GraphicsPipelineHandle> m_GraphicsPipelines;
    ResourcePool<nvrhi::ComputePipelineHandle>  m_ComputePipelines;
    // Cached descriptors used by NvrhiCommandContext for clear operations
    ResourcePool<FramebufferDesc>               m_FramebufferDescs;

    std::vector<GpuTexture> m_BackBufferTextures;
};

#endif // NVRHI_GPU_DEVICE_H
