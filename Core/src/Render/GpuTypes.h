#ifndef GPU_TYPES_H
#define GPU_TYPES_H

// Backend-agnostic GPU types: opaque handles, enums, and descriptor structs.
// No NVRHI, Vulkan, or D3D12 headers are included here.

#include <cstdint>
#include <cstddef>
#include <vector>

// ---------------------------------------------------------------------------
// Opaque resource handles
// All are typed wrappers around a uint32_t slot ID.
// ID == 0 is always the null/invalid sentinel.
// ---------------------------------------------------------------------------

template<typename Tag>
struct GpuHandle
{
    uint32_t id = 0;
    bool     IsValid()                  const { return id != 0; }
    bool     operator==(GpuHandle o)    const { return id == o.id; }
    bool     operator!=(GpuHandle o)    const { return id != o.id; }
};

struct GpuBufferTag          {};
struct GpuTextureTag         {};
struct GpuSamplerTag         {};
struct GpuShaderTag          {};
struct GpuInputLayoutTag     {};
struct GpuBindingLayoutTag   {};
struct GpuBindingSetTag      {};
struct GpuGraphicsPipelineTag{};
struct GpuComputePipelineTag {};
struct GpuFramebufferTag     {};

using GpuBuffer           = GpuHandle<GpuBufferTag>;
using GpuTexture          = GpuHandle<GpuTextureTag>;
using GpuSampler          = GpuHandle<GpuSamplerTag>;
using GpuShader           = GpuHandle<GpuShaderTag>;
using GpuInputLayout      = GpuHandle<GpuInputLayoutTag>;
using GpuBindingLayout    = GpuHandle<GpuBindingLayoutTag>;
using GpuBindingSet       = GpuHandle<GpuBindingSetTag>;
using GpuGraphicsPipeline = GpuHandle<GpuGraphicsPipelineTag>;
using GpuComputePipeline  = GpuHandle<GpuComputePipelineTag>;
using GpuFramebuffer      = GpuHandle<GpuFramebufferTag>;

// ---------------------------------------------------------------------------
// Format
// ---------------------------------------------------------------------------
enum class GpuFormat : uint32_t
{
    Unknown = 0,
    // 8-bit
    R8_UNORM,
    RGBA8_UNORM, RGBA8_SNORM, RGBA8_SRGB,
    BGRA8_UNORM, BGRA8_SRGB,
    // 16-bit
    R16_UINT, R16_FLOAT,
    RG16_FLOAT, RGBA16_FLOAT,
    // 32-bit
    R32_UINT, R32_FLOAT,
    RG32_FLOAT, RGB32_FLOAT, RGBA32_FLOAT,
    // Depth / stencil
    D16, D24S8, D32, D32S8,
    // Block-compressed
    BC1_UNORM, BC1_SRGB,
    BC3_UNORM, BC3_SRGB,
    BC4_UNORM, BC5_UNORM,
    BC7_UNORM, BC7_SRGB,
};

// ---------------------------------------------------------------------------
// Shader stage flags (bit values intentionally mirror nvrhi::ShaderType)
// ---------------------------------------------------------------------------
enum class ShaderStage : uint32_t
{
    None        = 0x00,
    Vertex      = 0x01,
    Hull        = 0x02,
    Domain      = 0x04,
    Geometry    = 0x08,
    Pixel       = 0x10,
    Compute     = 0x20,
    AllGraphics = 0x1F,
    All         = 0xFF,
};
inline ShaderStage operator|(ShaderStage a, ShaderStage b) { return (ShaderStage)((uint32_t)a | (uint32_t)b); }
inline uint32_t    operator&(ShaderStage a, ShaderStage b) { return  (uint32_t)a & (uint32_t)b; }

// ---------------------------------------------------------------------------
// Buffer usage flags
// ---------------------------------------------------------------------------
enum class BufferUsage : uint32_t
{
    None     = 0,
    Vertex   = 1 << 0,
    Index    = 1 << 1,
    Constant = 1 << 2,
    Storage  = 1 << 3,
    Indirect = 1 << 4,
    Staging  = 1 << 5,
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b) { return (BufferUsage)((uint32_t)a | (uint32_t)b); }
inline uint32_t    operator&(BufferUsage a, BufferUsage b) { return  (uint32_t)a & (uint32_t)b; }

