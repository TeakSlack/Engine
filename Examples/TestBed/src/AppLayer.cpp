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
#include <Render/ShaderLoader.h>

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
		fbDesc.ColorAttachments.push_back({ colorTex });
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
// Blit pipeline
// -------------------------------------------------------------------------

void AppLayer::CreateBlitPipeline()
{
	auto vsBytecode = ShaderLoader::LoadBinary("shaders/blit.vs.cso");
	auto psBytecode = ShaderLoader::LoadBinary("shaders/blit.ps.cso");

	ShaderDesc vsDesc;
	vsDesc.Stage      = ShaderStage::Vertex;
	vsDesc.DebugName  = "BlitVS";
	vsDesc.EntryPoint = "main";
	vsDesc.Bytecode   = vsBytecode.data();
	vsDesc.ByteSize   = vsBytecode.size();

	ShaderDesc psDesc;
	psDesc.Stage      = ShaderStage::Pixel;
	psDesc.DebugName  = "BlitPS";
	psDesc.EntryPoint = "main";
	psDesc.Bytecode   = psBytecode.data();
	psDesc.ByteSize   = psBytecode.size();

	GpuShader vsShader = m_GpuDevice->CreateShader(vsDesc);
	GpuShader psShader = m_GpuDevice->CreateShader(psDesc);

	// ---- Binding layout: LUT (t0) + sampler (s0) ----
	BindingLayoutDesc layoutDesc;
	layoutDesc.Items = {
		BindingLayoutItem::Texture(0, ShaderStage::Pixel),  // t0 — LUT SRV
		BindingLayoutItem::Sampler(0, ShaderStage::Pixel),  // s0 — LinearSampler
	};
	m_BlitBindingLayout = m_GpuDevice->CreateBindingLayout(layoutDesc);

	// ---- Sampler ----
	SamplerDesc samplerDesc;
	samplerDesc.MinFilter = Filter::Linear;
	samplerDesc.MagFilter = Filter::Linear;
	samplerDesc.AddressU  = AddressMode::Clamp;
	samplerDesc.AddressV  = AddressMode::Clamp;
	m_LinearSampler = m_GpuDevice->CreateSampler(samplerDesc);

	// ---- Binding set: bind the sky LUT as SRV ----
	BindingSetDesc setDesc;
	setDesc.Items = {
		BindingItem::Texture(0, m_SkyPass.TransmittanceLut()),
		BindingItem::Sampler(0, m_LinearSampler),
	};
	m_BlitBindingSet = m_GpuDevice->CreateBindingSet(setDesc, m_BlitBindingLayout);

	// ---- Pipeline ----
	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.VS             = vsShader;
	pipelineDesc.PS             = psShader;
	pipelineDesc.PrimType       = PrimitiveType::TriangleList;
	pipelineDesc.BindingLayouts = { m_BlitBindingLayout };
	// No input layout — positions are generated in the VS from SV_VertexID.
	pipelineDesc.Rasterizer.CullMode = CullMode::None;
	pipelineDesc.DepthStencil.DepthTestEnable = false;

	CORE_ASSERT(!m_Framebuffers.empty(), "Framebuffers must exist before CreateBlitPipeline");
	m_BlitPipeline = m_GpuDevice->CreateGraphicsPipeline(pipelineDesc, m_Framebuffers[0]);

	m_GpuDevice->DestroyShader(vsShader);
	m_GpuDevice->DestroyShader(psShader);
}

void AppLayer::DestroyBlitPipeline()
{
	m_GpuDevice->DestroyGraphicsPipeline(m_BlitPipeline);
	m_GpuDevice->DestroyBindingSet(m_BlitBindingSet);
	m_GpuDevice->DestroyBindingLayout(m_BlitBindingLayout);
	m_GpuDevice->DestroySampler(m_LinearSampler);
}

// -------------------------------------------------------------------------
// Layer lifecycle
// -------------------------------------------------------------------------

