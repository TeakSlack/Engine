VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["AppBase"] = "%{wks.location}/AppBase/src"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["spdlog"] = "%{wks.location}/vendor/spdlog/include"
IncludeDir["glfw"] = "%{wks.location}/vendor/glfw/include"
IncludeDir["glm"] = "%{wks.location}/vendor/glm"
IncludeDir["stb"] = "%{wks.location}/vendor/stb"
IncludeDir["vma"] = "%{wks.location}/vendor/VulkanMemoryAllocator/include"

LibraryDir = {}
LibraryDir["AppBase"] = "%{wks.location}/lib"
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["glfw"] = "%{wks.location}/vendor/glfw/lib"