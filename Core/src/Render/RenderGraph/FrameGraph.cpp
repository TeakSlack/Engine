#include "FrameGraph.h"
#include <algorithm>

// -----------------------------------------------------------------------
// PassBuilder
// -----------------------------------------------------------------------

RGMutableTextureHandle PassBuilder::CreateTexture(const TextureDesc& desc)
{
	uint16_t index = static_cast<uint16_t>(m_FrameGraph->m_Resources.size());
	RGResourceNode& node = m_FrameGraph->m_Resources.emplace_back();
	node.Kind           = RGResourceNode::ResourceKind::Texture;
	node.TextureDesc    = desc;
	node.Name           = desc.debugName ? desc.debugName : "";
	node.FirstPassIndex = m_PassIndex;

	m_Pass->Creates.push_back(index);

	RGMutableTextureHandle handle;
	handle.Index   = index;
	handle.Version = 0;
	return handle;
}

RGMutableBufferHandle PassBuilder::CreateBuffer(const BufferDesc& desc)
{
	uint16_t index = static_cast<uint16_t>(m_FrameGraph->m_Resources.size());
	RGResourceNode& node = m_FrameGraph->m_Resources.emplace_back();
	node.Kind           = RGResourceNode::ResourceKind::Buffer;
	node.BufferDesc     = desc;
	node.Name           = desc.debugName ? desc.debugName : "";
	node.FirstPassIndex = m_PassIndex;

	m_Pass->Creates.push_back(index);

	RGMutableBufferHandle handle;
	handle.Index   = index;
	handle.Version = 0;
	return handle;
}

RGTextureHandle PassBuilder::ReadTexture(RGTextureHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to read an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to read a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to read an out of date node!");

	m_Pass->Accesses.push_back({ handle.Index, RGAccessType::Read });
	return handle;
}

RGBufferHandle PassBuilder::ReadBuffer(RGBufferHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to read an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to read a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to read an out of date node!");

	m_Pass->Accesses.push_back({ handle.Index, RGAccessType::Read });
	return handle;
}

RGMutableTextureHandle PassBuilder::WriteTexture(RGMutableTextureHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to write to an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to write a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to write an out of date node!");

	node.Version++;
	m_Pass->Accesses.push_back({ handle.Index, RGAccessType::WriteRT });

	RGMutableTextureHandle next;
	next.Index   = handle.Index;
	next.Version = node.Version;
	return next;
}

RGMutableTextureHandle PassBuilder::WriteDepth(RGMutableTextureHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to write to an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to write a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to write an out of date node!");

	node.Version++;
	m_Pass->Accesses.push_back({ handle.Index, RGAccessType::WriteDepth });

	RGMutableTextureHandle next;
	next.Index   = handle.Index;
	next.Version = node.Version;
	return next;
}

RGMutableBufferHandle PassBuilder::WriteBuffer(RGMutableBufferHandle handle)
{
	CORE_ASSERT(handle.IsValid(), "Attempting to write to an invalid handle!");

	RGResourceNode& node = m_FrameGraph->m_Resources[handle.Index];
	CORE_ASSERT(!node.Culled, "Attempting to write a culled node!");
	CORE_ASSERT(handle.Version == node.Version, "Attempting to write an out of date node!");

	node.Version++;
	m_Pass->Accesses.push_back({ handle.Index, RGAccessType::WriteUAV });

	RGMutableBufferHandle next;
	next.Index   = handle.Index;
	next.Version = node.Version;
	return next;
}

// -----------------------------------------------------------------------
// FrameGraph
// -----------------------------------------------------------------------

RGTextureHandle FrameGraph::ImportTexture(GpuTexture texture, const TextureDesc& desc, ResourceLayout layout)
{
	CORE_ASSERT(texture.IsValid(), "Attempting to import an invalid texture!");

	uint16_t index = static_cast<uint16_t>(m_Resources.size());
	RGResourceNode& node = m_Resources.emplace_back();
	node.Kind            = RGResourceNode::ResourceKind::Texture;
	node.TextureDesc     = desc;
	node.Name            = desc.debugName ? desc.debugName : "";
	node.Imported        = true;
	node.RefCount        = 1;
	node.ImportedLayout  = layout;
	node.ResolvedTexture = texture;

	RGTextureHandle handle;
	handle.Index   = index;
	handle.Version = 0;
	return handle;
}

RGBufferHandle FrameGraph::ImportBuffer(GpuBuffer buffer, const BufferDesc& desc)
{
	CORE_ASSERT(buffer.id != 0, "Attempting to import an invalid buffer!");

	uint16_t index = static_cast<uint16_t>(m_Resources.size());
	RGResourceNode& node = m_Resources.emplace_back();
	node.Kind           = RGResourceNode::ResourceKind::Buffer;
	node.BufferDesc     = desc;
	node.Name           = desc.debugName ? desc.debugName : "";
	node.Imported       = true;
	node.RefCount       = 1;
	node.ResolvedBuffer = buffer;

	RGBufferHandle handle;
	handle.Index   = index;
	handle.Version = 0;
	return handle;
}

