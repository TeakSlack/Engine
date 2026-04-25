#include "AppLayer.h"
#define NOMINMAX

#ifdef COMPILE_WITH_VULKAN
#include <Graphics/VulkanDevice.h>
#endif

#ifdef COMPILE_WITH_DX12
#include <Graphics/D3D12Device.h>
#endif

#include <Engine.h>
#include <Window/GLFWWindow.h>
#include <Events/ApplicationEvents.h>
#include <Render/GpuTypes.h>
#include <fstream>

// -------------------------------------------------------------------------
// Utility
// -------------------------------------------------------------------------

struct Vertex2D
{
	float x, y;
	float r, g, b;
};

Vertex2D vertices[] =
{
	{ -0.5f,   0.5f, 1.0f, 0.0f, 0.0f },
	{  0.5f,   0.5f, 0.0f, 1.0f, 0.0f },
	{  0.5f,  -0.5f, 0.0f, 0.0f, 1.0f },
	{ -0.5f,  -0.5f, 1.0f, 1.0f, 0.0f }
};

uint32_t indices[] =
{
	0, 2, 1,
	2, 0, 3
};

std::vector<uint8_t> AppLayer::LoadBinary(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		APP_ERROR("Cannot open shader: {}", path.string());
		return {};
	}
	auto size = static_cast<size_t>(file.tellg());
	std::vector<uint8_t> data(size);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), size);
	return data;
}

// -------------------------------------------------------------------------
// Framebuffer management
// -------------------------------------------------------------------------

void AppLayer::CreateFramebuffers()
{
	auto extent = m_WindowSystem->GetExtent(m_WindowHandle);
	m_Width  = (uint32_t)extent.x;
	m_Height = (uint32_t)extent.y;

	const auto& backBuffers = m_GpuDevice->GetBackBufferTextures();
	m_Framebuffers.reserve(backBuffers.size());
	for (GpuTexture colorTex : backBuffers)
	{
		FramebufferDesc fbDesc;
		fbDesc.colorAttachments.push_back({ colorTex });
		m_Framebuffers.push_back(m_GpuDevice->CreateFramebuffer(fbDesc));
	}
}

void AppLayer::DestroyFramebuffers()
{
	for (auto fb : m_Framebuffers)
		m_GpuDevice->DestroyFramebuffer(fb);
	m_Framebuffers.clear();
}

// -------------------------------------------------------------------------
// Pipeline management
// -------------------------------------------------------------------------

