#ifndef FRAME_GRAPH_H
#define FRAME_GRAPH_H

#include "RenderResource.h"
#include "Render/ICommandContext.h"

struct IRenderPassBase
{
	virtual ~IRenderPassBase() = default;
};

template<typename PassData>
struct RenderPass : IRenderPassBase
{
	PassData data;
	RGMutableTextureHandle output;
};

struct PassBuilder;

class FrameGraph
{
public:
	template<typename PassData>
	RenderPass<PassData>& AddCallbackPass(
		const std::string& name,
		std::function<void(PassBuilder&, PassData&)> setup,
		std::function<void(const PassData&, const RenderPassResources&, ICommandContext*)> execute
	);

	RGTextureHandle ImportTexture(GpuTexture texture, const TextureDesc& desc);
	RGBufferHandle ImportBuffer(GpuBuffer buffer, const BufferDesc& desc);
private:
	std::vector<RGPassNode> m_Passes;
	std::vector<IRenderPassBase*> m_PassData;
	std::vector<RGResourceNode> m_Resources;

	friend class PassBuilder;
};

class PassBuilder
{
public:
	PassBuilder(FrameGraph* graph, RGPassNode* pass, const std::string& name) : m_FrameGraph(graph), m_Pass(pass), m_Name(name) {}

	RGMutableTextureHandle CreateTexture(const TextureDesc& desc);
	RGMutableBufferHandle CreateBuffer(const BufferDesc& desc);

	RGTextureHandle ReadTexture(RGTextureHandle handle);
	RGBufferHandle ReadBuffer(RGBufferHandle handle);

	RGMutableTextureHandle WriteTexture(RGMutableTextureHandle handle);
	RGMutableTextureHandle WriteDepth(RGMutableTextureHandle handle);
	RGMutableBufferHandle WriteBuffer(RGMutableBufferHandle handle);

private:
	FrameGraph* m_FrameGraph;
	RGPassNode* m_Pass;
	const std::string m_Name;
};

template<typename PassData>
inline RenderPass<PassData>& FrameGraph::AddCallbackPass(
	const std::string& name,
	std::function<void(PassBuilder&, PassData&)> setup, 
	std::function<void(const PassData&, const RenderPassResources&, ICommandContext*)> execute)
{
	// Allocated typed pass storage
	auto owner = std::make_unique<RenderPass<PassData>>();
	auto& renderPass = *owner;

	// Allocate new pass node within FrameGraph's flat table
	RGPassNode& node = m_Passes.emplace_back();
	node.Name = name;

	// Run the setup lambda to fill out the pass data and declare resource usage
	PassBuilder builder(this, &node, name);
	setup(builder, renderPass.data);

	// Store the execute lambda within the pass data for later invocation
	PassData* passData = new PassData(std::move(renderPass.data));
	node.ExecuteCallback = [execute, passData](const RenderPassResources& resources, ICommandContext* context)
		{
			execute(*passData, resources, context);
		};
	node.RefCount = static_cast<uint32_t>(node.Writes.size() + node.Creates.size());

	m_PassData.push_back(std::move(owner));

	return renderPass;
}

#endif // FRAME_GRAPH_H