void AppLayer::OnAttach()
{
	WindowDesc desc;
	desc.title  = "Sky Transmittance TestBed";
	desc.width  = 1280;
	desc.height = 720;
	m_WindowSystem = Engine::Get().GetSubmodule<GLFWWindowSystem>();
	m_WindowHandle = m_WindowSystem->OpenWindow(desc);

	auto* glfwWS = dynamic_cast<GLFWWindowSystem*>(m_WindowSystem);
	m_GlfwWindow  = glfwWS->GetGLFWWindow(m_WindowHandle);

	m_RenderDevice = std::make_unique<D3D12Device>(glfwWS->GetNativeHandle(m_WindowHandle));

	m_GpuDevice = m_RenderDevice->CreateDevice();

	auto extent = m_WindowSystem->GetExtent(m_WindowHandle);
	m_RenderDevice->CreateSwapchain((uint32_t)extent.x, (uint32_t)extent.y);

	m_CommandContext = m_GpuDevice->CreateCommandContext();
	m_FrameGraph     = std::make_unique<FrameGraph>();

	// ---- Init sky transmittance pass ----
	// Dispatches the compute shader once; the LUT is ready before the first frame.
	SkyLutsPass::Config skyConfig;
	skyConfig.Backend = RenderBackend::D3D12;

	m_CommandContext->Open();
	m_SkyPass.Init(m_GpuDevice, m_CommandContext.get(), skyConfig);
	m_CommandContext->Close();
	m_GpuDevice->ExecuteCommandContext(*m_CommandContext);
	m_GpuDevice->WaitForIdle();

	CreateFramebuffers();
	CreateBlitPipeline();
}

void AppLayer::OnDetach()
{
	if (m_GpuDevice)
		m_GpuDevice->WaitForIdle();

	DestroyBlitPipeline();
	m_SkyPass.Shutdown(m_GpuDevice);
	DestroyFramebuffers();

	m_CommandContext.reset();
	m_FrameGraph.reset();
	m_GpuDevice    = nullptr;
	m_RenderDevice.reset();

	m_WindowSystem->CloseWindow(m_WindowHandle);
}

void AppLayer::OnUpdate(float /*deltaTime*/)
{
	if (m_WindowSystem->ShouldClose(m_WindowHandle))
	{
		Engine::Get().RequestStop();
		return;
	}

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

	TextureDesc bbDesc;
	bbDesc.Width     = m_Width;
	bbDesc.Height    = m_Height;
	bbDesc.Format    = GpuFormat::BGRA8_UNORM;
	bbDesc.DebugName = "Backbuffer";
	RGMutableTextureHandle backbuffer = m_FrameGraph->ImportMutableTexture(
		m_GpuDevice->GetBackBufferTextures()[imageIdx],
		bbDesc,
		m_FrameCount++ == 0 ? ResourceLayout::Undefined : ResourceLayout::Present);

	// ---- Import the pre-computed sky LUT ----
	// The LUT was dispatched once during Init. Add() just imports it as
	// a readable FrameGraph resource so the blit pass can declare a dependency.
	SkyLutsPass::Output skyOut = m_SkyPass.Add(*m_FrameGraph);

	// ---- Fullscreen blit — display the LUT ----
	GpuFramebuffer fb = m_Framebuffers[imageIdx];

	struct BlitPassData
	{
		RGTextureHandle        Lut;
		RGMutableTextureHandle Target;
	};

	m_FrameGraph->AddCallbackPass<BlitPassData>(
		"BlitLut",
		[&](PassBuilder& builder, BlitPassData& data)
		{
			// Declare the read so the graph can sequence barriers correctly.
			data.Lut    = builder.ReadTexture(skyOut.TransmittanceLut);
			data.Target = builder.WriteTexture(backbuffer);
		},
		[fb, this](const BlitPassData& data, const RenderPassResources& res, ICommandContext* cmd)
		{
			RenderPassDesc passDesc;
			passDesc.Framebuffer = fb;
			passDesc.ClearColor  = true;
			passDesc.ColorValue  = ClearValue{ 0.0f, 0.0f, 0.0f, 1.0f };
			cmd->BeginRenderPass(passDesc);

			cmd->SetGraphicsPipeline(m_BlitPipeline);
			cmd->SetBindingSet(m_BlitBindingSet, 0);
			cmd->SetViewport(0.0f, 0.0f, (float)m_Width, (float)m_Height);
			cmd->SetScissor(0, 0, m_Width, m_Height);

			DrawArgs drawArgs;
			drawArgs.VertexCount   = 3; // fullscreen triangle
			drawArgs.InstanceCount = 1;
			cmd->Draw(drawArgs);

			cmd->EndRenderPass();
			cmd->TransitionTexture(res.GetTexture(data.Target), ResourceLayout::Present);
		});

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
			m_Width        = e.GetWidth();
			m_Height       = e.GetHeight();
			m_PendingResize = true;
		}
		return false;
	});
}