void AppLayer::CreatePipeline()
{
	// Create and upload vertex/index buffers
	BufferDesc vbDesc;
	vbDesc.byteSize = sizeof(vertices);
	vbDesc.debugName = "VertexBuffer";
	vbDesc.usage = BufferUsage::Vertex;

	BufferDesc ibDesc;
	ibDesc.byteSize = sizeof(indices);
	ibDesc.debugName = "IndexBuffer";
	ibDesc.usage = BufferUsage::Index;

	m_VertexBuffer = m_GpuDevice->CreateBuffer(vbDesc);
	m_IndexBuffer = m_GpuDevice->CreateBuffer(ibDesc);

	m_CommandContext->Open();
	m_CommandContext->WriteBuffer(m_VertexBuffer, vertices, sizeof(vertices));
	m_CommandContext->WriteBuffer(m_IndexBuffer, indices, sizeof(indices));
	m_CommandContext->Close();

	// Execute upload commands and wait for completion before destroying staging buffers
	m_GpuDevice->ExecuteCommandContext(*m_CommandContext);
	m_GpuDevice->WaitForIdle();

	// Create pipeline (shaders, input layout, etc.)
	auto vsBytecode = LoadBinary("shaders/quad_vert.spv");
	auto psBytecode = LoadBinary("shaders/quad_frag.spv");

	ShaderDesc vsDesc;
	vsDesc.stage = ShaderStage::Vertex;
	vsDesc.debugName = "VertexShader";
	vsDesc.entryPoint = "main";
	vsDesc.bytecode = vsBytecode.data();
	vsDesc.byteSize = vsBytecode.size();
	
	ShaderDesc psDesc;
	psDesc.stage = ShaderStage::Pixel;
	psDesc.debugName = "PixelShader";
	psDesc.entryPoint = "main";
	psDesc.bytecode = psBytecode.data();
	psDesc.byteSize = psBytecode.size();

	GpuShader vsShader = m_GpuDevice->CreateShader(vsDesc);
	GpuShader psShader = m_GpuDevice->CreateShader(psDesc);

	// Describe input layout
	std::vector<VertexAttributeDesc> attribs = {
		{"POSITION", GpuFormat::RG32_FLOAT, 0, 0, sizeof(Vertex2D)},
		{"COLOR",    GpuFormat::RGB32_FLOAT, 0, 8, sizeof(Vertex2D)}
	};
	GpuInputLayout inputLayout = m_GpuDevice->CreateInputLayout(attribs, vsShader);

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.vs = vsShader;
	pipelineDesc.ps = psShader;
	pipelineDesc.inputLayout = inputLayout;
	pipelineDesc.primType = PrimitiveType::TriangleList;
	pipelineDesc.rasterizer.frontCCW = true;
	pipelineDesc.rasterizer.cullMode = CullMode::Back;

	CORE_ASSERT(!m_Framebuffers.empty(), "No framebuffers available to create pipeline");
	m_Pipeline = m_GpuDevice->CreateGraphicsPipeline(pipelineDesc, m_Framebuffers[0]);
}

void AppLayer::DestroyPipeline()
{
	m_GpuDevice->DestroyBuffer(m_VertexBuffer);
	m_GpuDevice->DestroyBuffer(m_IndexBuffer);
	m_GpuDevice->DestroyGraphicsPipeline(m_Pipeline);
}

// -------------------------------------------------------------------------
// Layer lifecycle
// -------------------------------------------------------------------------

void AppLayer::OnAttach()
{
	WindowDesc desc;
	desc.title  = "FrameGraph Testbed";
	desc.width  = 1280;
	desc.height = 720;
	m_WindowSystem = Engine::Get().GetSubmodule<GLFWWindowSystem>();
	m_WindowHandle = m_WindowSystem->OpenWindow(desc);

	auto* glfwWS = dynamic_cast<GLFWWindowSystem*>(m_WindowSystem);
	m_GlfwWindow = glfwWS->GetGLFWWindow(m_WindowHandle);

	m_RenderDevice = std::make_unique<VulkanDevice>(m_GlfwWindow);
	//m_RenderDevice = std::make_unique<D3D12Device>(glfwWS->GetNativeHandle(m_WindowHandle));
	m_GpuDevice    = m_RenderDevice->CreateDevice();

	auto extent = m_WindowSystem->GetExtent(m_WindowHandle);
	m_RenderDevice->CreateSwapchain((uint32_t)extent.x, (uint32_t)extent.y);

	m_CommandContext = m_GpuDevice->CreateCommandContext();
	m_FrameGraph     = std::make_unique<FrameGraph>();

	CreateFramebuffers();
	CreatePipeline();
}

void AppLayer::OnDetach()
{
	if (m_GpuDevice)
		m_GpuDevice->WaitForIdle();

	DestroyPipeline();
	DestroyFramebuffers();

	m_CommandContext.reset();
	m_FrameGraph.reset();
	m_GpuDevice = nullptr;
	m_RenderDevice.reset();

	m_WindowSystem->CloseWindow(m_WindowHandle);
}

