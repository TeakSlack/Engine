file(GLOB_RECURSE 03_BindlessQuad_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE 03_BindlessQuad_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(03_BindlessQuad ${03_BindlessQuad_SOURCES} ${03_BindlessQuad_HEADERS})

# Mirror filesystem structure in VS filters
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${03_BindlessQuad_SOURCES} ${03_BindlessQuad_HEADERS})

# Core brings in Core/src, VendorIncludes, and backend libs transitively.
# Vulkan::Vulkan is linked directly here because 03_BindlessQuadLayer.cpp owns the
# VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE definition, which
# requires the Vulkan headers to be visible in that translation unit.
target_link_libraries(03_BindlessQuad PRIVATE
    Core
    glfw
)

if(COMPILE_WITH_VULKAN)
    target_link_libraries(03_BindlessQuad PRIVATE Vulkan::Vulkan)
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(03_BindlessQuad PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(03_BindlessQuad PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(03_BindlessQuad PRIVATE /utf-8)
else()
    target_compile_options(03_BindlessQuad PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — bin/{Config}-{OS}-x86_64/03_BindlessQuad/, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET 03_BindlessQuad PROPERTY FOLDER "Projects")

set_target_properties(03_BindlessQuad PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY             "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/03_BindlessQuad"
    VS_DEBUGGER_WORKING_DIRECTORY        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# Shader compilation via DXC
# DXC ships with the Vulkan SDK (VK_SDK_PATH / VULKAN_SDK) and the Windows SDK.
# Output .spv (Vulkan) and/or .cso (D3D12) go into 03_BindlessQuad/shaders/ so they are
# found at runtime relative to VS_DEBUGGER_WORKING_DIRECTORY (03_BindlessQuad/).
# ---------------------------------------------------------------------------
find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin"
    DOC "DirectX Shader Compiler"
)

if(NOT DXC_EXECUTABLE)
    message(WARNING
        "[03_BindlessQuad] dxc not found — shaders will not be compiled automatically. "
        "Ensure dxc.exe is on PATH or that VULKAN_SDK/VK_SDK_PATH is set.")
else()
    message(STATUS "DXC found: ${DXC_EXECUTABLE}")

    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_OUTPUTS)

# ---- Vertex shader ----
    if(COMPILE_WITH_VULKAN)
        add_custom_command(
            OUTPUT  "${SHADER_DIR}/quad_vert.spv"
            COMMAND "${DXC_EXECUTABLE}"
                    -T vs_6_0 -E main
                    -spirv -fspv-target-env=vulkan1.1
                    "${SHADER_DIR}/quad.vs.hlsl"
                    -Fo "${SHADER_DIR}/quad_vert.spv"
            DEPENDS "${SHADER_DIR}/quad.vs.hlsl"
            COMMENT "HLSL -> SPIR-V: quad.vs.hlsl"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS "${SHADER_DIR}/quad_vert.spv")
    endif()

    if(COMPILE_WITH_DX12)
        add_custom_command(
            OUTPUT  "${SHADER_DIR}/quad_vert.cso"
            COMMAND "${DXC_EXECUTABLE}"
                    -T vs_6_0 -E main
                    "${SHADER_DIR}/quad.vs.hlsl"
                    -Fo "${SHADER_DIR}/quad_vert.cso"
            DEPENDS "${SHADER_DIR}/quad.vs.hlsl"
            COMMENT "HLSL -> DXIL: quad.vs.hlsl"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS "${SHADER_DIR}/quad_vert.cso")
    endif()

    # ---- Pixel shader ----
    if(COMPILE_WITH_VULKAN)
        add_custom_command(
            OUTPUT  "${SHADER_DIR}/quad_frag.spv"
            COMMAND "${DXC_EXECUTABLE}"
                    -T ps_6_0 -E main
                    -spirv -fspv-target-env=vulkan1.1
                    "${SHADER_DIR}/quad.ps.hlsl"
                    -Fo "${SHADER_DIR}/quad_frag.spv"
            DEPENDS "${SHADER_DIR}/quad.ps.hlsl"
            COMMENT "HLSL -> SPIR-V: quad.ps.hlsl"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS "${SHADER_DIR}/quad_frag.spv")
    endif()

    if(COMPILE_WITH_DX12)
        add_custom_command(
            OUTPUT  "${SHADER_DIR}/quad_frag.cso"
            COMMAND "${DXC_EXECUTABLE}"
                    -T ps_6_0 -E main
                    "${SHADER_DIR}/quad.ps.hlsl"
                    -Fo "${SHADER_DIR}/quad_frag.cso"
            DEPENDS "${SHADER_DIR}/quad.ps.hlsl"
            COMMENT "HLSL -> DXIL: quad.ps.hlsl"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS "${SHADER_DIR}/quad_frag.cso")
    endif()

    file(GLOB 03_BindlessQuad_SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsl")
    # Prevent VS from invoking FXC on these files — compilation is handled
    # exclusively by the add_custom_command targets above (DXC via CMake).
    # VS_TOOL_OVERRIDE "None" is required; HEADER_FILE_ONLY alone does not
    # suppress VS's built-in HLSL item handler.
    set_source_files_properties(${03_BindlessQuad_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
    add_custom_target(03_BindlessQuadShaders ALL SOURCES ${03_BindlessQuad_SHADERS} DEPENDS ${SHADER_OUTPUTS})
    source_group("shaders" FILES ${03_BindlessQuad_SHADERS})
    set_property(TARGET 03_BindlessQuadShaders PROPERTY FOLDER "Shaders")
    add_dependencies(03_BindlessQuad 03_BindlessQuadShaders)
endif()
