#include "AppLayer.h"
#define NOMINMAX

#ifdef CORE_VULKAN
// Provide storage for NVRHI's Vulkan-HPP dynamic dispatch loader.
// MUST appear before any other Vulkan include so that vulkan_hpp_macros.hpp
// processes VULKAN_HPP_DISPATCH_LOADER_DYNAMIC before the C vulkan.h guards
// cause VULKAN_HPP_DEFAULT_DISPATCHER to be left undefined.
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#include <Graphics/VulkanDevice.h>
#endif

#ifdef CORE_DX12
#include <Graphics/D3D12Device.h>
#endif

#include <Engine.h>
#include <Window/GLFWWindow.h>
#include <Events/ApplicationEvents.h>
#include <Events/KeyEvents.h>
#include <Events/MouseEvents.h>

#include <GLFW/glfw3.h>

#include <Math/Vector3.h>
#include <Math/Matrix4x4.h>
#include <Util/Log.h>

#include <algorithm>
#include <cmath>
#include <fstream>


struct Vertex {
	float position[3];
	float color[3];
};

// 24 vertices (4 per face) so each face can have its own colour
static const Vertex vertices[] = {
	// +Z
	{{-0.5f, -0.5f,  0.5f}, {1,0,0}}, {{ 0.5f, -0.5f,  0.5f}, {1,0,0}},
	{{ 0.5f,  0.5f,  0.5f}, {1,0,0}}, {{-0.5f,  0.5f,  0.5f}, {1,0,0}},
	// -Z
	{{ 0.5f, -0.5f, -0.5f}, {0,1,0}}, {{-0.5f, -0.5f, -0.5f}, {0,1,0}},
	{{-0.5f,  0.5f, -0.5f}, {0,1,0}}, {{ 0.5f,  0.5f, -0.5f}, {0,1,0}},
	// +X
	{{ 0.5f, -0.5f,  0.5f}, {0,0,1}}, {{ 0.5f, -0.5f, -0.5f}, {0,0,1}},
	{{ 0.5f,  0.5f, -0.5f}, {0,0,1}}, {{ 0.5f,  0.5f,  0.5f}, {0,0,1}},
	// -X
	{{-0.5f, -0.5f, -0.5f}, {1,1,0}}, {{-0.5f, -0.5f,  0.5f}, {1,1,0}},
	{{-0.5f,  0.5f,  0.5f}, {1,1,0}}, {{-0.5f,  0.5f, -0.5f}, {1,1,0}},
	// +Y
	{{-0.5f,  0.5f,  0.5f}, {0,1,1}}, {{ 0.5f,  0.5f,  0.5f}, {0,1,1}},
	{{ 0.5f,  0.5f, -0.5f}, {0,1,1}}, {{-0.5f,  0.5f, -0.5f}, {0,1,1}},
	// -Y
	{{-0.5f, -0.5f, -0.5f}, {1,0,1}}, {{ 0.5f, -0.5f, -0.5f}, {1,0,1}},
	{{ 0.5f, -0.5f,  0.5f}, {1,0,1}}, {{-0.5f, -0.5f,  0.5f}, {1,0,1}},
};

static const uint32_t indices[] =
{
	0, 1, 2, 2, 3, 0,       // Front
	4, 5, 6, 6, 7, 4,       // Back
	8, 9, 10, 10, 11, 8,    // Left
	12, 13, 14, 14, 15, 12, // Right
	16, 17, 18, 18, 19, 16, // Bottom
	20, 21, 22, 22, 23, 20  // Top
};

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

std::vector<uint8_t> AppLayer::LoadSPIRV(const char* path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		APP_ERROR("Cannot open shader: {}", path);
		return {};
	}
	auto size = static_cast<size_t>(file.tellg());
	std::vector<uint8_t> data(size);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(data.data()), size);
	return data;
}

std::unique_ptr<IRenderDevice> AppLayer::MakeRenderDevice(RenderBackend backend)
{
#ifdef CORE_VULKAN
	if (backend == RenderBackend::Vulkan)
		return std::make_unique<VulkanDevice>(m_GlfwWindow);
#endif
#ifdef CORE_DX12
	if (backend == RenderBackend::D3D12)
		return std::make_unique<D3D12Device>(m_WindowSystem.GetNativeHandle(m_WindowHandle));
#endif
	APP_FATAL("Requested backend is not compiled in");
	abort();
}

