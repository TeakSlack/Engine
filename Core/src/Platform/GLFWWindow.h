#ifndef GLFWWINDOW_H
#define GLFWWINDOW_H

#include "../IWindowSystem.h"
#include "../Engine.h"
#include "../Handle.h"

struct GLFWwindow; // forward decl — keeps glfw3.h out of this header

class GLFWWindowSystem : public IWindowSystem, public IEngineSubmodule
{
public:
	GLFWWindowSystem() : IEngineSubmodule("GLFWWindowSystem") {}

	// IEngineSubmodule
	void Init()					override;
	void Shutdown()				override;
	void Tick(float deltaTime)	override;

	// IWindowSystem
	WindowHandle	OpenWindow(const WindowDesc& desc)			override;
	void			CloseWindow(WindowHandle handle)			override;

	glm::uvec2		GetExtent(WindowHandle handle)		const	override;
	void*			GetNativeHandle(WindowHandle handle)const	override;
	bool			ShouldClose(WindowHandle handle)	const	override;
	double			GetTime()							const	override;

	// Returns the raw GLFWwindow* for a handle (e.g. to pass to VulkanDevice).
	GLFWwindow* GetGLFWWindow(WindowHandle handle) const
	{
		const WindowEntry* entry = m_Windows.Get(handle);
		return entry ? entry->glfwWindow : nullptr;
	}

private:
	struct WindowEntry
	{
		GLFWwindow* glfwWindow = nullptr;
#ifdef _WIN32
		void* hwnd    = nullptr;
#else
		void* xwindow = nullptr;
		void* display = nullptr;
#endif
	};

	SlotMap<WindowEntry, WindowTag> m_Windows;

	// Packs a WindowHandle into a pointer-sized value for glfwSetWindowUserPointer.
	// Bits [0, 19] = id, bits [20, 31] = generation.
	static uintptr_t    PackHandle  (WindowHandle h);
	static WindowHandle UnpackHandle(uintptr_t packed);

	// GLFW static callbacks
	static void OnGLFWClose      (GLFWwindow* window);
	static void OnGLFWResize     (GLFWwindow* window, int width, int height);
	static void OnGLFWKey        (GLFWwindow* window, int key, int scancode, int action, int mods);
	static void OnGLFWChar       (GLFWwindow* window, unsigned int codepoint);
	static void OnGLFWMouseButton(GLFWwindow* window, int button, int action, int mods);
	static void OnGLFWCursorPos  (GLFWwindow* window, double xpos, double ypos);
	static void OnGLFWScroll     (GLFWwindow* window, double xoffset, double yoffset);
};

#endif // GLFWWINDOW_H