void AppLayer::OnUpdate(float deltaTime)
{
	if (m_WindowSystem->ShouldClose(m_WindowHandle))
	{
		Engine::Get().RequestStop();
		return;
	}

	// ---- Swapchain resize ----
	if (m_PendingResize)
	{
		m_GpuDevice->WaitForIdle();
		m_GpuDevice->RunGarbageCollection();
		DestroyFramebuffers();
		m_RenderDevice->RecreateSwapchain(m_Width, m_Height);
		CreateFramebuffers();
		m_PendingResize = false;
	}

	m_RenderDevice->BeginFrame();
	m_GpuDevice->RunGarbageCollection();

	uint32_t imageIdx = m_RenderDevice->GetCurrentImageIndex();

	// ---- FrameGraph ----
	m_FrameGraph->Reset();

	GpuFramebuffer fb = m_Framebuffers[imageIdx];

	TextureDesc bbDesc;
	bbDesc.width     = m_Width;
	bbDesc.height    = m_Height;
	bbDesc.format    = GpuFormat::BGRA8_UNORM;
	bbDesc.debugName = "Backbuffer";
	RGMutableTextureHandle backbuffer = m_FrameGraph->ImportMutableTexture(
		m_GpuDevice->GetBackBufferTextures()[imageIdx],
		bbDesc,
		m_FrameCount++ == 0 ? ResourceLayout::Undefined : ResourceLayout::Present);

	struct ClearPassData { RGMutableTextureHandle target; };
	auto& clearPass = m_FrameGraph->AddCallbackPass<ClearPassData>(
		"Clear",
		[&](PassBuilder& builder, ClearPassData& data)
		{
			data.target = builder.WriteTexture(backbuffer);
		},
		[fb](const ClearPassData& data, const RenderPassResources& res, ICommandContext* cmd)
		{
			RenderPassDesc passDesc;
			passDesc.framebuffer = fb;
			passDesc.clearDepth  = true;
			passDesc.depthValue  = 1.0f;
			passDesc.clearColor  = true;
			passDesc.colorValue = ClearValue{0.0f, 0.0f, 0.0f, 1.0f};
			cmd->BeginRenderPass(passDesc);
			cmd->EndRenderPass();
		}
	);

	struct QuadPassData { RGMutableTextureHandle target; };
	m_FrameGraph->AddCallbackPass<QuadPassData>(
		"DrawQuad",
		[&](PassBuilder& builder, QuadPassData& data)
		{
			data.target = builder.WriteTexture(clearPass.data.target);
		},
		[fb, this](const QuadPassData& data, const RenderPassResources& res, ICommandContext* cmd)
		{
			RenderPassDesc passDesc;
			passDesc.framebuffer = fb;
			passDesc.clearDepth  = false;
			passDesc.clearColor  = false;
			cmd->BeginRenderPass(passDesc);
			cmd->SetGraphicsPipeline(m_Pipeline);
			cmd->SetVertexBuffer(0, m_VertexBuffer, 0);
			cmd->SetIndexBuffer(m_IndexBuffer, GpuFormat::R32_UINT, 0);

			cmd->SetViewport(0.0f, 0.0f, (float)m_Width, (float)m_Height);
			cmd->SetScissor(0, 0, m_Width, m_Height);

			DrawIndexedArgs drawArgs;
			drawArgs.indexCount = 6;
			drawArgs.instanceCount = 1;

			cmd->DrawIndexed(drawArgs);
			cmd->EndRenderPass();
		}
	);

	m_FrameGraph->Compile();

	m_CommandContext->Open();
	m_FrameGraph->Execute(m_GpuDevice, m_CommandContext.get());
	m_CommandContext->Close();

	m_GpuDevice->ExecuteCommandContext(*m_CommandContext);
	m_RenderDevice->Present();
}

void AppLayer::OnEvent(Event& event)
{
	EventDispatcher d(event);

	d.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e)
	{
		if (e.GetWindow() == m_WindowHandle)
		{
			m_Width  = e.GetWidth();
			m_Height = e.GetHeight();
			m_PendingResize = true;
		}
		return false;
	});
}
