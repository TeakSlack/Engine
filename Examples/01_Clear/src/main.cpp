#include "AppLayer.h"

#include <Engine.h>
#include <Window/GLFWWindow.h>

int main()
{
	GLFWWindowSystem windowSystem;
	AppLayer appLayer;

	Engine::Get().RegisterSubmodule(&windowSystem);
	Engine::Get().PushLayer(&appLayer);
	Engine::Get().Run();
}