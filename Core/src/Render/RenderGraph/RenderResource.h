#ifndef RENDER_RESOURCE_H
#define RENDER_RESOURCE_H

#include "Render/GpuTypes.h"
#include "Util/Assert.h"
#include <cstdint>
#include <string>
#include <functional>

struct PassData;

// 16-bit index and 16-bit version, fits within uint32_t
struct RGHandle
{
	uint16_t Index;
	uint16_t Version = 0xFFFF;

	bool IsValid() const { return Index != 0xFFFF ; }
	bool operator==(const RGHandle&) const = default;
};

// Tag types
struct RGTextureTag {};
struct RGBufferTag {};

// Read handle
template<typename Tag>
struct RGReadHandle : RGHandle {};

// Write handle
template<typename Tag>
struct RGWriteHandle : RGHandle
{
	operator RGReadHandle<Tag>() const { return RGReadHandle<Tag>{ Index, Version }; }
};

using RGTextureHandle = RGReadHandle<RGTextureTag>;
using RGMutableTextureHandle = RGWriteHandle<RGTextureTag>;
using RGBufferHandle = RGReadHandle<RGBufferTag>;
using RGMutableBufferHandle = RGWriteHandle<RGBufferTag>;

struct RGResourceNode
{
	enum class ResourceKind : uint8_t { Texture, Buffer };

	ResourceKind Kind;
	std::string Name;
	bool Imported = false;

	TextureDesc TextureDesc;
	BufferDesc BufferDesc;

	uint16_t Version;

	bool Culled = false;
	uint32_t RefCount = 0;
	uint32_t FirstPassIndex = -1;
	uint32_t LastPassIndex = -1;

	GpuTexture ResolvedTexture;
	GpuBuffer ResolvedBuffer;
};

class RenderPassResources
{
public:
	RenderPassResources(const std::vector<RGResourceNode>& resources) : m_Resources(resources) {}

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

struct RGPassNode
{
	std::string Name;
	bool AsyncCompute = false;

	std::vector<uint16_t> Creates;
	std::vector<uint16_t> Reads;
	std::vector<uint16_t> Writes;
	std::vector<uint16_t> Dependencies;

	bool Culled = false;
	uint32_t RefCount = 0;

	// ---- Type erased execution callback ----
	std::function<void(const PassData&, const RenderPassResources&, ICommandContext*)> ExecuteCallback;
};

#endif // RENDER_RESOURCE_H