void AppLayer::CreatePipelineAndFramebuffers()
{
	// Binding layout — vertex shader only, constant buffer at slot 0
	BindingLayoutDesc layoutDesc;
	layoutDesc.items = { BindingLayoutItem::ConstantBuffer(0, ShaderStage::Vertex) };
	m_BindingLayout = m_GpuDevice->CreateBindingLayout(layoutDesc);

	// Input layout — two attributes from the same vertex buffer
	std::vector<VertexAttributeDesc> attribs = {
		{ "POSITION", GpuFormat::RGB32_FLOAT, 0, (uint32_t)offsetof(Vertex, position), (uint32_t)sizeof(Vertex) },
		{ "COLOR",    GpuFormat::RGB32_FLOAT, 0, (uint32_t)offsetof(Vertex, color),    (uint32_t)sizeof(Vertex) },
	};
	m_InputLayout = m_GpuDevice->CreateInputLayout(attribs, m_VertShader);

	m_Framebuffers.clear();

	// Depth buffer sized to the current window
	auto extent = m_WindowSystem.GetExtent(m_WindowHandle);
	TextureDesc depthDesc;
	depthDesc.width     = (uint32_t)extent.x;
	depthDesc.height    = (uint32_t)extent.y;
	depthDesc.format    = GpuFormat::D32;
	depthDesc.dimension = TextureDimension::Texture2D;
	depthDesc.usage     = TextureUsage::DepthStencil;
	depthDesc.debugName = "Depth Buffer";
	m_DepthBuffer = m_GpuDevice->CreateTexture(depthDesc);

	const auto& backBuffers = m_GpuDevice->GetBackBufferTextures();
	for (const auto& bb : backBuffers)
	{
		FramebufferDesc fbDesc;
		fbDesc.colorAttachments.push_back({ bb, 0, 0 });
		fbDesc.depthAttachment = { m_DepthBuffer, 0, 0 };
		m_Framebuffers.push_back(m_GpuDevice->CreateFramebuffer(fbDesc));
	}

	// Graphics pipeline
	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.vs                            = m_VertShader;
	pipelineDesc.ps                            = m_FragShader;
	pipelineDesc.inputLayout                   = m_InputLayout;
	pipelineDesc.primType                      = PrimitiveType::TriangleList;
	pipelineDesc.rasterizer.frontCCW           = true;
	pipelineDesc.depthStencil.depthTestEnable  = true;
	pipelineDesc.depthStencil.depthWriteEnable = true;
	pipelineDesc.depthStencil.depthFunc        = ComparisonFunc::Less;
	pipelineDesc.bindingLayouts                = { m_BindingLayout };
	m_Pipeline = m_GpuDevice->CreateGraphicsPipeline(pipelineDesc, m_Framebuffers[0]);

	// Binding set — wires the constant buffer into slot 0
	BindingSetDesc setDesc;
	setDesc.items = { BindingItem::ConstantBuffer(0, m_ConstantBuffer) };
	m_BindingSet = m_GpuDevice->CreateBindingSet(setDesc, m_BindingLayout);
}

void AppLayer::CreateBuffers()
{
	// Vertex buffer
	BufferDesc vbDesc;
	vbDesc.byteSize  = sizeof(vertices);
	vbDesc.usage     = BufferUsage::Vertex;
	vbDesc.debugName = "Vertex Buffer";
	m_VertexBuffer = m_GpuDevice->CreateBuffer(vbDesc);

	// Index buffer
	BufferDesc ibDesc;
	ibDesc.byteSize  = sizeof(indices);
	ibDesc.usage     = BufferUsage::Index;
	ibDesc.debugName = "Index Buffer";
	m_IndexBuffer = m_GpuDevice->CreateBuffer(ibDesc);

	// Upload geometry via a one-shot command context
	m_CommandContext->Open();
	m_CommandContext->WriteBuffer(m_VertexBuffer, vertices, sizeof(vertices));
	m_CommandContext->WriteBuffer(m_IndexBuffer,  indices,  sizeof(indices));
	m_CommandContext->Close();
	m_GpuDevice->ExecuteCommandContext(*m_CommandContext);

	// Constant buffer (MVP matrix, updated every frame)
	BufferDesc cbDesc;
	cbDesc.byteSize  = sizeof(float) * 16;
	cbDesc.usage     = BufferUsage::Constant;
	cbDesc.debugName = "Constant Buffer";
	m_ConstantBuffer = m_GpuDevice->CreateBuffer(cbDesc);
}

