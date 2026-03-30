#include "AppLayer.h"

#include <Engine.h>
#include <Platform/GLFWWindow.h>

int main()
{
	GLFWWindowSystem windowSystem;
	AppLayer appLayer(windowSystem);

	Engine::Get().RegisterSubmodule(&windowSystem);
	Engine::Get().PushLayer(&appLayer);
	Engine::Get().Run();
}