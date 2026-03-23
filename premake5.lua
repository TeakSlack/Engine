include "Dependencies.lua"

workspace "DeferredRender"
    configurations {"Debug", "Release"}
    platforms {"x86_64"}
    startproject "DeferredRender"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Projects"
    include "vendor/glfw"
    include "DeferredRender"