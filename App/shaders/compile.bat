@echo off
:: Compiles GLSL shaders to SPIR-V using glslc (ships with the Vulkan SDK).
:: Run this from the App/shaders/ directory, or adjust paths as needed.
:: Output .spv files are loaded at runtime from the working directory.

glslc triangle.vert -o triangle_vert.spv
glslc triangle.frag -o triangle_frag.spv

echo Done.
