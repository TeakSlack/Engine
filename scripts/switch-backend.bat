@echo off
:: Switch the CMake backend between Vulkan, D3D12, or both (for runtime hot-swap).
:: Usage: switch-backend.bat [vulkan | dx12 | both]

set BACKEND=%~1
if /i "%BACKEND%"=="vulkan" goto :do_vulkan
if /i "%BACKEND%"=="vk"     goto :do_vulkan
if /i "%BACKEND%"=="dx12"   goto :do_dx12
if /i "%BACKEND%"=="d3d12"  goto :do_dx12
if /i "%BACKEND%"=="both"   goto :do_both

echo Usage: switch-backend.bat [vulkan ^| dx12 ^| both]
echo.
echo   vulkan  -- CORE_VULKAN=ON  CORE_DX12=OFF
echo   dx12    -- CORE_VULKAN=OFF CORE_DX12=ON
echo   both    -- CORE_VULKAN=ON  CORE_DX12=ON  (enables ` key runtime hot-swap)
pause
exit /b 1

:do_vulkan
set VK_FLAG=ON
set DX_FLAG=OFF
set LABEL=Vulkan only
goto :configure

:do_dx12
set VK_FLAG=OFF
set DX_FLAG=ON
set LABEL=D3D12 only
goto :configure

:do_both
set VK_FLAG=ON
set DX_FLAG=ON
set LABEL=Vulkan + D3D12 (hot-swap enabled)
goto :configure

:configure
echo [switch-backend] Reconfiguring for %LABEL% (CORE_VULKAN=%VK_FLAG%, CORE_DX12=%DX_FLAG%)...
cd %~dp0..\
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCORE_VULKAN=%VK_FLAG% -DCORE_DX12=%DX_FLAG%
pause
