#ifndef RENDER_RESOURCE_H
#define RENDER_RESOURCE_H

#include "Render/GpuTypes.h"
#include "Util/Assert.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ICommandContext;

// -----------------------------------------------------------------------
// Versioned handles
// Version 0xFFFF is the invalid sentinel; index 0 is a valid resource slot.
// -----------------------------------------------------------------------
struct RGHandle
{
    uint16_t Index   = 0;
    uint16_t Version = 0xFFFF;

    bool IsValid() const { return Version != 0xFFFF; }
    bool operator==(const RGHandle&) const = default;
};

struct RGTextureTag {};
struct RGBufferTag  {};

template<typename Tag>
struct RGReadHandle : RGHandle {};

template<typename Tag>
struct RGWriteHandle : RGHandle
{
    operator RGReadHandle<Tag>() const { return RGReadHandle<Tag>{ Index, Version }; }
};

using RGTextureHandle        = RGReadHandle<RGTextureTag>;
using RGMutableTextureHandle = RGWriteHandle<RGTextureTag>;
using RGBufferHandle         = RGReadHandle<RGBufferTag>;
using RGMutableBufferHandle  = RGWriteHandle<RGBufferTag>;

// -----------------------------------------------------------------------
// Pass access types — how a pass uses each resource
// -----------------------------------------------------------------------
enum class RGAccessType : uint8_t
{
    Read,        // SRV / shader resource
    WriteRT,     // render target colour attachment
    WriteDepth,  // depth-stencil write
    WriteUAV,    // unordered access / compute write
};

struct RGPassAccess
{
    uint16_t     ResourceIndex;
    RGAccessType Type;
};

// -----------------------------------------------------------------------
// Resource barrier — produced during compilation, consumed during execution
// -----------------------------------------------------------------------
struct ResourceBarrier
{
    uint16_t       ResourceIndex;
    ResourceLayout From;
    ResourceLayout To;
};

// -----------------------------------------------------------------------
// Internal resource node
// -----------------------------------------------------------------------
struct RGResourceNode
{
    enum class ResourceKind : uint8_t { Texture, Buffer };

    ResourceKind Kind     = ResourceKind::Texture;
    std::string  Name;
    bool         Imported = false;
    bool         Culled   = false;
    uint32_t     RefCount = 0;

    // Lifetime (pass indices); expanded during compilation step 3.
    // FirstPassIndex starts at UINT32_MAX so std::min always wins on first touch.
    // LastPassIndex starts at 0 so std::max always wins on first touch.
    uint32_t FirstPassIndex = UINT32_MAX;
    uint32_t LastPassIndex  = 0;

    TextureDesc TextureDesc;
    BufferDesc  BufferDesc;

    uint16_t       Version        = 0;
    ResourceLayout ImportedLayout = ResourceLayout::Undefined;

    GpuTexture ResolvedTexture;
    GpuBuffer  ResolvedBuffer;
};

// -----------------------------------------------------------------------
// RenderPassResources — execution-phase resource accessor
// -----------------------------------------------------------------------
class RenderPassResources
{
public:
    explicit RenderPassResources(const std::vector<RGResourceNode>& resources)
        : m_Resources(resources) {}

    GpuTexture GetTexture(RGTextureHandle handle) const
    {
        const RGResourceNode& node = m_Resources[handle.Index];
        CORE_ASSERT(!node.Culled, "Attempting to access a culled resource!");
        CORE_ASSERT(handle.Version == node.Version, "Handle version out of date!");
        return node.ResolvedTexture;
    }

    GpuTexture GetTexture(RGMutableTextureHandle handle) const
    {
        const RGResourceNode& node = m_Resources[handle.Index];
        CORE_ASSERT(!node.Culled, "Attempting to access a culled resource!");
        CORE_ASSERT(handle.Version == node.Version, "Handle version out of date!");
        return node.ResolvedTexture;
    }

    GpuBuffer GetBuffer(RGBufferHandle handle) const
    {
        const RGResourceNode& node = m_Resources[handle.Index];
        CORE_ASSERT(!node.Culled, "Attempting to access a culled resource!");
        CORE_ASSERT(handle.Version == node.Version, "Handle version out of date!");
        return node.ResolvedBuffer;
    }

    GpuBuffer GetBuffer(RGMutableBufferHandle handle) const
    {
        const RGResourceNode& node = m_Resources[handle.Index];
        CORE_ASSERT(!node.Culled, "Attempting to access a culled resource!");
        CORE_ASSERT(handle.Version == node.Version, "Handle version out of date!");
        return node.ResolvedBuffer;
    }

private:
    const std::vector<RGResourceNode>& m_Resources;
};

// -----------------------------------------------------------------------
// Internal pass node
// -----------------------------------------------------------------------
struct RGPassNode
{
    std::string Name;
    bool        AsyncCompute = false;

    std::vector<uint16_t>     Creates;   // transient resources born this pass
    std::vector<RGPassAccess> Accesses;  // all reads and writes with access type

    bool     Culled   = false;
    uint32_t RefCount = 0;

    std::vector<ResourceBarrier> Barriers;

    std::function<void(const RenderPassResources&, ICommandContext*)> ExecuteCallback;
};

#endif // RENDER_RESOURCE_H
