#include "NvrhiGpuDevice.h"
#include "GpuTypes.h"
#include "Util/Log.h"

// ============================================================================
// Type-conversion helpers  (our types → NVRHI types)
// ============================================================================

static nvrhi::Format ToNvrhiFormat(GpuFormat f)
{
    switch (f)
    {
    case GpuFormat::R8_UNORM:     return nvrhi::Format::R8_UNORM;
    case GpuFormat::RGBA8_UNORM:  return nvrhi::Format::RGBA8_UNORM;
    case GpuFormat::RGBA8_SNORM:  return nvrhi::Format::RGBA8_SNORM;
    case GpuFormat::RGBA8_SRGB:   return nvrhi::Format::SRGBA8_UNORM;
    case GpuFormat::BGRA8_UNORM:  return nvrhi::Format::BGRA8_UNORM;
    case GpuFormat::BGRA8_SRGB:   return nvrhi::Format::SBGRA8_UNORM;
    case GpuFormat::R16_UINT:     return nvrhi::Format::R16_UINT;
    case GpuFormat::R16_FLOAT:    return nvrhi::Format::R16_FLOAT;
    case GpuFormat::RG16_FLOAT:   return nvrhi::Format::RG16_FLOAT;
    case GpuFormat::RGBA16_FLOAT: return nvrhi::Format::RGBA16_FLOAT;
    case GpuFormat::R32_UINT:     return nvrhi::Format::R32_UINT;
    case GpuFormat::R32_FLOAT:    return nvrhi::Format::R32_FLOAT;
    case GpuFormat::RG32_FLOAT:   return nvrhi::Format::RG32_FLOAT;
    case GpuFormat::RGB32_FLOAT:  return nvrhi::Format::RGB32_FLOAT;
    case GpuFormat::RGBA32_FLOAT: return nvrhi::Format::RGBA32_FLOAT;
    case GpuFormat::D16:          return nvrhi::Format::D16;
    case GpuFormat::D24S8:        return nvrhi::Format::D24S8;
    case GpuFormat::D32:          return nvrhi::Format::D32;
    case GpuFormat::D32S8:        return nvrhi::Format::D32S8;
    case GpuFormat::BC1_UNORM:    return nvrhi::Format::BC1_UNORM;
    case GpuFormat::BC1_SRGB:     return nvrhi::Format::BC1_UNORM_SRGB;
    case GpuFormat::BC3_UNORM:    return nvrhi::Format::BC3_UNORM;
    case GpuFormat::BC3_SRGB:     return nvrhi::Format::BC3_UNORM_SRGB;
    case GpuFormat::BC4_UNORM:    return nvrhi::Format::BC4_UNORM;
    case GpuFormat::BC5_UNORM:    return nvrhi::Format::BC5_UNORM;
    case GpuFormat::BC7_UNORM:    return nvrhi::Format::BC7_UNORM;
    case GpuFormat::BC7_SRGB:     return nvrhi::Format::BC7_UNORM_SRGB;
    default:                      return nvrhi::Format::UNKNOWN;
    }
}

// ShaderStage bit values match nvrhi::ShaderType exactly — safe to cast.
static nvrhi::ShaderType ToNvrhiShaderType(ShaderStage s)
{
    return static_cast<nvrhi::ShaderType>(static_cast<uint32_t>(s));
}

static nvrhi::PrimitiveType ToNvrhiPrimType(PrimitiveType p)
{
    switch (p)
    {
    case PrimitiveType::TriangleStrip: return nvrhi::PrimitiveType::TriangleStrip;
    case PrimitiveType::PointList:     return nvrhi::PrimitiveType::PointList;
    case PrimitiveType::LineList:      return nvrhi::PrimitiveType::LineList;
    case PrimitiveType::LineStrip:     return nvrhi::PrimitiveType::LineStrip;
    default:                           return nvrhi::PrimitiveType::TriangleList;
    }
}

static nvrhi::RasterCullMode ToNvrhiCullMode(CullMode c)
{
    switch (c)
    {
    case CullMode::None:  return nvrhi::RasterCullMode::None;
    case CullMode::Front: return nvrhi::RasterCullMode::Front;
    default:              return nvrhi::RasterCullMode::Back;
    }
}

static nvrhi::ComparisonFunc ToNvrhiCompFunc(ComparisonFunc f)
{
    switch (f)
    {
    case ComparisonFunc::Never:          return nvrhi::ComparisonFunc::Never;
    case ComparisonFunc::Less:           return nvrhi::ComparisonFunc::Less;
    case ComparisonFunc::Equal:          return nvrhi::ComparisonFunc::Equal;
    case ComparisonFunc::LessOrEqual:    return nvrhi::ComparisonFunc::LessOrEqual;
    case ComparisonFunc::Greater:        return nvrhi::ComparisonFunc::Greater;
    case ComparisonFunc::NotEqual:       return nvrhi::ComparisonFunc::NotEqual;
    case ComparisonFunc::GreaterOrEqual: return nvrhi::ComparisonFunc::GreaterOrEqual;
    default:                             return nvrhi::ComparisonFunc::Always;
    }
}

