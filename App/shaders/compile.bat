@echo off
:: Manually compile HLSL shaders via DXC.
:: CMake handles this automatically at build time; use this script for quick
:: iteration without a full rebuild.
::
:: Requires dxc.exe on PATH, or set VULKAN_SDK (ships with the Vulkan SDK).

if not "%VULKAN_SDK%"=="" (
    set DXC="%VULKAN_SDK%\Bin\dxc.exe"
) else (
    set DXC=dxc
)

echo Compiling SPIR-V (Vulkan)...
%DXC% -T vs_6_0 -E main -spirv -fspv-target-env=vulkan1.1 triangle.vs.hlsl -Fo triangle_vert.spv
%DXC% -T ps_6_0 -E main -spirv -fspv-target-env=vulkan1.1 triangle.ps.hlsl -Fo triangle_frag.spv

echo Compiling DXIL (D3D12)...
%DXC% -T vs_6_0 -E main triangle.vs.hlsl -Fo triangle_vert.cso
%DXC% -T ps_6_0 -E main triangle.ps.hlsl -Fo triangle_frag.cso

echo Done.
pause
