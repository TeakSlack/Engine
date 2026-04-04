#ifndef IWINDOWSYSTEM_H
#define IWINDOWSYSTEM_H

#include <glm/glm.hpp>
#include "../Util/Handle.h"

#include <string>

struct WindowTag {};
using WindowHandle = Handle<WindowTag>;

struct WindowDesc
{
	int width			= 1280;
	int height			= 720;
	std::string title	= "Core";
};

class IWindowSystem
{
public:
	virtual					~IWindowSystem() = default;

	virtual WindowHandle	OpenWindow(const WindowDesc& desc) = 0;
	virtual void			CloseWindow(WindowHandle handle) = 0;

	virtual glm::uvec2		GetExtent(WindowHandle handle) const = 0;
	virtual void*			GetNativeHandle(WindowHandle handle) const = 0;
	virtual bool			ShouldClose(WindowHandle handle) const = 0;

	// Wall-clock seconds since Init(). Used by Engine::Run for deltaTime.
	virtual double			GetTime() const = 0;
};

#endif // IWINDOWSYSTEM_H