static nvrhi::StencilOp ToNvrhiStencilOp(StencilOp op)
{
    switch (op)
    {
    case StencilOp::Zero:     return nvrhi::StencilOp::Zero;
    case StencilOp::Replace:  return nvrhi::StencilOp::Replace;
    case StencilOp::IncrSat:  return nvrhi::StencilOp::IncrementAndClamp;
    case StencilOp::DecrSat:  return nvrhi::StencilOp::DecrementAndClamp;
    case StencilOp::Invert:   return nvrhi::StencilOp::Invert;
    case StencilOp::IncrWrap: return nvrhi::StencilOp::IncrementAndWrap;
    case StencilOp::DecrWrap: return nvrhi::StencilOp::DecrementAndWrap;
    default:                  return nvrhi::StencilOp::Keep;
    }
}

static nvrhi::BlendFactor ToNvrhiBlendFactor(BlendFactor f)
{
    switch (f)
    {
    case BlendFactor::One:             return nvrhi::BlendFactor::One;
    case BlendFactor::SrcColor:        return nvrhi::BlendFactor::SrcColor;
    case BlendFactor::InvSrcColor:     return nvrhi::BlendFactor::OneMinusSrcColor;
    case BlendFactor::SrcAlpha:        return nvrhi::BlendFactor::SrcAlpha;
    case BlendFactor::InvSrcAlpha:     return nvrhi::BlendFactor::OneMinusSrcAlpha;
    case BlendFactor::DstColor:        return nvrhi::BlendFactor::DstColor;
    case BlendFactor::InvDstColor:     return nvrhi::BlendFactor::OneMinusDstColor;
    case BlendFactor::DstAlpha:        return nvrhi::BlendFactor::DstAlpha;
    case BlendFactor::InvDstAlpha:     return nvrhi::BlendFactor::OneMinusDstAlpha;
    case BlendFactor::ConstantColor:   return nvrhi::BlendFactor::ConstantColor;
    case BlendFactor::InvConstantColor:return nvrhi::BlendFactor::OneMinusConstantColor;
    default:                           return nvrhi::BlendFactor::Zero;
    }
}

static nvrhi::BlendOp ToNvrhiBlendOp(BlendOp op)
{
    switch (op)
    {
    case BlendOp::Subtract:    return nvrhi::BlendOp::Subtract;
    case BlendOp::RevSubtract: return nvrhi::BlendOp::ReverseSubtract;
    case BlendOp::Min:         return nvrhi::BlendOp::Min;
    case BlendOp::Max:         return nvrhi::BlendOp::Max;
    default:                   return nvrhi::BlendOp::Add;
    }
}

static nvrhi::SamplerAddressMode ToNvrhiAddressMode(AddressMode m)
{
    switch (m)
    {
    case AddressMode::Mirror: return nvrhi::SamplerAddressMode::Mirror;
    case AddressMode::Clamp:  return nvrhi::SamplerAddressMode::Clamp;
    case AddressMode::Border: return nvrhi::SamplerAddressMode::Border;
    default:                  return nvrhi::SamplerAddressMode::Wrap;
    }
}

static nvrhi::TextureDimension ToNvrhiTexDim(TextureDimension d)
{
    switch (d)
    {
    case TextureDimension::Texture1D:     return nvrhi::TextureDimension::Texture1D;
    case TextureDimension::Texture3D:     return nvrhi::TextureDimension::Texture3D;
    case TextureDimension::Texture2DArray:return nvrhi::TextureDimension::Texture2DArray;
    case TextureDimension::TextureCube:   return nvrhi::TextureDimension::TextureCube;
    default:                              return nvrhi::TextureDimension::Texture2D;
    }
}

// ============================================================================
// NvrhiCommandContext — ICommandContext implementation
// ============================================================================

class NvrhiCommandContext : public ICommandContext
{
public:
    NvrhiCommandContext(NvrhiGpuDevice* device, nvrhi::CommandListHandle cmdList)
        : m_Device(device), m_CmdList(std::move(cmdList)) {}

    void Open()  override { m_CmdList->open();  ResetState(); }
    void Close() override { m_CmdList->close(); }

    // ---- Upload ----
    void WriteBuffer(GpuBuffer dst, const void* data,
                     size_t byteSize, size_t offset) override
    {
        m_CmdList->writeBuffer(m_Device->GetBuffer(dst.id), data, byteSize, offset);
    }

