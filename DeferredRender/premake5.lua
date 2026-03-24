project "DeferredRender"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        targetdir ( "%{wks.location}/bin/" .. outputdir .. "/%{prj.name}" )
        objdir ( "%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}" )

        files 
        { 
            "src/**.h",
            "src/**.cpp",
        }

        includedirs 
        {
            "%{IncludeDir.VulkanSDK}",
            "%{IncludeDir.spdlog}",
            "%{IncludeDir.glfw}",
            "%{IncludeDir.glm}",
            "%{IncludeDir.stb}",
            "%{IncludeDir.vma}",
        }

        libdirs 
        { 
            "%{LibraryDir.VulkanSDK}" ,
            "%{LibraryDir.glfw}",
        }

        links
        {
            "glfw",
        }

        filter "system:windows"
            links 
            { 
                "vulkan-1", -- Vulkan lib for Windows ('vulkan-1.lib')
            }

            defines
            {
                "_CRT_SECURE_NO_WARNINGS"
            }

        filter "system:linux"
            links 
            { 
                "vulkan", -- Vulkan library for Linux (`libvulkan.so`)
            } 

        filter "configurations:Debug"
            defines { "DEBUG", "_DEBUG" }
            symbols "On"

        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "On"

        filter "action:gmake"
            buildoptions { "-Wall", "-Wextra", "-Werror", "-std=c++20" }

        filter "action:vs*"
            buildoptions { "/utf-8" }