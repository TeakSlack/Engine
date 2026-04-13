#include "FrameGraph.h"


RGMutableTextureHandle PassBuilder::CreateTexture(const TextureDesc& desc)
{
    // Allocate new resource node within FrameGraph's flat table
	uint16_t index = static_cast<uint16_t>(m_FrameGraph->m_Resources.size());
	RGResourceNode& node = m_FrameGraph->m_Resources.emplace_back();
	node.TextureDesc = desc;
	node.Name = desc.debugName;
	node.FirstPassIndex = static_cast<uint16_t>(m_FrameGraph->m_Passes.size()) - 1;

	// Tell the current pass it creates the resource
	m_Pass->Creates.push_back(index);

	// Return a typed, versioned handle to resource
	RGMutableTextureHandle handle;
	handle.Index = index;
	handle.Version = 0;
	return handle;
}

RGMutableBufferHandle PassBuilder::CreateBuffer(const BufferDesc& desc)
{
	// Allocate new resource node within FrameGraph's flat table
	uint16_t index = static_cast<uint16_t>(m_FrameGraph->m_Resources.size());
	RGResourceNode& node = m_FrameGraph->m_Resources.emplace_back();
	node.BufferDesc = desc;
	node.Name = desc.debugName;
	node.FirstPassIndex = static_cast<uint16_t>(m_FrameGraph->m_Passes.size()) - 1;

	// Tell the current pass it creates the resource
	m_Pass->Creates.push_back(index);

	// Return a typed, versioned handle to resource
	RGMutableBufferHandle handle;
	handle.Index = index;
	handle.Version = 0;
	return handle;
}

RGTextureHandle PassBuilder::ReadTexture(RGTextureHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to read an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to read culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to read out of date node!");

	m_Pass->Reads.push_back(handle.Index);
	return handle;
}

RGBufferHandle PassBuilder::ReadBuffer(RGBufferHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to read an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to read culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to read out of date node!");

	m_Pass->Reads.push_back(handle.Index);
	return handle;
}

RGMutableTextureHandle PassBuilder::WriteTexture(RGMutableTextureHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to write to an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to write a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to write out of date node!");

	node.Version++;
	m_Pass->Writes.push_back(handle.Index);

	RGMutableTextureHandle next;
	next.Index = handle.Index;
	next.Version = node.Version;
	return next;
}

RGMutableTextureHandle PassBuilder::WriteDepth(RGMutableTextureHandle handle)
{
	return WriteTexture(handle);
}

RGMutableBufferHandle PassBuilder::WriteBuffer(RGMutableBufferHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to write to an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to write a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to write out of date node!");

	node.Version++;
	m_Pass->Writes.push_back(handle.Index);

	RGMutableBufferHandle next;
	next.Index = handle.Index;
	next.Version = node.Version;
	return next;
}

RGTextureHandle FrameGraph::ImportTexture(GpuTexture texture, const TextureDesc& desc)
{
	CORE_ASSERT(texture.IsValid(), "Attempting to import an invalid texture!");

	uint16_t index = static_cast<uint16_t>(m_Resources.size());
	RGResourceNode& node = m_Resources.emplace_back();
	node.Kind = RGResourceNode::ResourceKind::Texture;
	node.TextureDesc = desc;
	node.Name = desc.debugName;
	node.Imported = true;
	node.ResolvedTexture = texture;
	node.FirstPassIndex = -1; // Not born in a pass

	RGTextureHandle handle;
	handle.Index = index;
	handle.Version = 0;
	return handle;
}

RGBufferHandle FrameGraph::ImportBuffer(GpuBuffer buffer, const BufferDesc& desc)
{
	CORE_ASSERT(buffer.id != 0, "Attempting to import an invalid buffer!");

	uint16_t index = static_cast<uint16_t>(m_Resources.size());
	RGResourceNode& node = m_Resources.emplace_back();
	node.Kind = RGResourceNode::ResourceKind::Buffer;
	node.BufferDesc = desc;
	node.Name = desc.debugName;
	node.Imported = true;
	node.ResolvedBuffer = buffer;
	node.FirstPassIndex = -1; // Not born in a pass

	RGBufferHandle handle;
	handle.Index = index;
	handle.Version = 0;
	return handle;
}