    void WriteTexture(GpuTexture dst, uint32_t arraySlice, uint32_t mipLevel,
                      const void* data, size_t rowPitch, size_t depthPitch) override
    {
        m_CmdList->writeTexture(m_Device->GetTexture(dst.id),
                                 arraySlice, mipLevel, data, rowPitch, depthPitch);
    }

    // ---- Copy ----
    void CopyBuffer(GpuBuffer dst, GpuBuffer src, size_t byteSize,
                    size_t dstOffset, size_t srcOffset) override
    {
        m_CmdList->copyBuffer(m_Device->GetBuffer(dst.id), dstOffset,
                               m_Device->GetBuffer(src.id), srcOffset, byteSize);
    }

    void CopyTexture(GpuTexture dst, GpuTexture src) override
    {
        nvrhi::TextureSlice slice;
        m_CmdList->copyTexture(m_Device->GetTexture(dst.id), slice,
                                m_Device->GetTexture(src.id), slice);
    }

    // ---- Explicit clears ----
    void ClearColor(GpuTexture texture, const ClearValue& c) override
    {
        m_CmdList->clearTextureFloat(m_Device->GetTexture(texture.id),
                                      nvrhi::AllSubresources,
                                      nvrhi::Color(c.r, c.g, c.b, c.a));
    }

    void ClearDepth(GpuTexture texture, float depth) override
    {
        m_CmdList->clearDepthStencilTexture(m_Device->GetTexture(texture.id),
                                             nvrhi::AllSubresources,
                                             true, depth, false, 0);
    }

    void ClearDepthStencil(GpuTexture texture, float depth, uint8_t stencil) override
    {
        m_CmdList->clearDepthStencilTexture(m_Device->GetTexture(texture.id),
                                             nvrhi::AllSubresources,
                                             true, depth, true, stencil);
    }

    // ---- Render pass ----
    // Clears are deferred until the first DrawFlush so setGraphicsState
    // has already bound the framebuffer (required for D3D12/NVRHI).
    void BeginRenderPass(const RenderPassDesc& desc) override
    {
        m_GfxState.framebuffer = m_Device->GetFramebuffer(desc.framebuffer.id);
        m_PendingFbHandle      = desc.framebuffer;
        m_PendingRpDesc        = desc;
        m_HasPendingClears     = desc.clearColor || desc.clearDepth || desc.clearStencil;
        m_GfxStateDirty        = true;
    }

    void EndRenderPass() override
    {
        // No explicit end call needed for NVRHI's non-render-pass API.
    }

    // ---- Graphics state ----
    void SetGraphicsPipeline(GpuGraphicsPipeline pipeline) override
    {
        m_GfxState.pipeline = m_Device->GetGraphicsPipeline(pipeline.id);
        m_GfxStateDirty = true;
    }

    void SetViewport(const Viewport& vp) override
    {
        nvrhi::Viewport nvVp(vp.x, vp.x + vp.width,
                             vp.y, vp.y + vp.height,
                             vp.minDepth, vp.maxDepth);
        auto& vps = m_GfxState.viewport.viewports;
        if (vps.empty()) vps.push_back(nvVp); else vps[0] = nvVp;

        // Auto scissor — overridden if SetScissor is called explicitly.
        if (!m_ExplicitScissor)
        {
            nvrhi::Rect nvSr((int)vp.x, (int)(vp.x + vp.width),
                             (int)vp.y, (int)(vp.y + vp.height));
            auto& srs = m_GfxState.viewport.scissorRects;
            if (srs.empty()) srs.push_back(nvSr); else srs[0] = nvSr;
        }
        m_GfxStateDirty = true;
    }

    void SetScissor(const ScissorRect& rect) override
    {
        nvrhi::Rect nvSr(rect.x, rect.x + rect.width,
                         rect.y, rect.y + rect.height);
        auto& srs = m_GfxState.viewport.scissorRects;
        if (srs.empty()) srs.push_back(nvSr); else srs[0] = nvSr;
        m_ExplicitScissor = true;
        m_GfxStateDirty   = true;
    }

    void SetVertexBuffer(uint32_t slot, GpuBuffer buffer, uint64_t byteOffset) override
    {
        while (m_GfxState.vertexBuffers.size() <= slot)
            m_GfxState.vertexBuffers.push_back({});
        m_GfxState.vertexBuffers[slot] =
            nvrhi::VertexBufferBinding{ m_Device->GetBuffer(buffer.id), slot, byteOffset };
        m_GfxStateDirty = true;
    }

    void SetIndexBuffer(GpuBuffer buffer, GpuFormat indexFormat,
                        uint64_t byteOffset) override
    {
        m_GfxState.indexBuffer =
            nvrhi::IndexBufferBinding{ m_Device->GetBuffer(buffer.id),
                                       ToNvrhiFormat(indexFormat), static_cast<uint32_t>(byteOffset) };
        m_GfxStateDirty = true;
    }

