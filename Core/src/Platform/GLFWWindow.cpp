#include "GLFWWindow.h"

#include <GLFW/glfw3.h>
#ifdef _WIN32
  #define GLFW_EXPOSE_NATIVE_WIN32
  #include <GLFW/glfw3native.h>
#else
  #define GLFW_EXPOSE_NATIVE_X11
  #include <GLFW/glfw3native.h>
#endif

#include "../Events/ApplicationEvents.h"
#include "../Events/KeyEvents.h"
#include "../Events/MouseEvents.h"

// -------------------------------------------------------------------------
// Handle packing helpers
// -------------------------------------------------------------------------
uintptr_t GLFWWindowSystem::PackHandle(WindowHandle h)
{
	uint32_t id  = h.id;
	uint32_t gen = h.generation;
	return static_cast<uintptr_t>(id | (gen << 20));
}

WindowHandle GLFWWindowSystem::UnpackHandle(uintptr_t packed)
{
	WindowHandle h;
	h.id		 = packed & 0xFFFFF;
	h.generation = (packed >> 20) & 0xFFF;
	return h;
}

// -------------------------------------------------------------------------
// IEngineSubmodule
// -------------------------------------------------------------------------
void GLFWWindowSystem::Init()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void GLFWWindowSystem::Shutdown()
{
	m_Windows.ForEach([](WindowHandle, WindowEntry& entry) {
		if (entry.glfwWindow)
		{
			glfwDestroyWindow(entry.glfwWindow);
			entry.glfwWindow = nullptr;
		}
	});

	glfwTerminate();
}

void GLFWWindowSystem::Tick(float)
{
	glfwPollEvents();
}

// -------------------------------------------------------------------------
// IWindowSystem
// -------------------------------------------------------------------------
WindowHandle GLFWWindowSystem::OpenWindow(const WindowDesc& desc)
{
	GLFWwindow* glfwWindow = glfwCreateWindow(
		desc.width, desc.height, desc.title.c_str(), nullptr, nullptr
	);

	WindowEntry entry;
	entry.glfwWindow = glfwWindow;

#ifdef _WIN32
	entry.hwnd = glfwGetWin32Window(glfwWindow);
#else
	entry.display = glfwGetX11Display();
	entry.xwindow = reinterpret_cast<void*>(glfwGetX11Window(glfwWindow));
#endif

	WindowHandle handle = m_Windows.Insert(std::move(entry));

	glfwSetWindowUserPointer(glfwWindow, reinterpret_cast<void*>(PackHandle(handle)));

	glfwSetWindowCloseCallback      (glfwWindow, OnGLFWClose);
	glfwSetFramebufferSizeCallback  (glfwWindow, OnGLFWResize);
	glfwSetKeyCallback              (glfwWindow, OnGLFWKey);
	glfwSetCharCallback             (glfwWindow, OnGLFWChar);
	glfwSetMouseButtonCallback      (glfwWindow, OnGLFWMouseButton);
	glfwSetCursorPosCallback        (glfwWindow, OnGLFWCursorPos);
	glfwSetScrollCallback           (glfwWindow, OnGLFWScroll);

	return handle;
}

void GLFWWindowSystem::CloseWindow(WindowHandle handle)
{
	WindowEntry* entry = m_Windows.Get(handle);
	if (!entry) return;

	glfwDestroyWindow(entry->glfwWindow);
	m_Windows.Remove(handle);
}

glm::uvec2 GLFWWindowSystem::GetExtent(WindowHandle handle) const
{
	const WindowEntry* entry = m_Windows.Get(handle);
	if (!entry) return {};

	int w, h;
	glfwGetFramebufferSize(entry->glfwWindow, &w, &h);
	return { static_cast<unsigned int>(w), static_cast<unsigned int>(h) };
}

void* GLFWWindowSystem::GetNativeHandle(WindowHandle handle) const
{
	const WindowEntry* entry = m_Windows.Get(handle);
	if (!entry) return nullptr;

#ifdef _WIN32
	return entry->hwnd;
#else
	return entry->glfwWindow;
#endif
}

bool GLFWWindowSystem::ShouldClose(WindowHandle handle) const
{
	const WindowEntry* entry = m_Windows.Get(handle);
	if (!entry) return true;

	return glfwWindowShouldClose(entry->glfwWindow);
}

double GLFWWindowSystem::GetTime() const
{
	return glfwGetTime();
}

// -------------------------------------------------------------------------
// GLFW callbacks
// -------------------------------------------------------------------------
void GLFWWindowSystem::OnGLFWClose(GLFWwindow* window)
{
	WindowHandle handle = UnpackHandle(
		reinterpret_cast<uintptr_t>(glfwGetWindowUserPointer(window))
	);

	WindowCloseEvent e(handle);
	Engine::Get().OnEvent(e);
}

void GLFWWindowSystem::OnGLFWResize(GLFWwindow* window, int width, int height)
{
	WindowHandle handle = UnpackHandle(
		reinterpret_cast<uintptr_t>(glfwGetWindowUserPointer(window))
	);

	WindowResizeEvent e(handle,
		static_cast<unsigned int>(width),
		static_cast<unsigned int>(height)
	);
	Engine::Get().OnEvent(e);
}

void GLFWWindowSystem::OnGLFWKey(GLFWwindow*, int key, int, int action, int)
{
	switch (action)
	{
		case GLFW_PRESS:
		{
			KeyPressedEvent e(key, 0);
			Engine::Get().OnEvent(e);
			break;
		}
		case GLFW_REPEAT:
		{
			KeyPressedEvent e(key, 1);
			Engine::Get().OnEvent(e);
			break;
		}
		case GLFW_RELEASE:
		{
			KeyReleasedEvent e(key);
			Engine::Get().OnEvent(e);
			break;
		}
	}
}

void GLFWWindowSystem::OnGLFWChar(GLFWwindow*, unsigned int codepoint)
{
	KeyTypedEvent e(static_cast<int>(codepoint));
	Engine::Get().OnEvent(e);
}

void GLFWWindowSystem::OnGLFWMouseButton(GLFWwindow*, int button, int action, int)
{
	if (action == GLFW_PRESS)
	{
		MouseButtonPressedEvent e(button);
		Engine::Get().OnEvent(e);
	}
	else if (action == GLFW_RELEASE)
	{
		MouseButtonReleasedEvent e(button);
		Engine::Get().OnEvent(e);
	}
}

void GLFWWindowSystem::OnGLFWCursorPos(GLFWwindow*, double xpos, double ypos)
{
	MouseMovedEvent e(static_cast<float>(xpos), static_cast<float>(ypos));
	Engine::Get().OnEvent(e);
}

void GLFWWindowSystem::OnGLFWScroll(GLFWwindow*, double xoffset, double yoffset)
{
	MouseScrolledEvent e(static_cast<float>(xoffset), static_cast<float>(yoffset));
	Engine::Get().OnEvent(e);
}
