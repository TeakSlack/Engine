include "Dependencies.lua"

workspace "VulkanProjects"
    configurations {"Debug", "Release"}
    platforms {"x86_64"}
    startproject "VulkanDeferredRenderer"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Projects"
    include "vendor/glfw"
    include "VulkanDeferredRenderer"