    void SetBindingSet(GpuBindingSet set, uint32_t slot) override
    {
        while (m_GfxState.bindings.size() <= slot)
            m_GfxState.bindings.push_back(nullptr);
        m_GfxState.bindings[slot] = m_Device->GetBindingSet(set.id);
        m_GfxStateDirty = true;
    }

    // ---- Draw ----
    void Draw(const DrawArgs& args) override
    {
        FlushGraphicsState();
        nvrhi::DrawArguments a;
        a.vertexCount         = args.vertexCount;
        a.instanceCount       = args.instanceCount;
        a.startVertexLocation = args.startVertex;
        a.startInstanceLocation = args.startInstance;
        m_CmdList->draw(a);
    }

    void DrawIndexed(const DrawIndexedArgs& args) override
    {
        FlushGraphicsState();
        nvrhi::DrawArguments a;
        a.vertexCount           = args.indexCount;
        a.instanceCount         = args.instanceCount;
        a.startIndexLocation    = args.startIndex;
        a.startVertexLocation   = args.baseVertex;
        a.startInstanceLocation = args.startInstance;
        m_CmdList->drawIndexed(a);
    }

    void DrawIndirect(GpuBuffer argsBuffer, uint64_t byteOffset) override
    {
        FlushGraphicsState();
        m_CmdList->drawIndirect(byteOffset, 1); // TODO: fix this
    }

    void DrawIndexedIndirect(GpuBuffer argsBuffer, uint64_t byteOffset) override
    {
        FlushGraphicsState();
        m_CmdList->drawIndexedIndirect(byteOffset, 1); // TODO: fix this
    }

    // ---- Compute ----
    void SetComputePipeline(GpuComputePipeline pipeline) override
    {
        m_CmpState.pipeline = m_Device->GetComputePipeline(pipeline.id);
        m_CmpStateDirty = true;
    }

    void SetComputeBindingSet(GpuBindingSet set, uint32_t slot) override
    {
        while (m_CmpState.bindings.size() <= slot)
            m_CmpState.bindings.push_back(nullptr);
        m_CmpState.bindings[slot] = m_Device->GetBindingSet(set.id);
        m_CmpStateDirty = true;
    }

    void Dispatch(const DispatchArgs& args) override
    {
        FlushComputeState();
        m_CmdList->dispatch(args.groupX, args.groupY, args.groupZ);
    }

    void DispatchIndirect(GpuBuffer argsBuffer, uint64_t byteOffset) override
    {
        FlushComputeState();
        m_CmdList->dispatchIndirect(byteOffset);
    }

    // ---- Debug markers ----
    void BeginMarker(const char* name) override { m_CmdList->beginMarker(name); }
    void EndMarker()                   override { m_CmdList->endMarker(); }

    nvrhi::ICommandList* GetNative() const { return m_CmdList.Get(); }

private:
    void ResetState()
    {
        m_GfxState       = {};
        m_CmpState       = {};
        m_GfxStateDirty  = false;
        m_CmpStateDirty  = false;
        m_ExplicitScissor = false;
        m_HasPendingClears = false;
        m_PendingFbHandle  = {};
        m_PendingRpDesc    = {};
    }

    void FlushGraphicsState()
    {
        if (!m_GfxStateDirty) return;
        if (!m_GfxState.pipeline || !m_GfxState.framebuffer) return;
        m_CmdList->setGraphicsState(m_GfxState);
        m_GfxStateDirty = false;

        if (m_HasPendingClears)
        {
            // Apply deferred clears now that the framebuffer is bound (required
            // for D3D12). On Vulkan, clearTextureFloat issues vkCmdClearColorImage
            // which must be outside a render pass, so NVRHI ends the active pass.
            // We call setGraphicsState again afterwards to restart it before draw.
            ApplyPendingClears();
            m_HasPendingClears = false;
            m_CmdList->setGraphicsState(m_GfxState);
        }
    }

    void FlushComputeState()
    {
        if (!m_CmpStateDirty) return;
        if (!m_CmpState.pipeline) return;
        m_CmdList->setComputeState(m_CmpState);
        m_CmpStateDirty = false;
    }

    void ApplyPendingClears()
    {
        const auto& fbDesc = m_Device->GetFramebufferDesc(m_PendingFbHandle.id);

        if (m_PendingRpDesc.clearColor)
        {
            auto& cv = m_PendingRpDesc.colorValue;
            for (const auto& attach : fbDesc.colorAttachments)
            {
                if (!attach.texture.IsValid()) continue;
                m_CmdList->clearTextureFloat(
                    m_Device->GetTexture(attach.texture.id),
                    nvrhi::AllSubresources,
                    nvrhi::Color(cv.r, cv.g, cv.b, cv.a));
            }
        }

        if ((m_PendingRpDesc.clearDepth || m_PendingRpDesc.clearStencil)
            && fbDesc.depthAttachment.texture.IsValid())
        {
            m_CmdList->clearDepthStencilTexture(
                m_Device->GetTexture(fbDesc.depthAttachment.texture.id),
                nvrhi::AllSubresources,
                m_PendingRpDesc.clearDepth,   m_PendingRpDesc.depthValue,
                m_PendingRpDesc.clearStencil, m_PendingRpDesc.stencilValue);
        }
    }

