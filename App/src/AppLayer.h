#ifndef APP_LAYER_H
#define APP_LAYER_H

#include <Layer.h>
#include <IWindowSystem.h>

class AppLayer : public Layer
{
public:
	AppLayer(IWindowSystem& ws) : Layer("AppLayer"), m_WindowSystem(ws) {}

	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate(float deltaTime) override;
	void OnEvent(Event& event) override;

private:
	IWindowSystem& m_WindowSystem;
	WindowHandle m_WindowHandle;
};

#endif // APP_LAYER_H