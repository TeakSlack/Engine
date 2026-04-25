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
struct GpuBindlessLayoutTag  {};
struct GpuDescriptorTableTag {};

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
using GpuBindlessLayout   = GpuHandle<GpuBindlessLayoutTag>;
using GpuDescriptorTable  = GpuHandle<GpuDescriptorTableTag>;

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
    float X = 0.f, Y = 0.f;
    float Width = 0.f, Height = 0.f;
    float MinDepth = 0.f, MaxDepth = 1.f;
};

struct ScissorRect
{
    int X = 0, Y = 0, Width = 0, Height = 0;
};

// ---------------------------------------------------------------------------
// Clear colour
// ---------------------------------------------------------------------------
struct ClearValue
{
    float R = 0.f, G = 0.f, B = 0.f, A = 1.f;
};

// ---------------------------------------------------------------------------
// Buffer descriptor
// ---------------------------------------------------------------------------
struct BufferDesc
{
    uint64_t    ByteSize   = 0;
    BufferUsage Usage      = BufferUsage::None;
    bool        CpuVisible = false;
    const char* DebugName  = nullptr;
};

// ---------------------------------------------------------------------------
// Texture descriptor
// ---------------------------------------------------------------------------
enum class TextureDimension { Texture1D, Texture2D, Texture3D, Texture2DArray, TextureCube };

struct TextureDesc
{
    uint32_t         Width              = 1;
    uint32_t         Height             = 1;
    uint32_t         Depth              = 1;
    uint32_t         MipLevels          = 1;
    uint32_t         SampleCount        = 1;
    GpuFormat        Format             = GpuFormat::RGBA8_UNORM;
    TextureDimension Dimension          = TextureDimension::Texture2D;
    TextureUsage     Usage              = TextureUsage::ShaderResource;
    const char*      DebugName          = nullptr;
    // Optimized clear value — must match what you pass to BeginRenderPass.
    // D3D12 warns if these differ. Set for any RenderTarget or DepthStencil texture.
    ClearValue       OptimizedClearColor   = {};        // used when usage = RenderTarget
    float            OptimizedClearDepth   = 1.0f;      // used when usage = DepthStencil
    uint8_t          OptimizedClearStencil = 0;
};

// ---------------------------------------------------------------------------
// Sampler descriptor
// ---------------------------------------------------------------------------
struct SamplerDesc
{
    Filter         MinFilter     = Filter::Linear;
    Filter         MagFilter     = Filter::Linear;
    Filter         MipFilter     = Filter::Linear;
    AddressMode    AddressU      = AddressMode::Wrap;
    AddressMode    AddressV      = AddressMode::Wrap;
    AddressMode    AddressW      = AddressMode::Wrap;
    float          MipLODBias    = 0.f;
    uint32_t       MaxAnisotropy = 1;
    ComparisonFunc Comparison    = ComparisonFunc::Always;
    float          BorderColor[4]= { 0.f, 0.f, 0.f, 0.f };
    float          MinLOD        = 0.f;
    float          MaxLOD        = 1000.f;
};

// ---------------------------------------------------------------------------
// Shader descriptor
// ---------------------------------------------------------------------------
struct ShaderDesc
{
    ShaderStage  Stage      = ShaderStage::None;
    const void*  Bytecode   = nullptr;
    size_t       ByteSize   = 0;
    const char*  EntryPoint = "main";
    const char*  DebugName  = nullptr;
};

// ---------------------------------------------------------------------------
// Vertex input layout
// ---------------------------------------------------------------------------
struct VertexAttributeDesc
{
    const char* Name        = nullptr;
    GpuFormat   Format      = GpuFormat::Unknown;
    uint32_t    BufferIndex = 0;
    uint32_t    Offset      = 0;
    uint32_t    Stride      = 0;
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
    BindingType Type  = BindingType::ConstantBuffer;
    ShaderStage Stage = ShaderStage::AllGraphics;
    uint32_t    Slot  = 0;

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
    std::vector<BindingLayoutItem> Items;
};

// ---------------------------------------------------------------------------
// Binding set
// ---------------------------------------------------------------------------
struct BindingItem
{
    BindingType Type    = BindingType::ConstantBuffer;
    uint32_t    Slot    = 0;
    GpuBuffer   Buffer;
    GpuTexture  Texture;
    GpuSampler  Sampler;
    static BindingItem ConstantBuffer(uint32_t slot, GpuBuffer buf)
        { BindingItem i; i.Type = BindingType::ConstantBuffer; i.Slot = slot; i.Buffer = buf; return i; }
    static BindingItem Texture(uint32_t slot, GpuTexture tex)
        { BindingItem i; i.Type = BindingType::Texture; i.Slot = slot; i.Texture = tex; return i; }
    static BindingItem Sampler(uint32_t slot, GpuSampler smp)
        { BindingItem i; i.Type = BindingType::Sampler; i.Slot = slot; i.Sampler = smp; return i; }
    static BindingItem StorageBuffer(uint32_t slot, GpuBuffer buf)
        { BindingItem i; i.Type = BindingType::StorageBuffer; i.Slot = slot; i.Buffer = buf; return i; }
    static BindingItem StorageTexture(uint32_t slot, GpuTexture tex)
        { BindingItem i; i.Type = BindingType::StorageTexture; i.Slot = slot; i.Texture = tex; return i; }
};