    NvrhiGpuDevice*      m_Device        = nullptr;
    nvrhi::CommandListHandle m_CmdList;

    nvrhi::GraphicsState m_GfxState;
    bool                 m_GfxStateDirty  = false;
    bool                 m_ExplicitScissor = false;

    nvrhi::ComputeState  m_CmpState;
    bool                 m_CmpStateDirty  = false;

    bool           m_HasPendingClears = false;
    GpuFramebuffer m_PendingFbHandle;
    RenderPassDesc m_PendingRpDesc;
};

// ============================================================================
// NvrhiGpuDevice — resource creation
// ============================================================================

NvrhiGpuDevice::NvrhiGpuDevice(nvrhi::IDevice* device)
    : m_Device(device)
{}

// ---- Buffers ----

GpuBuffer NvrhiGpuDevice::CreateBuffer(const BufferDesc& desc)
{
    nvrhi::BufferDesc bd;
    bd.byteSize       = desc.byteSize;
    bd.debugName      = desc.debugName ? desc.debugName : "";
    bd.isConstantBuffer   = (desc.usage & BufferUsage::Constant) != 0;
    bd.isVertexBuffer     = (desc.usage & BufferUsage::Vertex)   != 0;
    bd.isIndexBuffer      = (desc.usage & BufferUsage::Index)    != 0;
    bd.isDrawIndirectArgs = (desc.usage & BufferUsage::Indirect)  != 0;
    if (desc.usage & BufferUsage::Storage)
        bd.canHaveUAVs = true;
    if (desc.usage & BufferUsage::Staging)
    {
        bd.cpuAccess  = nvrhi::CpuAccessMode::Write;
        bd.isVertexBuffer = false; // staging buffers are not VBs
    }
    bd.keepInitialState = true;

    // Set initial resource state
    if (desc.usage & BufferUsage::Constant)
        bd.initialState = nvrhi::ResourceStates::ConstantBuffer;
    else if (desc.usage & BufferUsage::Vertex)
        bd.initialState = nvrhi::ResourceStates::VertexBuffer;
    else if (desc.usage & BufferUsage::Index)
        bd.initialState = nvrhi::ResourceStates::IndexBuffer;

    return { m_Buffers.Add(m_Device->createBuffer(bd)) };
}

void NvrhiGpuDevice::DestroyBuffer(GpuBuffer handle)
{
    m_Buffers.Release(handle.id);
}

// ---- Textures ----

GpuTexture NvrhiGpuDevice::CreateTexture(const TextureDesc& desc)
{
    nvrhi::TextureDesc td;
    td.width       = desc.width;
    td.height      = desc.height;
    td.depth       = desc.depth;
    td.mipLevels   = desc.mipLevels;
    td.sampleCount = desc.sampleCount;
    td.format      = ToNvrhiFormat(desc.format);
    td.dimension   = ToNvrhiTexDim(desc.dimension);
    td.debugName   = desc.debugName ? desc.debugName : "";
    td.isRenderTarget   = (desc.usage & TextureUsage::RenderTarget) != 0;
    td.isUAV            = (desc.usage & TextureUsage::Storage)      != 0;
    td.keepInitialState = true;

    if (desc.usage & TextureUsage::DepthStencil)
    {
        td.isRenderTarget  = true;
        td.initialState    = nvrhi::ResourceStates::DepthWrite;
    }
    else if (desc.usage & TextureUsage::RenderTarget)
    {
        td.initialState = nvrhi::ResourceStates::RenderTarget;
    }
    else
    {
        td.initialState = nvrhi::ResourceStates::ShaderResource;
    }

    return { m_Textures.Add(m_Device->createTexture(td)) };
}

void NvrhiGpuDevice::DestroyTexture(GpuTexture handle)
{
    m_Textures.Release(handle.id);
}

// ---- Samplers ----