// ---------------------------------------------------------------------------
// Texture usage flags
// ---------------------------------------------------------------------------
enum class TextureUsage : uint32_t
{
    None           = 0,
    ShaderResource = 1 << 0,
    RenderTarget   = 1 << 1,
    DepthStencil   = 1 << 2,
    Storage        = 1 << 3,
};
inline TextureUsage operator|(TextureUsage a, TextureUsage b) { return (TextureUsage)((uint32_t)a | (uint32_t)b); }
inline uint32_t     operator&(TextureUsage a, TextureUsage b) { return  (uint32_t)a & (uint32_t)b; }

// ---------------------------------------------------------------------------
// Resource layout — used for barrier transitions
// ---------------------------------------------------------------------------
enum class ResourceLayout : uint8_t
{
    Undefined,
    RenderTarget,
    DepthWrite,
    DepthRead,
    ShaderResource,
    UnorderedAccess,
    CopySource,
    CopyDest,
    Present,
};

// ---------------------------------------------------------------------------
// Primitive topology, rasterizer, depth-stencil, blend enums
// ---------------------------------------------------------------------------
enum class PrimitiveType  { TriangleList, TriangleStrip, PointList, LineList, LineStrip };
enum class FillMode       { Solid, Wireframe };
enum class CullMode       { None, Front, Back };
enum class ComparisonFunc { Never, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always };
enum class StencilOp      { Keep, Zero, Replace, IncrSat, DecrSat, Invert, IncrWrap, DecrWrap };
enum class BlendFactor    { Zero, One, SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha,
                            DstColor, InvDstColor, DstAlpha, InvDstAlpha,
                            ConstantColor, InvConstantColor };
enum class BlendOp        { Add, Subtract, RevSubtract, Min, Max };

// ---------------------------------------------------------------------------
// Sampler enums
// ---------------------------------------------------------------------------
enum class Filter      { Point, Linear, Anisotropic };
enum class AddressMode { Wrap, Mirror, Clamp, Border };

// ---------------------------------------------------------------------------
// Viewport and scissor
// ---------------------------------------------------------------------------
struct Viewport
{
    float x = 0.f, y = 0.f;
    float width = 0.f, height = 0.f;
    float minDepth = 0.f, maxDepth = 1.f;
};

struct ScissorRect
{
    int x = 0, y = 0, width = 0, height = 0;
};

// ---------------------------------------------------------------------------
// Clear colour
// ---------------------------------------------------------------------------
struct ClearValue
{
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
};

// ---------------------------------------------------------------------------
// Buffer descriptor
// ---------------------------------------------------------------------------
struct BufferDesc
{
    uint64_t    byteSize   = 0;
    BufferUsage usage      = BufferUsage::None;
    bool        cpuVisible = false;
    const char* debugName  = nullptr;
};

// ---------------------------------------------------------------------------
// Texture descriptor
// ---------------------------------------------------------------------------
enum class TextureDimension { Texture1D, Texture2D, Texture3D, Texture2DArray, TextureCube };

struct TextureDesc
{
    uint32_t         width              = 1;
    uint32_t         height             = 1;
    uint32_t         depth              = 1;
    uint32_t         mipLevels          = 1;
    uint32_t         sampleCount        = 1;
    GpuFormat        format             = GpuFormat::RGBA8_UNORM;
    TextureDimension dimension          = TextureDimension::Texture2D;
    TextureUsage     usage              = TextureUsage::ShaderResource;
    const char*      debugName          = nullptr;
    // Optimized clear value — must match what you pass to BeginRenderPass.
    // D3D12 warns if these differ. Set for any RenderTarget or DepthStencil texture.
    ClearValue       optimizedClearColor   = {};        // used when usage = RenderTarget
    float            optimizedClearDepth   = 1.0f;      // used when usage = DepthStencil
    uint8_t          optimizedClearStencil = 0;
};

