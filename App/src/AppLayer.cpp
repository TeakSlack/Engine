#include "AppLayer.h"
#define NOMINMAX

#ifdef COMPILE_WITH_VULKAN
// Provide storage for NVRHI's Vulkan-HPP dynamic dispatch loader.
// MUST appear before any other Vulkan include so that vulkan_hpp_macros.hpp
// processes VULKAN_HPP_DISPATCH_LOADER_DYNAMIC before the C vulkan.h guards
// cause VULKAN_HPP_DEFAULT_DISPATCHER to be left undefined.
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#include <Graphics/VulkanDevice.h>
#endif

#ifdef COMPILE_WITH_DX12
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
#include <Asset/AssetManager.h>
#include <ECS/SceneManager.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

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
#ifdef COMPILE_WITH_VULKAN
	if (backend == RenderBackend::Vulkan)
		return std::make_unique<VulkanDevice>(m_GlfwWindow);
#endif
#ifdef COMPILE_WITH_DX12
	if (backend == RenderBackend::D3D12)
		return std::make_unique<D3D12Device>(m_WindowSystem.GetNativeHandle(m_WindowHandle));
#endif
	APP_FATAL("Requested backend is not compiled in");
	abort();
}

void AppLayer::CreatePipelineAndFramebuffers()
{
	// Per-material binding sets reference m_BindingLayout. Destroy them before
	// recreating the layout so they don't hold a stale handle.
	for (auto& [id, set] : m_MaterialSets)
		m_GpuDevice->DestroyBindingSet(set);
	m_MaterialSets.clear();

	// Binding layout:
	//   b0 (VS) — PerObject: World, ViewProjection, CameraPosition
	//   t0 (PS) — albedo
	//   t1 (PS) — normal map       (binding 3, would collide with s0 — sampler moved to s2)
	//   t2 (PS) — metallic-roughness
	//   s2 (PS) — shared sampler   (binding 5, avoids t1 collision)
	BindingLayoutDesc layoutDesc;
	layoutDesc.items = {
		BindingLayoutItem::ConstantBuffer(0, ShaderStage::Vertex | ShaderStage::Pixel),
		BindingLayoutItem::Texture(0,         ShaderStage::Pixel),
		BindingLayoutItem::Texture(1,         ShaderStage::Pixel),
		BindingLayoutItem::Texture(2,         ShaderStage::Pixel),
		BindingLayoutItem::Sampler(2,         ShaderStage::Pixel),
	};
	m_BindingLayout = m_GpuDevice->CreateBindingLayout(layoutDesc);

	uint32_t vertexStride = (uint32_t)sizeof(Vertex);

	std::vector<VertexAttributeDesc> attribs = {
		{ "POSITION", GpuFormat::RGB32_FLOAT, 0, (uint32_t)offsetof(Vertex, Position), vertexStride },
		{ "NORMAL",   GpuFormat::RGB32_FLOAT, 0, (uint32_t)offsetof(Vertex, Normal),   vertexStride },
		{ "TEXCOORD", GpuFormat::RG32_FLOAT,  0, (uint32_t)offsetof(Vertex, TexCoord), vertexStride },
		{ "TANGENT",  GpuFormat::RGBA32_FLOAT, 0, (uint32_t)offsetof(Vertex, Tangent),  vertexStride },
	};
	m_InputLayout = m_GpuDevice->CreateInputLayout(attribs, m_VertShader);

	m_Framebuffers.clear();

	// Depth buffer sized to the current window
	auto extent = m_WindowSystem.GetExtent(m_WindowHandle);
	TextureDesc depthDesc;
	depthDesc.width                = (uint32_t)extent.x;
	depthDesc.height               = (uint32_t)extent.y;
	depthDesc.format               = GpuFormat::D32;
	depthDesc.dimension            = TextureDimension::Texture2D;
	depthDesc.usage                = TextureUsage::DepthStencil;
	depthDesc.debugName            = "Depth Buffer";
	depthDesc.optimizedClearDepth  = 1.0f;
	m_DepthBuffer = m_GpuDevice->CreateTexture(depthDesc);

	const auto& backBuffers = m_GpuDevice->GetBackBufferTextures();
	for (const auto& bb : backBuffers)
	{
		FramebufferDesc fbDesc;
		fbDesc.colorAttachments.push_back({ bb, 0, 0 });
		fbDesc.depthAttachment = { m_DepthBuffer, 0, 0 };
		m_Framebuffers.push_back(m_GpuDevice->CreateFramebuffer(fbDesc));
	}

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.vs                            = m_VertShader;
	pipelineDesc.ps                            = m_FragShader;
	pipelineDesc.inputLayout                   = m_InputLayout;
	pipelineDesc.primType                      = PrimitiveType::TriangleList;
	pipelineDesc.rasterizer.frontCCW           = true;
	pipelineDesc.rasterizer.cullMode           = CullMode::Back;
	pipelineDesc.depthStencil.depthTestEnable  = true;
	pipelineDesc.depthStencil.depthWriteEnable = true;
	pipelineDesc.depthStencil.depthFunc        = ComparisonFunc::Less;
	pipelineDesc.bindingLayouts                = { m_BindingLayout };
	if (m_Framebuffers.empty())
	{
		APP_FATAL("No framebuffers created — swapchain may not be initialized");
		abort();
	}
	m_Pipeline = m_GpuDevice->CreateGraphicsPipeline(pipelineDesc, m_Framebuffers[0]);
}