GpuSampler NvrhiGpuDevice::CreateSampler(const SamplerDesc& desc)
{
    nvrhi::SamplerDesc sd;
    sd.addressU    = ToNvrhiAddressMode(desc.addressU);
    sd.addressV    = ToNvrhiAddressMode(desc.addressV);
    sd.addressW    = ToNvrhiAddressMode(desc.addressW);
    sd.mipBias     = desc.mipLODBias;
    sd.maxAnisotropy = (float)desc.maxAnisotropy;
    //sd.comparisonFunc = ToNvrhiCompFunc(desc.comparison); // TODO: fix this
    //sd.minLod      = desc.minLOD; // TODO: fix this
    //sd.maxLod      = desc.maxLOD; // TODO: fix this

    bool linear = (desc.minFilter == Filter::Linear || desc.magFilter == Filter::Linear);
    bool aniso  = (desc.minFilter == Filter::Anisotropic);
    sd.minFilter = sd.magFilter = sd.mipFilter = linear || aniso;
    sd.reductionType = aniso ? nvrhi::SamplerReductionType::Standard
                              : nvrhi::SamplerReductionType::Standard;

    return { m_Samplers.Add(m_Device->createSampler(sd)) };
}

void NvrhiGpuDevice::DestroySampler(GpuSampler handle)
{
    m_Samplers.Release(handle.id);
}

// ---- Shaders ----

GpuShader NvrhiGpuDevice::CreateShader(const ShaderDesc& desc)
{
    nvrhi::ShaderDesc sd;
    sd.shaderType = ToNvrhiShaderType(desc.stage);
    sd.entryName  = desc.entryPoint ? desc.entryPoint : "main";
    sd.debugName  = desc.debugName  ? desc.debugName  : "";

    return { m_Shaders.Add(
        m_Device->createShader(sd, desc.bytecode, desc.byteSize)) };
}

void NvrhiGpuDevice::DestroyShader(GpuShader handle)
{
    m_Shaders.Release(handle.id);
}

// ---- Input layout ----

GpuInputLayout NvrhiGpuDevice::CreateInputLayout(
    const std::vector<VertexAttributeDesc>& attribs,
    GpuShader vertexShader)
{
    std::vector<nvrhi::VertexAttributeDesc> nvAttribs;
    nvAttribs.reserve(attribs.size());
    for (const auto& a : attribs)
    {
        nvrhi::VertexAttributeDesc va;
        va.name          = a.name ? a.name : "";
        va.format        = ToNvrhiFormat(a.format);
        va.bufferIndex   = a.bufferIndex;
        va.offset        = a.offset;
        va.elementStride = a.stride;
        nvAttribs.push_back(va);
    }
    auto nvVs = m_Shaders.Get(vertexShader.id);
    return { m_InputLayouts.Add(
        m_Device->createInputLayout(nvAttribs.data(),
                                     (uint32_t)nvAttribs.size(), nvVs)) };
}

void NvrhiGpuDevice::DestroyInputLayout(GpuInputLayout handle)
{
    m_InputLayouts.Release(handle.id);
}

// ---- Binding layout ----

GpuBindingLayout NvrhiGpuDevice::CreateBindingLayout(const BindingLayoutDesc& desc)
{
    nvrhi::BindingLayoutDesc bld;

    // Aggregate visibility from all items.
    uint32_t vis = 0;
    for (const auto& item : desc.items)
        vis |= (uint32_t)item.stage;
    bld.visibility = static_cast<nvrhi::ShaderType>(vis);
    bld.bindingOffsets.setConstantBufferOffset(0); // match GLSL binding 0

    for (const auto& item : desc.items)
    {
        switch (item.type)
        {
        case BindingType::ConstantBuffer:
            bld.bindings.push_back(nvrhi::BindingLayoutItem::ConstantBuffer(item.slot));
            break;
        case BindingType::Texture:
            bld.bindings.push_back(nvrhi::BindingLayoutItem::Texture_SRV(item.slot));
            break;
        case BindingType::Sampler:
            bld.bindings.push_back(nvrhi::BindingLayoutItem::Sampler(item.slot));
            break;
        case BindingType::StorageBuffer:
            bld.bindings.push_back(nvrhi::BindingLayoutItem::RawBuffer_UAV(item.slot));
            break;
        case BindingType::StorageTexture:
            bld.bindings.push_back(nvrhi::BindingLayoutItem::Texture_UAV(item.slot));
            break;
        }
    }

    return { m_BindingLayouts.Add(m_Device->createBindingLayout(bld)) };
}

void NvrhiGpuDevice::DestroyBindingLayout(GpuBindingLayout handle)
{
    m_BindingLayouts.Release(handle.id);
}

// ---- Binding set ----

