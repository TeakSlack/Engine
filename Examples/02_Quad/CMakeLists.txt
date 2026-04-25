file(GLOB_RECURSE 02_Quad_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE 02_Quad_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(02_Quad ${02_Quad_SOURCES} ${02_Quad_HEADERS})

# Mirror filesystem structure in VS filters
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${02_Quad_SOURCES} ${02_Quad_HEADERS})

# Core brings in Core/src, VendorIncludes, and backend libs transitively.
# Vulkan::Vulkan is linked directly here because 02_QuadLayer.cpp owns the
# VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE definition, which
# requires the Vulkan headers to be visible in that translation unit.
target_link_libraries(02_Quad PRIVATE
    Core
    glfw
)

if(COMPILE_WITH_VULKAN)
    target_link_libraries(02_Quad PRIVATE Vulkan::Vulkan)
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(02_Quad PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(02_Quad PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(02_Quad PRIVATE /utf-8)
else()
    target_compile_options(02_Quad PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — bin/{Config}-{OS}-x86_64/02_Quad/, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET 02_Quad PROPERTY FOLDER "Projects")

set_target_properties(02_Quad PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY             "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/02_Quad"
    VS_DEBUGGER_WORKING_DIRECTORY        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# Shader compilation via DXC
# DXC ships with the Vulkan SDK (VK_SDK_PATH / VULKAN_SDK) and the Windows SDK.
# Output .spv (Vulkan) and/or .cso (D3D12) go into 02_Quad/shaders/ so they are
# found at runtime relative to VS_DEBUGGER_WORKING_DIRECTORY (02_Quad/).
# ---------------------------------------------------------------------------
find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin"
    DOC "DirectX Shader Compiler"
)

if(NOT DXC_EXECUTABLE)
    message(WARNING
        "[02_Quad] dxc not found — shaders will not be compiled automatically. "
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

    file(GLOB 02_Quad_SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsl")
    # Prevent VS from invoking FXC on these files — compilation is handled
    # exclusively by the add_custom_command targets above (DXC via CMake).
    # VS_TOOL_OVERRIDE "None" is required; HEADER_FILE_ONLY alone does not
    # suppress VS's built-in HLSL item handler.
    set_source_files_properties(${02_Quad_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
    add_custom_target(02_QuadShaders ALL SOURCES ${02_Quad_SHADERS} DEPENDS ${SHADER_OUTPUTS})
    source_group("shaders" FILES ${02_Quad_SHADERS})
    set_property(TARGET 02_QuadShaders PROPERTY FOLDER "Shaders")
    add_dependencies(02_Quad 02_QuadShaders)
endif()
