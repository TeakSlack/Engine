#ifndef APP_LAYER_H
#define APP_LAYER_H

#include <Layer.h>
#include <IWindowSystem.h>
#include <Graphics/VulkanDevice.h>

#include <nvrhi/nvrhi.h>

#include <memory>
#include <vector>

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

	IWindowSystem& m_WindowSystem;
	WindowHandle   m_WindowHandle;

	std::unique_ptr<VulkanDevice>         m_RenderDevice;
	nvrhi::IDevice*                       m_NvrhiDevice = nullptr;

	nvrhi::ShaderHandle                   m_VertShader;
	nvrhi::ShaderHandle                   m_FragShader;
	nvrhi::GraphicsPipelineHandle         m_Pipeline;
	std::vector<nvrhi::FramebufferHandle> m_Framebuffers;
	nvrhi::CommandListHandle              m_CommandList;
};

#endif // APP_LAYER_H
