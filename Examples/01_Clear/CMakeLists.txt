file(GLOB_RECURSE 01_Clear_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE 01_Clear_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(01_Clear ${01_Clear_SOURCES} ${01_Clear_HEADERS})

# Mirror filesystem structure in VS filters
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${01_Clear_SOURCES} ${01_Clear_HEADERS})

# Core brings in Core/src, VendorIncludes, and backend libs transitively.
# Vulkan::Vulkan is linked directly here because 01_ClearLayer.cpp owns the
# VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE definition, which
# requires the Vulkan headers to be visible in that translation unit.
target_link_libraries(01_Clear PRIVATE
    Core
    glfw
)

if(COMPILE_WITH_VULKAN)
    target_link_libraries(01_Clear PRIVATE Vulkan::Vulkan)
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(01_Clear PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(01_Clear PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(01_Clear PRIVATE /utf-8)
else()
    target_compile_options(01_Clear PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — bin/{Config}-{OS}-x86_64/01_Clear/, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET 01_Clear PROPERTY FOLDER "Projects")

set_target_properties(01_Clear PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY             "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/01_Clear"
    VS_DEBUGGER_WORKING_DIRECTORY        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# Shader compilation via DXC
# DXC ships with the Vulkan SDK (VK_SDK_PATH / VULKAN_SDK) and the Windows SDK.
# Output .spv (Vulkan) and/or .cso (D3D12) go into 01_Clear/shaders/ so they are
# found at runtime relative to VS_DEBUGGER_WORKING_DIRECTORY (01_Clear/).
# ---------------------------------------------------------------------------
find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin"
    DOC "DirectX Shader Compiler"
)

if(NOT DXC_EXECUTABLE)
    message(WARNING
        "[01_Clear] dxc not found — shaders will not be compiled automatically. "
        "Ensure dxc.exe is on PATH or that VULKAN_SDK/VK_SDK_PATH is set.")
else()
    message(STATUS "DXC found: ${DXC_EXECUTABLE}")

    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_OUTPUTS)

    file(GLOB 01_Clear_SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsl")
    # Prevent VS from invoking FXC on these files — compilation is handled
    # exclusively by the add_custom_command targets above (DXC via CMake).
    # VS_TOOL_OVERRIDE "None" is required; HEADER_FILE_ONLY alone does not
    # suppress VS's built-in HLSL item handler.
    set_source_files_properties(${01_Clear_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
    add_custom_target(01_ClearShaders ALL SOURCES ${01_Clear_SHADERS} DEPENDS ${SHADER_OUTPUTS})
    source_group("shaders" FILES ${01_Clear_SHADERS})
    set_property(TARGET 01_ClearShaders PROPERTY FOLDER "Shaders")
    add_dependencies(01_Clear 01_ClearShaders)
endif()
