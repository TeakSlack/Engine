#ifndef FRAME_GRAPH_H
#define FRAME_GRAPH_H

#include "RenderResource.h"
#include "FrameGraphBlackboard.h"
#include "TransientResourceSystem.h"
#include "Render/ICommandContext.h"
#include "Render/IGpuDevice.h"
#include <memory>

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

class PassBuilder;

class FrameGraph
{
public:
	template<typename PassData>
	RenderPass<PassData>& AddCallbackPass(
		const std::string& name,
		std::function<void(PassBuilder&, PassData&)> setup,
		std::function<void(const PassData&, const RenderPassResources&, ICommandContext*)> execute
	);

	RGTextureHandle ImportTexture(GpuTexture texture, const TextureDesc& desc, ResourceLayout layout);
	RGBufferHandle  ImportBuffer(GpuBuffer buffer, const BufferDesc& desc);

	void Compile();
	void Execute(IGpuDevice* device, ICommandContext* ctx);
	void Reset();

	FrameGraphBlackboard&       GetBlackboard()       { return m_Blackboard; }
	const FrameGraphBlackboard& GetBlackboard() const { return m_Blackboard; }

private:
	static ResourceLayout ToLayout(RGAccessType access);

	std::vector<RGPassNode>                       m_Passes;
	std::vector<std::unique_ptr<IRenderPassBase>> m_PassData;
	std::vector<RGResourceNode>                   m_Resources;
	TransientResourceSystem                       m_Transients;
	FrameGraphBlackboard                          m_Blackboard;

	friend class PassBuilder;
};

class PassBuilder
{
public:
	PassBuilder(FrameGraph* graph, RGPassNode* pass, uint32_t passIndex, const std::string& name)
		: m_FrameGraph(graph), m_Pass(pass), m_PassIndex(passIndex), m_Name(name) {}

	RGMutableTextureHandle CreateTexture(const TextureDesc& desc);
	RGMutableBufferHandle  CreateBuffer(const BufferDesc& desc);

	RGTextureHandle        ReadTexture(RGTextureHandle handle);
	RGBufferHandle         ReadBuffer(RGBufferHandle handle);

	RGMutableTextureHandle WriteTexture(RGMutableTextureHandle handle);
	RGMutableTextureHandle WriteDepth(RGMutableTextureHandle handle);
	RGMutableBufferHandle  WriteBuffer(RGMutableBufferHandle handle);

private:
	FrameGraph* m_FrameGraph;
	RGPassNode* m_Pass;
	uint32_t    m_PassIndex;
	std::string m_Name;
};

template<typename PassData>
inline RenderPass<PassData>& FrameGraph::AddCallbackPass(
	const std::string& name,
	std::function<void(PassBuilder&, PassData&)> setup,
	std::function<void(const PassData&, const RenderPassResources&, ICommandContext*)> execute)
{
	RGPassNode& node = m_Passes.emplace_back();
	node.Name = name;
	uint32_t passIndex = static_cast<uint32_t>(m_Passes.size()) - 1;

	auto owner = std::make_unique<RenderPass<PassData>>();
	RenderPass<PassData>& renderPass = *owner;

	PassBuilder builder(this, &node, passIndex, name);
	setup(builder, renderPass.data);

	// PassData lifetime is managed by owner, which is stored in m_PassData below.
	PassData* passDataPtr = &renderPass.data;
	node.ExecuteCallback = [execute, passDataPtr](const RenderPassResources& resources, ICommandContext* ctx)
	{
		execute(*passDataPtr, resources, ctx);
	};

	m_PassData.push_back(std::move(owner));
	return renderPass;
}

#endif // FRAME_GRAPH_H