struct BindingSetDesc
{
    std::vector<BindingItem> Items;
};

// ---------------------------------------------------------------------------
// Framebuffer
// ---------------------------------------------------------------------------
struct FramebufferAttachment
{
    GpuTexture Texture;
    uint32_t   MipLevel   = 0;
    uint32_t   ArraySlice = 0;
};

struct FramebufferDesc
{
    std::vector<FramebufferAttachment> ColorAttachments;
    FramebufferAttachment              DepthAttachment;
};

// ---------------------------------------------------------------------------
// Rasterizer state
// ---------------------------------------------------------------------------
struct RasterizerDesc
{
    FillMode FillMode             = FillMode::Solid;
    CullMode CullMode             = CullMode::Back;
    bool     FrontCCW             = true;
    int32_t  DepthBias            = 0;
    float    SlopeScaledDepthBias = 0.f;
    bool     DepthClipEnable      = true;
};

// ---------------------------------------------------------------------------
// Depth-stencil state
// ---------------------------------------------------------------------------
struct StencilOpDesc
{
    StencilOp      FailOp      = StencilOp::Keep;
    StencilOp      DepthFailOp = StencilOp::Keep;
    StencilOp      PassOp      = StencilOp::Keep;
    ComparisonFunc Func        = ComparisonFunc::Always;
};

struct DepthStencilDesc
{
    bool           DepthTestEnable  = true;
    bool           DepthWriteEnable = true;
    ComparisonFunc DepthFunc        = ComparisonFunc::Less;
    bool           StencilEnable    = false;
    uint8_t        StencilReadMask  = 0xFF;
    uint8_t        StencilWriteMask = 0xFF;
    StencilOpDesc  FrontFace;
    StencilOpDesc  BackFace;
};

// ---------------------------------------------------------------------------
// Blend state
// ---------------------------------------------------------------------------
struct RenderTargetBlendDesc
{
    bool        BlendEnable   = false;
    BlendFactor SrcBlend      = BlendFactor::SrcAlpha;
    BlendFactor DstBlend      = BlendFactor::InvSrcAlpha;
    BlendOp     BlendOperator = BlendOp::Add;
    BlendFactor SrcBlendAlpha = BlendFactor::One;
    BlendFactor DstBlendAlpha = BlendFactor::Zero;
    BlendOp     BlendOpAlpha  = BlendOp::Add;
    uint8_t     WriteMask     = 0xF;
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
    GpuShader      VS;
    GpuShader      HS;
    GpuShader      DS;
    GpuShader      GS;
    GpuShader      PS;
    GpuInputLayout InputLayout;
    PrimitiveType  PrimType = PrimitiveType::TriangleList;
    RasterizerDesc    Rasterizer;
    DepthStencilDesc  DepthStencil;
    BlendDesc         Blend;
    std::vector<GpuBindingLayout> BindingLayouts;
};

// ---------------------------------------------------------------------------
// Compute pipeline descriptor
// ---------------------------------------------------------------------------
struct ComputePipelineDesc
{
    GpuShader CS;
    std::vector<GpuBindingLayout> BindingLayouts;
};

// ---------------------------------------------------------------------------
// Draw / dispatch argument structs
// ---------------------------------------------------------------------------
struct DrawArgs
{
    uint32_t VertexCount   = 0;
    uint32_t InstanceCount = 1;
    uint32_t StartVertex   = 0;
    uint32_t StartInstance = 0;
};

struct DrawIndexedArgs
{
    uint32_t IndexCount    = 0;
    uint32_t InstanceCount = 1;
    uint32_t StartIndex    = 0;
    int32_t  BaseVertex    = 0;
    uint32_t StartInstance = 0;
};

struct DispatchArgs
{
    uint32_t GroupX = 1;
    uint32_t GroupY = 1;
    uint32_t GroupZ = 1;
};

// ---------------------------------------------------------------------------
// Render pass descriptor (used with ICommandContext::BeginRenderPass)
// ---------------------------------------------------------------------------
struct RenderPassDesc
{
    GpuFramebuffer Framebuffer;
    bool           ClearColor   = false;
    ClearValue     ColorValue   = {};
    bool           ClearDepth   = false;
    float          DepthValue   = 1.0f;
    bool           ClearStencil = false;
    uint8_t        StencilValue = 0;
};

// ---------------------------------------------------------------------------
// Bindless resource layout descriptor
// ---------------------------------------------------------------------------

enum class BindlessResourceType : uint8_t
{
    Texture,
    Sampler,
    Buffer,
};

inline BindlessResourceType operator|(BindlessResourceType a, BindlessResourceType b)
{
    return (BindlessResourceType)((uint8_t)a | (uint8_t)b);
}

inline uint8_t operator&(BindlessResourceType a, BindlessResourceType b)
{
    return (uint8_t)a & (uint8_t)b;
}

struct BindlessLayoutDesc
{
	BindlessResourceType ResourceType = BindlessResourceType::Texture;
	uint32_t             MaxResources = 65536; // Max descriptors
};

using DescriptorIndex = uint32_t;
static constexpr DescriptorIndex InvalidDescriptorIndex = UINT32_MAX;

#endif // GPU_TYPES_H