ResourceLayout FrameGraph::ToLayout(RGAccessType access)
{
	switch (access)
	{
		case RGAccessType::Read:       return ResourceLayout::ShaderResource;
		case RGAccessType::WriteRT:    return ResourceLayout::RenderTarget;
		case RGAccessType::WriteDepth: return ResourceLayout::DepthWrite;
		case RGAccessType::WriteUAV:   return ResourceLayout::UnorderedAccess;
	}
	return ResourceLayout::Undefined;
}

void FrameGraph::Compile()
{
	// ------------------------------------------------------------------
	// Step 1 — Seed reference counts
	// ------------------------------------------------------------------
	for (RGPassNode& pass : m_Passes)
	{
		uint32_t writeCount = 0;
		for (const RGPassAccess& access : pass.Accesses)
			if (access.Type != RGAccessType::Read)
				writeCount++;
		pass.RefCount = writeCount + static_cast<uint32_t>(pass.Creates.size());
	}

	for (RGResourceNode& resource : m_Resources)
		if (resource.Imported)
			resource.RefCount = 1;

	// ------------------------------------------------------------------
	// Step 2 — Cull unreferenced passes
	// ------------------------------------------------------------------
	std::vector<uint32_t> unreferenced;
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_Passes.size()); i++)
		if (m_Passes[i].RefCount == 0)
			unreferenced.push_back(i);

	while (!unreferenced.empty())
	{
		uint32_t passIndex = unreferenced.back();
		unreferenced.pop_back();

		RGPassNode& pass = m_Passes[passIndex];
		pass.Culled = true;

		for (const RGPassAccess& access : pass.Accesses)
		{
			if (access.Type != RGAccessType::Read)
				continue;

			RGResourceNode& resource = m_Resources[access.ResourceIndex];
			if (resource.RefCount == 0)
				continue;

			resource.RefCount--;

			if (resource.RefCount == 0 && !resource.Imported)
			{
				RGPassNode& creator = m_Passes[resource.FirstPassIndex];
				creator.RefCount--;
				if (creator.RefCount == 0)
					unreferenced.push_back(resource.FirstPassIndex);
			}
		}
	}

	// ------------------------------------------------------------------
	// Step 3 — Resource lifetime computation
	// ------------------------------------------------------------------
	for (uint32_t passIndex = 0; passIndex < static_cast<uint32_t>(m_Passes.size()); passIndex++)
	{
		const RGPassNode& pass = m_Passes[passIndex];
		if (pass.Culled)
			continue;

		auto expandLifetime = [&](uint16_t resourceIndex)
		{
			RGResourceNode& resource = m_Resources[resourceIndex];
			resource.FirstPassIndex = std::min(resource.FirstPassIndex, passIndex);
			resource.LastPassIndex  = std::max(resource.LastPassIndex,  passIndex);
		};

		for (uint16_t i : pass.Creates)
			expandLifetime(i);
		for (const RGPassAccess& access : pass.Accesses)
			expandLifetime(access.ResourceIndex);
	}

	// ------------------------------------------------------------------
	// Step 4 — Barrier inference
	// ------------------------------------------------------------------
	std::vector<ResourceLayout> currentLayout(m_Resources.size(), ResourceLayout::Undefined);

	for (uint32_t i = 0; i < static_cast<uint32_t>(m_Resources.size()); i++)
		if (m_Resources[i].Imported)
			currentLayout[i] = m_Resources[i].ImportedLayout;

	for (RGPassNode& pass : m_Passes)
	{
		if (pass.Culled)
			continue;

		for (const RGPassAccess& access : pass.Accesses)
		{
			ResourceLayout needed = ToLayout(access.Type);

			if (currentLayout[access.ResourceIndex] != needed)
			{
				pass.Barriers.push_back({
					access.ResourceIndex,
					currentLayout[access.ResourceIndex],
					needed
				});
				currentLayout[access.ResourceIndex] = needed;
			}
		}
	}
}

void FrameGraph::Execute(IGpuDevice* device, ICommandContext* ctx)
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_Passes.size()); i++)
	{
		RGPassNode& pass = m_Passes[i];
		if (pass.Culled)
			continue;

		// Allocate transient resources created by this pass.
		for (uint16_t idx : pass.Creates)
			m_Transients.Acquire(device, &m_Resources[idx]);

		// Issue barriers inferred during Compile().
		for (const ResourceBarrier& barrier : pass.Barriers)
		{
			RGResourceNode& resource = m_Resources[barrier.ResourceIndex];
			if (resource.Kind == RGResourceNode::ResourceKind::Texture)
				ctx->TransitionTexture(resource.ResolvedTexture, barrier.To);
			else
				ctx->TransitionBuffer(resource.ResolvedBuffer, barrier.To);
		}

		// Execute the pass callback.
		RenderPassResources resources(m_Resources);
		pass.ExecuteCallback(resources, ctx);

		// Release transient resources whose lifetime ends after this pass.
		for (RGResourceNode& resource : m_Resources)
		{
			if (!resource.Imported && !resource.Culled && resource.LastPassIndex == i)
				m_Transients.Release(device, &resource);
		}
	}
}

void FrameGraph::Reset()
{
	m_Passes.clear();
	m_PassData.clear();
	m_Resources.clear();
	m_Blackboard.Clear();
}