GpuBindingSet NvrhiGpuDevice::CreateBindingSet(const BindingSetDesc& desc,
                                                 GpuBindingLayout layout)
{
    nvrhi::BindingSetDesc bsd;
    for (const auto& item : desc.items)
    {
        switch (item.type)
        {
        case BindingType::ConstantBuffer:
            bsd.bindings.push_back(
                nvrhi::BindingSetItem::ConstantBuffer(item.slot,
                    m_Buffers.Get(item.buffer.id)));
            break;
        case BindingType::Texture:
            bsd.bindings.push_back(
                nvrhi::BindingSetItem::Texture_SRV(item.slot,
                    m_Textures.Get(item.texture.id)));
            break;
        case BindingType::Sampler:
            bsd.bindings.push_back(
                nvrhi::BindingSetItem::Sampler(item.slot,
                    m_Samplers.Get(item.sampler.id)));
            break;
        case BindingType::StorageBuffer:
            bsd.bindings.push_back(
                nvrhi::BindingSetItem::RawBuffer_UAV(item.slot,
                    m_Buffers.Get(item.buffer.id)));
            break;
        case BindingType::StorageTexture:
            bsd.bindings.push_back(
                nvrhi::BindingSetItem::Texture_UAV(item.slot,
                    m_Textures.Get(item.texture.id)));
            break;
        }
    }

    auto nvLayout = m_BindingLayouts.Get(layout.id);
    return { m_BindingSets.Add(m_Device->createBindingSet(bsd, nvLayout)) };
}

void NvrhiGpuDevice::DestroyBindingSet(GpuBindingSet handle)
{
    m_BindingSets.Release(handle.id);
}

// ---- Framebuffer ----

GpuFramebuffer NvrhiGpuDevice::CreateFramebuffer(const FramebufferDesc& desc)
{
    nvrhi::FramebufferDesc nvDesc;
    for (const auto& attach : desc.colorAttachments)
    {
        nvrhi::FramebufferAttachment a;
        a.texture    = m_Textures.Get(attach.texture.id);
        a.setMipLevel(attach.mipLevel);
        a.setArraySlice(attach.arraySlice);
        nvDesc.addColorAttachment(a);
    }
    if (desc.depthAttachment.texture.IsValid())
    {
        nvrhi::FramebufferAttachment a;
        a.texture    = m_Textures.Get(desc.depthAttachment.texture.id);
        a.setMipLevel(desc.depthAttachment.mipLevel);
        a.setArraySlice(desc.depthAttachment.arraySlice);
        nvDesc.setDepthAttachment(a);
    }

    uint32_t nvFbId = m_Framebuffers.Add(m_Device->createFramebuffer(nvDesc));
    // Slot IDs must stay in sync between both pools.
    uint32_t descId = m_FramebufferDescs.Add(desc);
    (void)descId;

    return { nvFbId };
}

void NvrhiGpuDevice::DestroyFramebuffer(GpuFramebuffer handle)
{
    m_Framebuffers.Release(handle.id);
    m_FramebufferDescs.Release(handle.id);
}

std::pair<uint32_t, uint32_t> NvrhiGpuDevice::GetFramebufferSize(
    GpuFramebuffer handle) const
{
    auto& fbDesc = m_FramebufferDescs.Get(handle.id);
    if (!fbDesc.colorAttachments.empty())
    {
        auto nvTex = m_Textures.Get(fbDesc.colorAttachments[0].texture.id);
        if (nvTex)
        {
            const auto& td = nvTex->getDesc();
            return { td.width, td.height };
        }
    }
    return { 0, 0 };
}

// ---- Pipelines ----

static nvrhi::RenderState ToNvrhiRenderState(const GraphicsPipelineDesc& desc)
{
    nvrhi::RenderState rs;

    // Rasterizer
    rs.rasterState.fillMode = desc.rasterizer.fillMode == FillMode::Wireframe
        ? nvrhi::RasterFillMode::Wireframe : nvrhi::RasterFillMode::Fill;
    rs.rasterState.cullMode              = ToNvrhiCullMode(desc.rasterizer.cullMode);
    rs.rasterState.frontCounterClockwise = desc.rasterizer.frontCCW;
    rs.rasterState.depthBias             = desc.rasterizer.depthBias;
    rs.rasterState.slopeScaledDepthBias  = desc.rasterizer.slopeScaledDepthBias;
    rs.rasterState.depthClipEnable       = desc.rasterizer.depthClipEnable;

    // Depth-stencil
    auto& ds = rs.depthStencilState;
    ds.depthTestEnable  = desc.depthStencil.depthTestEnable;
    ds.depthWriteEnable = desc.depthStencil.depthWriteEnable;
    ds.depthFunc        = ToNvrhiCompFunc(desc.depthStencil.depthFunc);
    ds.stencilEnable    = desc.depthStencil.stencilEnable;
    ds.stencilReadMask  = desc.depthStencil.stencilReadMask;
    ds.stencilWriteMask = desc.depthStencil.stencilWriteMask;

    auto cvtStencilOp = [](const StencilOpDesc& s) {
        nvrhi::DepthStencilState::StencilOpDesc o;
        o.failOp      = ToNvrhiStencilOp(s.failOp);
        o.depthFailOp = ToNvrhiStencilOp(s.depthFailOp);
        o.passOp      = ToNvrhiStencilOp(s.passOp);
        o.stencilFunc = ToNvrhiCompFunc(s.func);
        return o;
    };
    ds.frontFaceStencil = cvtStencilOp(desc.depthStencil.frontFace);
    ds.backFaceStencil  = cvtStencilOp(desc.depthStencil.backFace);

    // Blend
    for (int t = 0; t < 8; ++t)
    {
        const auto& src = desc.blend.renderTargets[t];
        auto&       dst = rs.blendState.targets[t];
        dst.blendEnable    = src.blendEnable;
        dst.srcBlend       = ToNvrhiBlendFactor(src.srcBlend);
        dst.destBlend      = ToNvrhiBlendFactor(src.dstBlend);
        dst.blendOp        = ToNvrhiBlendOp(src.blendOp);
        dst.srcBlendAlpha  = ToNvrhiBlendFactor(src.srcBlendAlpha);
        dst.destBlendAlpha = ToNvrhiBlendFactor(src.dstBlendAlpha);
        dst.blendOpAlpha   = ToNvrhiBlendOp(src.blendOpAlpha);
        dst.colorWriteMask = static_cast<nvrhi::ColorMask>(src.writeMask);
    }

    return rs;
}

