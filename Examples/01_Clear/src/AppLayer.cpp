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

// -------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------

ClearValue HsvToRgb(float h, float s, float v)
{
	float r = 0.0f, g = 0.0f, b = 0.0f;

	int i = static_cast<int>(std::floor(h * 6));
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);

	switch (i % 6)
	{
	case 0: r = v, g = t, b = p; break;
	case 1: r = q, g = v, b = p; break;
	case 2: r = p, g = v, b = t; break;
	case 3: r = p, g = q, b = v; break;
	case 4: r = t, g = p, b = v; break;
	case 5: r = v, g = p, b = q; break;
	default: break;
	}

	return { r, g, b, 1.0f };
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
}

void AppLayer::OnDetach()
{
	if (m_GpuDevice)
		m_GpuDevice->WaitForIdle();

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

	ClearValue clearColor = HsvToRgb(m_Hue == 1.0f ? m_Hue = 0 : m_Hue += 0.1f * deltaTime, 1.0f, 1.0f);

	struct ClearPassData { RGMutableTextureHandle target; };
	m_FrameGraph->AddCallbackPass<ClearPassData>(
		"Clear",
		[&](PassBuilder& builder, ClearPassData& data)
		{
			data.target = builder.WriteTexture(backbuffer);
		},
		[fb, clearColor](const ClearPassData& data, const RenderPassResources& res, ICommandContext* cmd)
		{
			RenderPassDesc passDesc;
			passDesc.framebuffer = fb;
			passDesc.clearDepth  = true;
			passDesc.depthValue  = 1.0f;
			passDesc.clearColor  = true;
			passDesc.colorValue = clearColor;
			cmd->BeginRenderPass(passDesc);
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
