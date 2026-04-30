#ifndef APP_LAYER_H
#define APP_LAYER_H

#include <Util/Layer.h>
#include <Window/IWindowSystem.h>
#include <Graphics/RenderDevice.h>
#include <Render/IGpuDevice.h>
#include <Render/ICommandContext.h>
#include <Render/FrameGraph/FrameGraph.h>
#include <Render/Pipeline/SkyLuts.h>
#include <memory>
#include <vector>

struct GLFWwindow;

class AppLayer : public Layer
{
public:
	AppLayer() : Layer("AppLayer") {}

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(float deltaTime) override;
	void OnEvent(Event& event) override;

private:
	void CreateFramebuffers();
	void DestroyFramebuffers();
	void CreateBlitPipeline();
	void DestroyBlitPipeline();

	IWindowSystem*                   m_WindowSystem  = nullptr;
	WindowHandle                     m_WindowHandle;
	GLFWwindow*                      m_GlfwWindow    = nullptr;

	std::unique_ptr<IRenderDevice>   m_RenderDevice;
	IGpuDevice*                      m_GpuDevice     = nullptr;
	std::unique_ptr<ICommandContext> m_CommandContext;
	std::unique_ptr<FrameGraph>      m_FrameGraph;

	std::vector<GpuFramebuffer>      m_Framebuffers;

	// ---- Sky LUTs (transmittance + single scattering) ----
	SkyLutsPass                      m_SkyPass;

	// ---- Fullscreen blit to display the LUT ----
	GpuGraphicsPipeline              m_BlitPipeline;
	GpuBindingLayout                 m_BlitBindingLayout;
	GpuBindingSet                    m_BlitBindingSet;
	GpuSampler                       m_LinearSampler;

	bool     m_PendingResize = false;
	uint32_t m_Width = 0, m_Height = 0;
	uint32_t m_FrameCount = 0;
};

#endif // APP_LAYER_H
