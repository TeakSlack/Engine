#include "AppLayer.h"
#include <Engine.h>
#include <Events/KeyEvents.h>

#include <iostream>

void AppLayer::OnAttach()
{
	WindowDesc desc;
	desc.title = "AppLayer Window";
	m_WindowHandle = m_WindowSystem.OpenWindow(desc);
}

void AppLayer::OnDetach()
{
	m_WindowSystem.CloseWindow(m_WindowHandle);
}

void AppLayer::OnUpdate(float deltaTime)
{
	if (m_WindowSystem.ShouldClose(m_WindowHandle))
	{

	}
}

void AppLayer::OnEvent(Event& event)
{
    EventDispatcher d(event);
    d.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
        std::cout << e.GetKeyCode() << std::endl;
        if (e.GetKeyCode() == 87)
        {
            
        }
        return false; // don't consume — let other layers see it too
        });
}