// ---------------------------------------------------------------------------
// Sampler descriptor
// ---------------------------------------------------------------------------
struct SamplerDesc
{
    Filter         minFilter     = Filter::Linear;
    Filter         magFilter     = Filter::Linear;
    Filter         mipFilter     = Filter::Linear;
    AddressMode    addressU      = AddressMode::Wrap;
    AddressMode    addressV      = AddressMode::Wrap;
    AddressMode    addressW      = AddressMode::Wrap;
    float          mipLODBias    = 0.f;
    uint32_t       maxAnisotropy = 1;
    ComparisonFunc comparison    = ComparisonFunc::Always;
    float          borderColor[4]= { 0.f, 0.f, 0.f, 0.f };
    float          minLOD        = 0.f;
    float          maxLOD        = 1000.f;
};

// ---------------------------------------------------------------------------
// Shader descriptor
// ---------------------------------------------------------------------------
struct ShaderDesc
{
    ShaderStage  stage      = ShaderStage::None;
    const void*  bytecode   = nullptr;
    size_t       byteSize   = 0;
    const char*  entryPoint = "main";
    const char*  debugName  = nullptr;
};

// ---------------------------------------------------------------------------
// Vertex input layout
// ---------------------------------------------------------------------------
struct VertexAttributeDesc
{
    const char* name        = nullptr;
    GpuFormat   format      = GpuFormat::Unknown;
    uint32_t    bufferIndex = 0;
    uint32_t    offset      = 0;
    uint32_t    stride      = 0;
};

// ---------------------------------------------------------------------------
// Binding layout
// ---------------------------------------------------------------------------
enum class BindingType
{
    ConstantBuffer,
    Texture,
    Sampler,
    StorageBuffer,
    StorageTexture,
};

struct BindingLayoutItem
{
    BindingType type  = BindingType::ConstantBuffer;
    ShaderStage stage = ShaderStage::AllGraphics;
    uint32_t    slot  = 0;

    static BindingLayoutItem ConstantBuffer(uint32_t slot, ShaderStage stage = ShaderStage::AllGraphics)
        { return { BindingType::ConstantBuffer, stage, slot }; }
    static BindingLayoutItem Texture(uint32_t slot, ShaderStage stage = ShaderStage::AllGraphics)
        { return { BindingType::Texture, stage, slot }; }
    static BindingLayoutItem Sampler(uint32_t slot, ShaderStage stage = ShaderStage::AllGraphics)
        { return { BindingType::Sampler, stage, slot }; }
    static BindingLayoutItem StorageBuffer(uint32_t slot, ShaderStage stage = ShaderStage::AllGraphics)
        { return { BindingType::StorageBuffer, stage, slot }; }
    static BindingLayoutItem StorageTexture(uint32_t slot, ShaderStage stage = ShaderStage::AllGraphics)
        { return { BindingType::StorageTexture, stage, slot }; }
};

struct BindingLayoutDesc
{
    std::vector<BindingLayoutItem> items;
};

// ---------------------------------------------------------------------------
// Binding set
// ---------------------------------------------------------------------------
struct BindingItem
{
    BindingType type    = BindingType::ConstantBuffer;
    uint32_t    slot    = 0;
    GpuBuffer   buffer;
    GpuTexture  texture;
    GpuSampler  sampler;

    static BindingItem ConstantBuffer(uint32_t slot, GpuBuffer buf)
        { BindingItem i; i.type = BindingType::ConstantBuffer; i.slot = slot; i.buffer = buf; return i; }
    static BindingItem Texture(uint32_t slot, GpuTexture tex)
        { BindingItem i; i.type = BindingType::Texture; i.slot = slot; i.texture = tex; return i; }
    static BindingItem Sampler(uint32_t slot, GpuSampler smp)
        { BindingItem i; i.type = BindingType::Sampler; i.slot = slot; i.sampler = smp; return i; }
    static BindingItem StorageBuffer(uint32_t slot, GpuBuffer buf)
        { BindingItem i; i.type = BindingType::StorageBuffer; i.slot = slot; i.buffer = buf; return i; }
    static BindingItem StorageTexture(uint32_t slot, GpuTexture tex)
        { BindingItem i; i.type = BindingType::StorageTexture; i.slot = slot; i.texture = tex; return i; }
};

struct BindingSetDesc
{
    std::vector<BindingItem> items;
};

// ---------------------------------------------------------------------------
// Framebuffer
// ---------------------------------------------------------------------------
struct FramebufferAttachment
{
    GpuTexture texture;
    uint32_t   mipLevel   = 0;
    uint32_t   arraySlice = 0;
};