void AppLayer::InitGpuResources()
{
	std::vector<uint8_t> vertBytes, fragBytes;
#ifdef COMPILE_WITH_VULKAN
	if (m_ActiveBackend == RenderBackend::Vulkan)
	{
		vertBytes = LoadSPIRV("shaders/test_vert.spv");
		fragBytes = LoadSPIRV("shaders/test_frag.spv");
	}
#endif
#ifdef COMPILE_WITH_DX12
	if (m_ActiveBackend == RenderBackend::D3D12)
	{
		vertBytes = LoadSPIRV("shaders/test_vs.cso");
		fragBytes = LoadSPIRV("shaders/test_ps.cso");
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

	// b0 — PerObject: World (64) + ViewProjection (64) + CameraPosition (12) + pad (4) + CameraForward (12) + pad (4) = 160 bytes
	BufferDesc mvpDesc;
	mvpDesc.byteSize  = sizeof(float) * 40;
	mvpDesc.usage     = BufferUsage::Constant;
	mvpDesc.debugName = "PerObject Buffer";
	m_MvpBuffer = m_GpuDevice->CreateBuffer(mvpDesc);

	// b1 — material constants: base color factor (16 bytes)
	BufferDesc matDesc;
	matDesc.byteSize  = sizeof(float) * 4;
	matDesc.usage     = BufferUsage::Constant;
	matDesc.debugName = "Material Buffer";
	m_MaterialBuffer = m_GpuDevice->CreateBuffer(matDesc);

	// Shared linear-wrap sampler
	SamplerDesc samplerDesc;
	samplerDesc.minFilter     = Filter::Linear;
	samplerDesc.magFilter     = Filter::Linear;
	samplerDesc.mipFilter     = Filter::Linear;
	samplerDesc.addressU      = AddressMode::Wrap;
	samplerDesc.addressV      = AddressMode::Wrap;
	samplerDesc.addressW      = AddressMode::Wrap;
	samplerDesc.maxAnisotropy = 1;
	m_Sampler = m_GpuDevice->CreateSampler(samplerDesc);

	// 1×1 white fallback texture — bound whenever a material has no albedo map
	{
		TextureDesc desc;
		desc.width     = 1;
		desc.height    = 1;
		desc.format    = GpuFormat::RGBA8_UNORM;
		desc.usage     = TextureUsage::ShaderResource;
		desc.debugName = "Fallback White Texture";
		m_FallbackTexture = m_GpuDevice->CreateTexture(desc);

		const uint8_t white[4] = { 255, 255, 255, 255 };
		auto uploadCtx = m_GpuDevice->CreateCommandContext();
		uploadCtx->Open();
		uploadCtx->WriteTexture(m_FallbackTexture, 0, 0, white, 4);
		uploadCtx->Close();
		m_GpuDevice->ExecuteCommandContext(*uploadCtx);
		m_GpuDevice->WaitForIdle();
	}

	// Flat normal fallback: (128, 128, 255) decodes to (0, 0, 1) — no perturbation
	{
		TextureDesc desc;
		desc.width     = 1;
		desc.height    = 1;
		desc.format    = GpuFormat::RGBA8_UNORM;
		desc.usage     = TextureUsage::ShaderResource;
		desc.debugName = "Flat Normal Texture";
		m_FlatNormalTexture = m_GpuDevice->CreateTexture(desc);

		const uint8_t flatNormal[4] = { 128, 128, 255, 255 };
		auto uploadCtx = m_GpuDevice->CreateCommandContext();
		uploadCtx->Open();
		uploadCtx->WriteTexture(m_FlatNormalTexture, 0, 0, flatNormal, 4);
		uploadCtx->Close();
		m_GpuDevice->ExecuteCommandContext(*uploadCtx);
		m_GpuDevice->WaitForIdle();
	}

	// Metallic-roughness fallback: G=204 (roughness≈0.8), B=0 (metallic=0) — non-metallic, rough
	{
		TextureDesc desc;
		desc.width     = 1;
		desc.height    = 1;
		desc.format    = GpuFormat::RGBA8_UNORM;
		desc.usage     = TextureUsage::ShaderResource;
		desc.debugName = "Default MetallicRoughness Texture";
		m_DefaultMetallicRoughness = m_GpuDevice->CreateTexture(desc);

		const uint8_t mr[4] = { 0, 204, 0, 255 }; // R=unused, G=roughness(0.8), B=metallic(0)
		auto uploadCtx = m_GpuDevice->CreateCommandContext();
		uploadCtx->Open();
		uploadCtx->WriteTexture(m_DefaultMetallicRoughness, 0, 0, mr, 4);
		uploadCtx->Close();
		m_GpuDevice->ExecuteCommandContext(*uploadCtx);
		m_GpuDevice->WaitForIdle();
	}

	m_SceneRenderer->SetDevice(m_GpuDevice);

	CreatePipelineAndFramebuffers();
}

void AppLayer::DestroyGpuResources()
{
	m_SceneRenderer->ReleaseGpuResources();

	m_CommandContext.reset();

	for (auto& [id, set] : m_MaterialSets)
		m_GpuDevice->DestroyBindingSet(set);
	m_MaterialSets.clear();

	for (auto& [id, tex] : m_GpuTextures)
		m_GpuDevice->DestroyTexture(tex);
	m_GpuTextures.clear();

	m_GpuDevice->DestroyTexture(m_FallbackTexture);           m_FallbackTexture          = {};
	m_GpuDevice->DestroyTexture(m_FlatNormalTexture);         m_FlatNormalTexture        = {};
	m_GpuDevice->DestroyTexture(m_DefaultMetallicRoughness);  m_DefaultMetallicRoughness = {};
	m_GpuDevice->DestroySampler(m_Sampler);              m_Sampler          = {};
	m_GpuDevice->DestroyBuffer(m_MaterialBuffer);        m_MaterialBuffer   = {};
	m_GpuDevice->DestroyBuffer(m_MvpBuffer);             m_MvpBuffer        = {};
	m_GpuDevice->DestroyGraphicsPipeline(m_Pipeline);    m_Pipeline         = {};
	for (auto fb : m_Framebuffers)
		m_GpuDevice->DestroyFramebuffer(fb);
	m_Framebuffers.clear();
	m_GpuDevice->DestroyTexture(m_DepthBuffer);          m_DepthBuffer      = {};
	m_GpuDevice->DestroyInputLayout(m_InputLayout);      m_InputLayout      = {};
	m_GpuDevice->DestroyBindingLayout(m_BindingLayout);  m_BindingLayout    = {};
	m_GpuDevice->DestroyShader(m_FragShader);            m_FragShader       = {};
	m_GpuDevice->DestroyShader(m_VertShader);            m_VertShader       = {};
}

GpuTexture AppLayer::EnsureTextureUploaded(AssetHandle<TextureAsset> handle)
{
	if (!handle.IsValid())
		return m_FallbackTexture;

	auto it = m_GpuTextures.find(handle.id);
	if (it != m_GpuTextures.end())
		return it->second;

	TextureAsset* asset = m_AssetManager->GetAsset(handle);
	if (!asset || asset->Data.empty())
		return m_FallbackTexture;

	TextureDesc desc;
	desc.width     = asset->Width;
	desc.height    = asset->Height;
	desc.format    = GpuFormat::RGBA8_SRGB; // albedo data is sRGB-encoded; GPU linearises on sample
	desc.usage     = TextureUsage::ShaderResource;
	desc.debugName = "Texture";
	GpuTexture tex = m_GpuDevice->CreateTexture(desc);

	auto uploadCtx = m_GpuDevice->CreateCommandContext();
	uploadCtx->Open();
	uploadCtx->WriteTexture(tex, 0, 0, asset->Data.data(), asset->Width * 4u);
	uploadCtx->Close();
	m_GpuDevice->ExecuteCommandContext(*uploadCtx);
	m_GpuDevice->WaitForIdle();

	m_GpuTextures.emplace(handle.id, tex);
	return tex;
}

GpuBindingSet AppLayer::GetOrCreateMaterialSet(AssetHandle<MaterialAsset> handle)
{
	AssetID key = handle.IsValid() ? handle.id : NullAssetId;

	auto it = m_MaterialSets.find(key);
	if (it != m_MaterialSets.end())
		return it->second;

	GpuTexture albedo             = m_FallbackTexture;
	GpuTexture normal             = m_FlatNormalTexture;
	GpuTexture metallicRoughness  = m_DefaultMetallicRoughness;

	if (handle.IsValid())
	{
		MaterialAsset* mat = m_AssetManager->GetAsset(handle);
		if (mat)
		{
			if (mat->AlbedoMap.IsValid())           albedo           = EnsureTextureUploaded(mat->AlbedoMap);
			if (mat->NormalMap.IsValid())            normal           = EnsureTextureUploaded(mat->NormalMap);
			if (mat->MetallicRoughnessMap.IsValid()) metallicRoughness= EnsureTextureUploaded(mat->MetallicRoughnessMap);
		}
	}

	BindingSetDesc setDesc;
	setDesc.items = {
		BindingItem::ConstantBuffer(0, m_MvpBuffer),
		BindingItem::Texture(0,         albedo),
		BindingItem::Texture(1,         normal),
		BindingItem::Texture(2,         metallicRoughness),
		BindingItem::Sampler(2,         m_Sampler),
	};
	GpuBindingSet set = m_GpuDevice->CreateBindingSet(setDesc, m_BindingLayout);
	m_MaterialSets.emplace(key, set);
	return set;
}

void AppLayer::SwitchBackend(RenderBackend next)
{
	m_GpuDevice->WaitForIdle();
	m_GpuDevice->RunGarbageCollection();
	DestroyGpuResources();

	// m_GpuDevice is non-owning — clear the pointer before destroying the owner
	m_GpuDevice = nullptr;
	m_RenderDevice.reset();
	glfwPollEvents();

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
	// Get engine submodules
	m_SceneRenderer = Engine::Get().GetSubmodule<SceneRenderer>();
	m_SceneManager = Engine::Get().GetSubmodule<SceneManager>();
	m_AssetManager = Engine::Get().GetSubmodule<AssetManager>();

	// Create the main scene and populate it with Sponza mesh entities
	Scene* scene = m_SceneManager->CreateScene("Main");
	m_SceneManager->SetActiveScene("Main");

	GltfObject model = m_AssetManager->LoadGltf("Sponza/Sponza.gltf");
	for (size_t i = 0; i < model.Meshes.size(); ++i)
	{
		Entity entity = scene->CreateEntity("SponzaMesh");
		entity.AddComponent<MeshComponent>().Mesh = model.Meshes[i];
		entity.AddComponent<MaterialComponent>().Material = model.MeshMaterials[i];
		entity.GetComponent<TransformComponent>().Scale = Vector3(0.01f);
	}

	// Open window
	WindowDesc desc;
	desc.title  = "Triangle Demo";
	desc.width  = 1280;
	desc.height = 720;
	m_WindowHandle = m_WindowSystem.OpenWindow(desc);

	// Always GLFW, so we can always get the raw window for cursor control.
	auto& glfwWS = static_cast<GLFWWindowSystem&>(m_WindowSystem);
	m_GlfwWindow = glfwWS.GetGLFWWindow(m_WindowHandle);
	glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Pick the first compiled-in backend as the default
#if defined(COMPILE_WITH_VULKAN)
	m_ActiveBackend = RenderBackend::Vulkan;
#elif defined(COMPILE_WITH_DX12)
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
#if defined(COMPILE_WITH_VULKAN) && defined(COMPILE_WITH_DX12)
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
		// Release resources that hold back-buffer references or reference the binding layout
		for (auto& [id, set] : m_MaterialSets)
			m_GpuDevice->DestroyBindingSet(set);
		m_MaterialSets.clear();
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
	if (m_Keys[GLFW_KEY_E]) wishDir.y -= 1.f;
	if (m_Keys[GLFW_KEY_Q]) wishDir.y += 1.f;

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

	Matrix4x4 view       = Matrix4x4::LookAt(m_CamPos, m_CamPos + front, Vector3(0.f, 1.f, 0.f));
	Matrix4x4 projection = Matrix4x4::Perspective(60.0f, (float)fbWidth / (float)fbHeight, 0.1f, 500.0f);

	// ---- Collect, cull, sort ----
	Viewport vp;
	vp.x = 0.f;  vp.y = 0.f;
	vp.width    = (float)fbWidth;
	vp.height   = (float)fbHeight;
	vp.minDepth = 0.f;  vp.maxDepth = 1.f;

	RenderView renderView = MakeRenderView(
		m_CamPos, view, projection, vp, fb, 0.1f, 500.0f);

	m_SceneRenderer->BeginFrame();

	if (Scene* scene = m_SceneManager->GetActiveScene())
		SceneRenderSystem::CollectAndSubmit(*scene, m_CamPos, *m_SceneRenderer);

	m_SceneRenderer->RenderView(renderView);

	// Pre-warm: upload any pending textures and create binding sets for all
	// visible packets BEFORE opening the command context. Creating GPU resources
	// (textures, binding sets) or executing upload command contexts inside a
	// render pass corrupts backend state — all draws end up using the first
	// binding set that was created.
	for (const RenderPacket* packet : m_SceneRenderer->GetVisiblePackets())
		GetOrCreateMaterialSet(packet->Material);

	// ---- Record draw calls ----
	m_CommandContext->Open();

	RenderPassDesc passDesc;
	passDesc.framebuffer = fb;
	passDesc.clearColor  = true;
	passDesc.colorValue  = { 0.1f, 0.1f, 0.1f, 1.0f };
	passDesc.clearDepth  = true;
	passDesc.depthValue  = 1.0f;
	m_CommandContext->BeginRenderPass(passDesc);

	m_CommandContext->SetGraphicsPipeline(m_Pipeline);
	m_CommandContext->SetViewport(vp);

	ScissorRect sr;
	sr.x = 0;  sr.y = 0;
	sr.width  = (int)fbWidth;
	sr.height = (int)fbHeight;
	m_CommandContext->SetScissor(sr);

	// PerObject cbuffer layout must match test.vs.hlsl exactly.
	struct PerObjectData
	{
		Matrix4x4 World;
		Matrix4x4 ViewProjection;
		float     CameraPosition[3];
		float     _pad;
		float     CameraForward[3];
		float     _pad1;
	};

	Matrix4x4 viewProjection = view * projection;

	for (const RenderPacket* packet : m_SceneRenderer->GetVisiblePackets())
	{
		const SceneRenderer::GpuMesh* gpuMesh = m_SceneRenderer->GetGpuMesh(packet->Mesh);
		if (!gpuMesh)
			continue;

		PerObjectData perObj;
		perObj.World             = packet->WorldTransform;
		perObj.ViewProjection    = viewProjection;
		perObj.CameraPosition[0] = m_CamPos.x;
		perObj.CameraPosition[1] = m_CamPos.y;
		perObj.CameraPosition[2] = m_CamPos.z;
		perObj._pad              = 0.f;
		perObj.CameraForward[0]  = front.x;
		perObj.CameraForward[1]  = front.y;
		perObj.CameraForward[2]  = front.z;
		perObj._pad1             = 0.f;
		m_CommandContext->WriteBuffer(m_MvpBuffer, &perObj, sizeof(perObj));

		// Binding set carries the albedo texture for this material
		GpuBindingSet bindingSet = GetOrCreateMaterialSet(packet->Material);

		m_CommandContext->SetVertexBuffer(0, gpuMesh->VertexBuffer);
		m_CommandContext->SetIndexBuffer(gpuMesh->IndexBuffer, GpuFormat::R32_UINT);
		m_CommandContext->SetBindingSet(bindingSet);

		DrawIndexedArgs drawArgs;
		drawArgs.indexCount = gpuMesh->IndexCount;
		m_CommandContext->DrawIndexed(drawArgs);
	}

	m_CommandContext->EndRenderPass();
	m_CommandContext->Close();

	m_GpuDevice->ExecuteCommandContext(*m_CommandContext);
	m_SceneRenderer->EndFrame();
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
#if defined(COMPILE_WITH_VULKAN) && defined(COMPILE_WITH_DX12)
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

	d.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e)
	{
		if (m_FirstMouse)
		{
			m_LastMouseX = e.GetX();
			m_LastMouseY = e.GetY();
			m_FirstMouse = false;
			return false;
		}

		float dx = (e.GetX() - m_LastMouseX) * m_MouseSens;
		float dy = (e.GetY() - m_LastMouseY) * m_MouseSens;
		m_LastMouseX = e.GetX();
		m_LastMouseY = e.GetY();

		m_CamYaw   += dx;
		m_CamPitch  = std::clamp(m_CamPitch - dy, -89.f, 89.f);
		return false;
	});

	d.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e)
	{
		m_MoveSpeed = std::clamp(m_MoveSpeed + e.GetYOffset() * 0.5f, 0.5f, 50.f);
		return false;
	});
}
