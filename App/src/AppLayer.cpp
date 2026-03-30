// Provide storage for NVRHI's Vulkan-HPP dynamic dispatch loader.
// MUST appear before any other Vulkan include so that vulkan_hpp_macros.hpp
// processes VULKAN_HPP_DISPATCH_LOADER_DYNAMIC before the C vulkan.h guards
// cause VULKAN_HPP_DEFAULT_DISPATCHER to be left undefined.
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "AppLayer.h"

#include <Engine.h>
#include <Platform/GLFWWindow.h>

#include <iostream>
#include <fstream>

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

std::vector<uint8_t> AppLayer::LoadSPIRV(const char* path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		std::cerr << "[AppLayer] Cannot open shader: " << path << "\n";
		return {};
	}
	auto size = static_cast<size_t>(file.tellg());
	std::vector<uint8_t> data(size);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), size);
	return data;
}

void AppLayer::CreatePipelineAndFramebuffers()
{
	m_Framebuffers.clear();

	const auto& backBuffers = m_RenderDevice->GetBackBuffers();
	for (const auto& bb : backBuffers)
	{
		nvrhi::FramebufferDesc fbDesc;
		fbDesc.addColorAttachment(bb);
		m_Framebuffers.push_back(m_NvrhiDevice->createFramebuffer(fbDesc));
	}

	nvrhi::GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.VS       = m_VertShader;
	pipelineDesc.PS       = m_FragShader;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	pipelineDesc.renderState.rasterState.setCullNone();
	pipelineDesc.renderState.depthStencilState.depthTestEnable  = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;

	m_Pipeline = m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, m_Framebuffers[0]);
}

// -------------------------------------------------------------------------
// Layer lifecycle
// -------------------------------------------------------------------------

void AppLayer::OnAttach()
{
	// Open window
	WindowDesc desc;
	desc.title  = "Triangle Demo";
	desc.width  = 1280;
	desc.height = 720;
	m_WindowHandle = m_WindowSystem.OpenWindow(desc);

	// Get raw GLFWwindow* for device creation
	auto& glfwWS   = static_cast<GLFWWindowSystem&>(m_WindowSystem);
	GLFWwindow* win = glfwWS.GetGLFWWindow(m_WindowHandle);

	// Create device and swapchain
	m_RenderDevice = std::make_unique<VulkanDevice>(win);
	m_NvrhiDevice  = m_RenderDevice->CreateDevice();

	auto extent = m_WindowSystem.GetExtent(m_WindowHandle);
	m_RenderDevice->CreateSwapchain(extent.x, extent.y);

	// Load shaders — paths are relative to VS_DEBUGGER_WORKING_DIRECTORY (App/)
	auto vertSpv = LoadSPIRV("shaders/triangle_vert.spv");
	auto fragSpv = LoadSPIRV("shaders/triangle_frag.spv");

	nvrhi::ShaderDesc vertDesc;
	vertDesc.shaderType = nvrhi::ShaderType::Vertex;
	vertDesc.entryName  = "main";
	m_VertShader = m_NvrhiDevice->createShader(vertDesc, vertSpv.data(), vertSpv.size());

	nvrhi::ShaderDesc fragDesc;
	fragDesc.shaderType = nvrhi::ShaderType::Pixel;
	fragDesc.entryName  = "main";
	m_FragShader = m_NvrhiDevice->createShader(fragDesc, fragSpv.data(), fragSpv.size());

	CreatePipelineAndFramebuffers();

	m_CommandList = m_NvrhiDevice->createCommandList();
}

void AppLayer::OnDetach()
{
	if (m_NvrhiDevice)
		m_NvrhiDevice->waitForIdle();

	// Release NVRHI resources before destroying the device
	m_CommandList = nullptr;
	m_Pipeline    = nullptr;
	m_Framebuffers.clear();
	m_VertShader  = nullptr;
	m_FragShader  = nullptr;

	// Destructor calls DestroyDevice internally
	m_RenderDevice.reset();

	m_WindowSystem.CloseWindow(m_WindowHandle);
}

void AppLayer::OnUpdate(float /*deltaTime*/)
{
	if (m_WindowSystem.ShouldClose(m_WindowHandle))
	{
		Engine::Get().RequestStop();
		return;
	}

	// ---- Begin frame (acquires swapchain image) ----
	m_RenderDevice->BeginFrame();
	uint32_t imageIdx = m_RenderDevice->GetCurrentImageIndex();

	auto& fb      = m_Framebuffers[imageIdx];
	auto  fbInfo  = fb->getFramebufferInfo();

	// ---- Record ----
	m_CommandList->open();

	// Clear the back buffer to a dark grey
	m_CommandList->clearTextureFloat(
		m_RenderDevice->GetBackBuffers()[imageIdx],
		nvrhi::AllSubresources,
		nvrhi::Color(0.1f, 0.1f, 0.1f, 1.0f)
	);

	// Draw the triangle (positions baked into the vertex shader via gl_VertexIndex)
	nvrhi::GraphicsState state;
	state.pipeline    = m_Pipeline;
	state.framebuffer = fb;
	state.viewport.addViewportAndScissorRect(fbInfo.getViewport());
	m_CommandList->setGraphicsState(state);

	nvrhi::DrawArguments drawArgs;
	drawArgs.vertexCount = 3;
	m_CommandList->draw(drawArgs);

	m_CommandList->close();

	// ---- Submit and present ----
	m_NvrhiDevice->executeCommandList(m_CommandList);
	m_RenderDevice->Present();
}

void AppLayer::OnEvent(Event& /*event*/)
{
	// Window-close is detected via ShouldClose in OnUpdate.
	// Add event handling here as needed.
}
