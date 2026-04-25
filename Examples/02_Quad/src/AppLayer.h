#ifndef APP_LAYER_H
#define APP_LAYER_H

#include <Util/Layer.h>
#include <Window/IWindowSystem.h>
#include <Graphics/RenderDevice.h>
#include <Render/IGpuDevice.h>
#include <Render/ICommandContext.h>
#include <Render/FrameGraph/FrameGraph.h>
#include <memory>
#include <vector>
#include <filesystem>

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
	std::vector<uint8_t> LoadBinary(const std::filesystem::path& path);

	void CreateFramebuffers();
	void DestroyFramebuffers();
	void CreatePipeline();
	void DestroyPipeline();

	IWindowSystem*					 m_WindowSystem = nullptr;
	WindowHandle					 m_WindowHandle;
	GLFWwindow*						 m_GlfwWindow = nullptr;

	// ---- Render ----
	std::unique_ptr<IRenderDevice>   m_RenderDevice;
	IGpuDevice*                      m_GpuDevice = nullptr; // non-owning; owned by m_RenderDevice
	std::unique_ptr<ICommandContext> m_CommandContext;
	std::unique_ptr<FrameGraph>      m_FrameGraph;

	std::vector<GpuFramebuffer>      m_Framebuffers;
	GpuGraphicsPipeline				 m_Pipeline;
	GpuBuffer						 m_VertexBuffer;
	GpuBuffer						 m_IndexBuffer;

	bool     m_PendingResize = false;
	uint32_t m_Width = 0, m_Height = 0;
	uint32_t m_FrameCount = 0;
};

#endif // APP_LAYER_H
