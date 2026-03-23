project "glfw"
    kind "StaticLib"
    language "C"
    architecture "x86_64"

    targetdir ( "%{wks.location}/vendor/glfw/lib/" )
    objdir ( "%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}" )

    files
    {
        "glfw/src/context.c",
        "glfw/src/egl_context.c",
        "glfw/src/init.c",
        "glfw/src/input.c",
        "glfw/src/internal.h",
        "glfw/src/mappings.h",
        "glfw/src/monitor.c",
        "glfw/src/null_init.c",
        "glfw/src/null_joystick.c",
        "glfw/src/null_joystick.h",
        "glfw/src/null_monitor.c",
        "glfw/src/null_platform.h",
        "glfw/src/null_window.c",
        "glfw/src/osmesa_context.c",
        "glfw/src/platform.c",
        "glfw/src/platform.h",
        "glfw/src/vulkan.c",
        "glfw/src/window.c",
    }

    includedirs { "glfw/include" }

    filter "system:windows"
        systemversion "latest"
        staticruntime "on"

        files
        {
            "glfw/src/win32_init.c",
            "glfw/src/win32_joystick.c",
            "glfw/src/win32_joystick.h",
            "glfw/src/win32_module.c",
            "glfw/src/win32_monitor.c",
            "glfw/src/win32_platform.h",
            "glfw/src/win32_thread.c",
            "glfw/src/win32_thread.h",
            "glfw/src/win32_time.c",
            "glfw/src/win32_time.h",
            "glfw/src/win32_window.c",
            "glfw/src/wgl_context.c"
        }

        defines
        {
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "system:linux"
        systemversion "latest"
        staticruntime "on"

        files
        {
            "glfw/src/glx_context.c",
            "glfw/src/linux_joystick.c",
            "glfw/src/linux_joystick.h",
            "glfw/src/posix_module.c",
            "glfw/src/posix_poll.c",
            "glfw/src/posix_poll.h",
            "glfw/src/posix_thread.c",
            "glfw/src/posix_thread.h",
            "glfw/src/posix_time.c",
            "glfw/src/posix_time.h",
            "glfw/src/wl_init.c",
            "glfw/src/wl_monitor.c",
            "glfw/src/wl_platform.h",
            "glfw/src/wl_window.c",
            "glfw/src/x11_init.c",
            "glfw/src/x11_monitor.c",
            "glfw/src/x11_platform.h",
            "glfw/src/x11_window.c",
            "glfw/src/xkb_unicode.c",
            "glfw/src/xkb_unicode.h"
        }

        defines
        {
            "_GLFW_X11",
            "_GLFW_WAYLAND",
        }

    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"