void AppLayer::InitGpuResources()
{
	// Load the correct shader format for the active backend
	std::vector<uint8_t> vertBytes, fragBytes;
#ifdef CORE_VULKAN
	if (m_ActiveBackend == RenderBackend::Vulkan)
	{
		vertBytes = LoadSPIRV("shaders/triangle_vert.spv");
		fragBytes = LoadSPIRV("shaders/triangle_frag.spv");
	}
#endif
#ifdef CORE_DX12
	if (m_ActiveBackend == RenderBackend::D3D12)
	{
		vertBytes = LoadSPIRV("shaders/triangle_vert.cso");
		fragBytes = LoadSPIRV("shaders/triangle_frag.cso");
	}
#endif

	ShaderDesc vertDesc;
	vertDesc.stage      = ShaderStage::Vertex;
	vertDesc.bytecode   = vertBytes.data();
	vertDesc.byteSize   = vertBytes.size();
	vertDesc.entryPoint = "main";
	m_VertShader = m_GpuDevice->CreateShader(vertDesc);

	ShaderDesc fragDesc;
	fragDesc.stage      = ShaderStage::Pixel;
	fragDesc.bytecode   = fragBytes.data();
	fragDesc.byteSize   = fragBytes.size();
	fragDesc.entryPoint = "main";
	m_FragShader = m_GpuDevice->CreateShader(fragDesc);

	m_CommandContext = m_GpuDevice->CreateCommandContext();

	CreateBuffers();
	CreatePipelineAndFramebuffers();
}

void AppLayer::DestroyGpuResources()
{
	m_CommandContext.reset();
	m_GpuDevice->DestroyBindingSet(m_BindingSet);        m_BindingSet     = {};
	m_GpuDevice->DestroyGraphicsPipeline(m_Pipeline);    m_Pipeline       = {};
	for (auto fb : m_Framebuffers)
		m_GpuDevice->DestroyFramebuffer(fb);
	m_Framebuffers.clear();
	m_GpuDevice->DestroyTexture(m_DepthBuffer);          m_DepthBuffer    = {};
	m_GpuDevice->DestroyInputLayout(m_InputLayout);      m_InputLayout    = {};
	m_GpuDevice->DestroyBindingLayout(m_BindingLayout);  m_BindingLayout  = {};
	m_GpuDevice->DestroyBuffer(m_ConstantBuffer);        m_ConstantBuffer = {};
	m_GpuDevice->DestroyBuffer(m_IndexBuffer);           m_IndexBuffer    = {};
	m_GpuDevice->DestroyBuffer(m_VertexBuffer);          m_VertexBuffer   = {};
	m_GpuDevice->DestroyShader(m_FragShader);            m_FragShader     = {};
	m_GpuDevice->DestroyShader(m_VertShader);            m_VertShader     = {};
}

void AppLayer::SwitchBackend(RenderBackend next)
{
	m_GpuDevice->WaitForIdle();
	m_GpuDevice->RunGarbageCollection();
	DestroyGpuResources();

	GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(m_WindowSystem.GetNativeHandle(m_WindowHandle));
	glfwWaitEvents();

	// m_GpuDevice is non-owning — clear the pointer before destroying the owner
	m_GpuDevice = nullptr;
	m_RenderDevice.reset();

	m_ActiveBackend = next;
	m_RenderDevice = MakeRenderDevice(next);
	m_GpuDevice    = m_RenderDevice->CreateDevice();

	auto extent = m_WindowSystem.GetExtent(m_WindowHandle);
	m_RenderDevice->CreateSwapchain((uint32_t)extent.x, (uint32_t)extent.y);

	// Absorb any queued resize — CreateSwapchain already used the current size
	m_PendingResize = false;

	InitGpuResources();

	APP_INFO("Backend switched to {}", next == RenderBackend::Vulkan ? "Vulkan" : "D3D12");
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

	// Always GLFW, so we can always get the raw window for cursor control.
	auto& glfwWS = static_cast<GLFWWindowSystem&>(m_WindowSystem);
	m_GlfwWindow = glfwWS.GetGLFWWindow(m_WindowHandle);

	// Pick the first compiled-in backend as the default
#if defined(CORE_VULKAN)
	m_ActiveBackend = RenderBackend::Vulkan;
#elif defined(CORE_DX12)
	m_ActiveBackend = RenderBackend::D3D12;
#endif

	m_RenderDevice = MakeRenderDevice(m_ActiveBackend);
	m_GpuDevice    = m_RenderDevice->CreateDevice();

	auto extent = m_WindowSystem.GetExtent(m_WindowHandle);
	m_RenderDevice->CreateSwapchain((uint32_t)extent.x, (uint32_t)extent.y);

	InitGpuResources();
}