struct FramebufferDesc
{
    std::vector<FramebufferAttachment> colorAttachments;
    FramebufferAttachment              depthAttachment;
};

// ---------------------------------------------------------------------------
// Rasterizer state
// ---------------------------------------------------------------------------
struct RasterizerDesc
{
    FillMode fillMode             = FillMode::Solid;
    CullMode cullMode             = CullMode::Back;
    bool     frontCCW             = true;
    int32_t  depthBias            = 0;
    float    slopeScaledDepthBias = 0.f;
    bool     depthClipEnable      = true;
};

// ---------------------------------------------------------------------------
// Depth-stencil state
// ---------------------------------------------------------------------------
struct StencilOpDesc
{
    StencilOp      failOp      = StencilOp::Keep;
    StencilOp      depthFailOp = StencilOp::Keep;
    StencilOp      passOp      = StencilOp::Keep;
    ComparisonFunc func        = ComparisonFunc::Always;
};

struct DepthStencilDesc
{
    bool           depthTestEnable  = true;
    bool           depthWriteEnable = true;
    ComparisonFunc depthFunc        = ComparisonFunc::Less;
    bool           stencilEnable    = false;
    uint8_t        stencilReadMask  = 0xFF;
    uint8_t        stencilWriteMask = 0xFF;
    StencilOpDesc  frontFace;
    StencilOpDesc  backFace;
};

// ---------------------------------------------------------------------------
// Blend state
// ---------------------------------------------------------------------------
struct RenderTargetBlendDesc
{
    bool        blendEnable   = false;
    BlendFactor srcBlend      = BlendFactor::SrcAlpha;
    BlendFactor dstBlend      = BlendFactor::InvSrcAlpha;
    BlendOp     blendOp       = BlendOp::Add;
    BlendFactor srcBlendAlpha = BlendFactor::One;
    BlendFactor dstBlendAlpha = BlendFactor::Zero;
    BlendOp     blendOpAlpha  = BlendOp::Add;
    uint8_t     writeMask     = 0xF;
};

struct BlendDesc
{
    RenderTargetBlendDesc renderTargets[8];
};

// ---------------------------------------------------------------------------
// Graphics pipeline descriptor
// ---------------------------------------------------------------------------
struct GraphicsPipelineDesc
{
    GpuShader      vs;
    GpuShader      hs;
    GpuShader      ds;
    GpuShader      gs;
    GpuShader      ps;
    GpuInputLayout inputLayout;
    PrimitiveType  primType = PrimitiveType::TriangleList;
    RasterizerDesc    rasterizer;
    DepthStencilDesc  depthStencil;
    BlendDesc         blend;
    std::vector<GpuBindingLayout> bindingLayouts;
};

// ---------------------------------------------------------------------------
// Compute pipeline descriptor
// ---------------------------------------------------------------------------
struct ComputePipelineDesc
{
    GpuShader cs;
    std::vector<GpuBindingLayout> bindingLayouts;
};

// ---------------------------------------------------------------------------
// Draw / dispatch argument structs
// ---------------------------------------------------------------------------
struct DrawArgs
{
    uint32_t vertexCount   = 0;
    uint32_t instanceCount = 1;
    uint32_t startVertex   = 0;
    uint32_t startInstance = 0;
};

struct DrawIndexedArgs
{
    uint32_t indexCount    = 0;
    uint32_t instanceCount = 1;
    uint32_t startIndex    = 0;
    int32_t  baseVertex    = 0;
    uint32_t startInstance = 0;
};

struct DispatchArgs
{
    uint32_t groupX = 1;
    uint32_t groupY = 1;
    uint32_t groupZ = 1;
};

// ---------------------------------------------------------------------------
// Render pass descriptor (used with ICommandContext::BeginRenderPass)
// ---------------------------------------------------------------------------
struct RenderPassDesc
{
    GpuFramebuffer framebuffer;
    bool           clearColor   = false;
    ClearValue     colorValue   = {};
    bool           clearDepth   = false;
    float          depthValue   = 1.0f;
    bool           clearStencil = false;
    uint8_t        stencilValue = 0;
};

#endif // GPU_TYPES_H