GpuGraphicsPipeline NvrhiGpuDevice::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc, GpuFramebuffer framebuffer)
{
    nvrhi::GraphicsPipelineDesc pd;
    pd.VS          = m_Shaders.Get(desc.vs.id);
    pd.PS          = m_Shaders.Get(desc.ps.id);
    if (desc.hs.IsValid()) pd.HS = m_Shaders.Get(desc.hs.id);
    if (desc.ds.IsValid()) pd.DS = m_Shaders.Get(desc.ds.id);
    if (desc.gs.IsValid()) pd.GS = m_Shaders.Get(desc.gs.id);
    pd.inputLayout = m_InputLayouts.Get(desc.inputLayout.id);
    pd.primType    = ToNvrhiPrimType(desc.primType);
    pd.renderState = ToNvrhiRenderState(desc);

    for (const auto& bl : desc.bindingLayouts)
        pd.bindingLayouts.push_back(m_BindingLayouts.Get(bl.id));

    auto nvFb = m_Framebuffers.Get(framebuffer.id);
    return { m_GraphicsPipelines.Add(m_Device->createGraphicsPipeline(pd, nvFb)) };
}

void NvrhiGpuDevice::DestroyGraphicsPipeline(GpuGraphicsPipeline handle)
{
    m_GraphicsPipelines.Release(handle.id);
}

GpuComputePipeline NvrhiGpuDevice::CreateComputePipeline(
    const ComputePipelineDesc& desc)
{
    nvrhi::ComputePipelineDesc pd;
    pd.CS = m_Shaders.Get(desc.cs.id);
    for (const auto& bl : desc.bindingLayouts)
        pd.bindingLayouts.push_back(m_BindingLayouts.Get(bl.id));

    return { m_ComputePipelines.Add(m_Device->createComputePipeline(pd)) };
}

void NvrhiGpuDevice::DestroyComputePipeline(GpuComputePipeline handle)
{
    m_ComputePipelines.Release(handle.id);
}

// ---- Command context ----

std::unique_ptr<ICommandContext> NvrhiGpuDevice::CreateCommandContext()
{
    auto cmdList = m_Device->createCommandList();
    return std::make_unique<NvrhiCommandContext>(this, std::move(cmdList));
}

void NvrhiGpuDevice::ExecuteCommandContext(ICommandContext& ctx)
{
    auto* nvrhi_ctx = static_cast<NvrhiCommandContext*>(&ctx);
    m_Device->executeCommandList(nvrhi_ctx->GetNative());
}

// ---- Device management ----

void NvrhiGpuDevice::WaitForIdle()
{
    m_Device->waitForIdle();
}

void NvrhiGpuDevice::RunGarbageCollection()
{
    m_Device->runGarbageCollection();
}

// ---- Back buffer registration (called by IRenderDevice backends) ----

void NvrhiGpuDevice::RegisterBackBuffers(
    const std::vector<nvrhi::TextureHandle>& nativeBuffers)
{
    m_BackBufferTextures.clear();
    m_BackBufferTextures.reserve(nativeBuffers.size());
    for (const auto& nvTex : nativeBuffers)
        m_BackBufferTextures.push_back({ m_Textures.Add(nvTex) });
}

void NvrhiGpuDevice::ClearBackBuffers()
{
    for (auto& t : m_BackBufferTextures)
        m_Textures.Release(t.id);
    m_BackBufferTextures.clear();
}

const std::vector<GpuTexture>& NvrhiGpuDevice::GetBackBufferTextures() const
{
    return m_BackBufferTextures;
}