void AppLayer::OnDetach()
{
	if (m_GpuDevice)
		m_GpuDevice->WaitForIdle();

	DestroyGpuResources();

	m_GpuDevice = nullptr;
	m_RenderDevice.reset();

	m_WindowSystem.CloseWindow(m_WindowHandle);
}

void AppLayer::OnUpdate(float deltaTime)
{
	if (m_WindowSystem.ShouldClose(m_WindowHandle))
	{
		Engine::Get().RequestStop();
		return;
	}

	// ---- Backend hot-swap (` key, only when both backends are compiled) ----
#if defined(CORE_VULKAN) && defined(CORE_DX12)
	if (m_PendingBackendSwap)
	{
		m_PendingBackendSwap = false;
		RenderBackend next = (m_ActiveBackend == RenderBackend::Vulkan)
		                   ? RenderBackend::D3D12
		                   : RenderBackend::Vulkan;
		SwitchBackend(next);
		return; // skip rendering this frame — resources just rebuilt
	}
#endif

	// ---- Swapchain resize ----
	if (m_PendingResize)
	{
		m_GpuDevice->RunGarbageCollection();
		// Release resources that hold back-buffer references
		m_GpuDevice->DestroyBindingSet(m_BindingSet);      m_BindingSet = {};
		m_GpuDevice->DestroyGraphicsPipeline(m_Pipeline);  m_Pipeline   = {};
		for (auto fb : m_Framebuffers)
			m_GpuDevice->DestroyFramebuffer(fb);
		m_Framebuffers.clear();
		m_GpuDevice->DestroyTexture(m_DepthBuffer);        m_DepthBuffer = {};
		m_RenderDevice->RecreateSwapchain(m_Width, m_Height);
		CreatePipelineAndFramebuffers();
		m_PendingResize = false;
	}

	// ---- Begin frame (acquires swapchain image) ----
	m_RenderDevice->BeginFrame();

	// Retire command buffers whose GPU work is complete. Must be called every
	// frame; without it, NVRHI's m_CommandBuffersInFlight grows unboundedly.
	m_GpuDevice->RunGarbageCollection();

	uint32_t imageIdx = m_RenderDevice->GetCurrentImageIndex();
	GpuFramebuffer fb = m_Framebuffers[imageIdx];
	auto [fbWidth, fbHeight] = m_GpuDevice->GetFramebufferSize(fb);

	// ---- Camera basis ----
	constexpr float kDeg2Rad = 3.14159265358979f / 180.0f;
	Vector3 front;
	front.x = std::cosf(m_CamYaw * kDeg2Rad) * std::cosf(m_CamPitch * kDeg2Rad);
	front.y = std::sinf(m_CamPitch * kDeg2Rad);
	front.z = std::sinf(m_CamYaw * kDeg2Rad) * std::cosf(m_CamPitch * kDeg2Rad);
	front = Vector3::Normalize(front);
	Vector3 right = Vector3::Normalize(Vector3::Cross(front, Vector3(0.f, 1.f, 0.f)));

	// ---- Quake-style velocity movement ----
	Vector3 wishDir;
	if (m_Keys[GLFW_KEY_W]) wishDir += front;
	if (m_Keys[GLFW_KEY_S]) wishDir -= front;
	if (m_Keys[GLFW_KEY_D]) wishDir += right;
	if (m_Keys[GLFW_KEY_A]) wishDir -= right;
	if (m_Keys[GLFW_KEY_E]) wishDir.y += 1.f;
	if (m_Keys[GLFW_KEY_Q]) wishDir.y -= 1.f;

	float wishSpeed = m_MoveSpeed * (m_Keys[GLFW_KEY_LEFT_SHIFT] ? 3.f : 1.f);
	if (Vector3::Magnitude(wishDir) > 0.f)
		wishDir = Vector3::Normalize(wishDir);

	// Friction — exponential decay, frame-rate independent
	constexpr float friction = 7.f;
	float speed = Vector3::Magnitude(m_CamVel);
	if (speed > 0.001f)
		m_CamVel *= std::max(speed - friction * speed * deltaTime, 0.f) / speed;

	// Acceleration — only add velocity in the wish direction up to wishSpeed
	constexpr float accel = 25.f;
	float currentSpeed = Vector3::Dot(m_CamVel, wishDir);
	float addSpeed = wishSpeed - currentSpeed;
	if (addSpeed > 0.f)
		m_CamVel += wishDir * std::min(accel * deltaTime * wishSpeed, addSpeed);

	m_CamPos += m_CamVel * deltaTime;

	// Row-vector convention: mvp = model * view * projection.
	// My row-major storage produces the same bytes as GLM's column-major,
	// so the existing shaders work unchanged.
	Matrix4x4 model      = Matrix4x4::Identity();
	Matrix4x4 view       = Matrix4x4::LookAt(m_CamPos, m_CamPos + front, Vector3(0.f, 1.f, 0.f));
	Matrix4x4 projection = Matrix4x4::Perspective(60.0f, (float)fbWidth / (float)fbHeight, 0.1f, 500.0f);
	projection[1][1] *= m_RenderDevice->GetClipSpaceYSign();
	Matrix4x4 mvp = model * view * projection;

	// ---- Record ----
	m_CommandContext->Open();

	m_CommandContext->WriteBuffer(m_ConstantBuffer, &mvp, sizeof(mvp));

	RenderPassDesc passDesc;
	passDesc.framebuffer = fb;
	passDesc.clearColor  = true;
	passDesc.colorValue  = { 0.1f, 0.1f, 0.1f, 1.0f };
	passDesc.clearDepth  = true;
	passDesc.depthValue  = 1.0f;
	m_CommandContext->BeginRenderPass(passDesc);

	m_CommandContext->SetGraphicsPipeline(m_Pipeline);

	Viewport vp;
	vp.x = 0.f;  vp.y = 0.f;
	vp.width    = (float)fbWidth;
	vp.height   = (float)fbHeight;
	vp.minDepth = 0.f;  vp.maxDepth = 1.f;
	m_CommandContext->SetViewport(vp);

	ScissorRect sr;
	sr.x = 0;  sr.y = 0;
	sr.width  = (int)fbWidth;
	sr.height = (int)fbHeight;
	m_CommandContext->SetScissor(sr);

	m_CommandContext->SetVertexBuffer(0, m_VertexBuffer);
	m_CommandContext->SetIndexBuffer(m_IndexBuffer, GpuFormat::R32_UINT);
	m_CommandContext->SetBindingSet(m_BindingSet);

	DrawIndexedArgs drawArgs;
	drawArgs.indexCount = 36;
	m_CommandContext->DrawIndexed(drawArgs);

	m_CommandContext->EndRenderPass();
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
			m_Width = e.GetWidth();
			m_Height = e.GetHeight();
			m_PendingResize = true;
		}
		return false;
	});

	d.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)
	{
		int k = e.GetKeyCode();
		if (k >= 0 && k < 512) m_Keys[k] = true;
#if defined(CORE_VULKAN) && defined(CORE_DX12)
		if (k == GLFW_KEY_GRAVE_ACCENT)
			m_PendingBackendSwap = true;
#endif
		return false;
	});

	d.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& e)
	{
		int k = e.GetKeyCode();
		if (k >= 0 && k < 512) m_Keys[k] = false;
		return false;
	});

	d.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() == GLFW_MOUSE_BUTTON_RIGHT)
		{
			m_RmbHeld    = true;
			m_FirstMouse = true;
			glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		return false;
	});

	d.Dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& e)
	{
		if (e.GetMouseButton() == GLFW_MOUSE_BUTTON_RIGHT)
		{
			m_RmbHeld = false;
			glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		return false;
	});

	d.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e)
	{
		if (!m_RmbHeld) return false;

		if (m_FirstMouse)
		{
			m_LastMouseX = e.GetX();
			m_LastMouseY = e.GetY();
			m_FirstMouse = false;
			return false;
		}

		float dx = (e.GetX() - m_LastMouseX) * m_MouseSens;
		float dy = (e.GetY() - m_LastMouseY) * m_MouseSens; // screen down = pitch up (inverted)
		m_LastMouseX = e.GetX();
		m_LastMouseY = e.GetY();

		m_CamYaw   += dx;
		m_CamPitch  = std::clamp(m_CamPitch + dy, -89.f, 89.f);
		return false;
	});

	d.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e)
	{
		if (!m_RmbHeld) return false;
		m_MoveSpeed = std::clamp(m_MoveSpeed + e.GetYOffset() * 0.5f, 0.5f, 50.f);
		return false;
	});
}
