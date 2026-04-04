#ifndef APP_LAYER_H
#define APP_LAYER_H

#include <Util/Layer.h>
#include <Window/IWindowSystem.h>
#include <Graphics/RenderDevice.h>
#include <Graphics/RenderBackend.h>
#include <Render/IGpuDevice.h>
#include <Render/ICommandContext.h>

#include <Math/Vector3.h>

#include <memory>
#include <vector>

struct GLFWwindow;

class AppLayer : public Layer
{
public:
	AppLayer(IWindowSystem& ws) : Layer("AppLayer"), m_WindowSystem(ws) {}

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(float deltaTime) override;
	void OnEvent(Event& event) override;

private:
	static std::vector<uint8_t> LoadSPIRV(const char* path);
	void CreatePipelineAndFramebuffers();
	void CreateBuffers();

	// Creates the correct IRenderDevice for the given backend.
	std::unique_ptr<IRenderDevice> MakeRenderDevice(RenderBackend backend);

	// Allocate all GPU-side resources (shaders, buffers, pipeline, framebuffers).
	// Requires m_GpuDevice and m_RenderDevice to already be valid.
	void InitGpuResources();

	// Release all GPU-side resources. Does NOT destroy the device or swapchain.
	void DestroyGpuResources();

	// Full backend swap: tears down GPU resources + device, rebuilds with `next`.
	void SwitchBackend(RenderBackend next);

	IWindowSystem& m_WindowSystem;
	WindowHandle   m_WindowHandle;
	GLFWwindow*    m_GlfwWindow = nullptr;

	// ---- Camera ----
	Vector3 m_CamPos    = { 0.f, 0.f, 3.f };
	float   m_CamYaw    = -90.f;
	float   m_CamPitch  =   0.f;
	float   m_MoveSpeed =   3.f;
	float   m_MouseSens =  0.12f;

	// ---- Input state ----
	bool    m_Keys[512]  = {};
	bool    m_RmbHeld    = false;
	float   m_LastMouseX = 0.f;
	float   m_LastMouseY = 0.f;
	bool    m_FirstMouse = true;
	Vector3 m_CamVel;

	// ---- Render ----
	RenderBackend                         m_ActiveBackend = RenderBackend::None;
	bool                                  m_PendingBackendSwap = false;
	std::unique_ptr<IRenderDevice>        m_RenderDevice;
	IGpuDevice*                           m_GpuDevice = nullptr; // non-owning; owned by m_RenderDevice

	GpuBuffer                             m_VertexBuffer;
	GpuBuffer                             m_IndexBuffer;
	GpuBuffer                             m_ConstantBuffer;
	GpuShader                             m_VertShader;
	GpuShader                             m_FragShader;
	GpuGraphicsPipeline                   m_Pipeline;
	std::vector<GpuFramebuffer>           m_Framebuffers;
	GpuTexture                            m_DepthBuffer;
	GpuBindingLayout                      m_BindingLayout;
	GpuInputLayout                        m_InputLayout;
	GpuBindingSet                         m_BindingSet;
	std::unique_ptr<ICommandContext>      m_CommandContext;
	bool                                  m_PendingResize = false;
	uint32_t                              m_Width = 0, m_Height = 0;
};

#endif // APP_LAYER_H
