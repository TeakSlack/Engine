# Gather shared sources, excluding backend files so they are only compiled
# when their corresponding option is ON.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS "src/*.h")

list(FILTER CORE_SOURCES EXCLUDE REGEX "VulkanDevice\\.cpp$")
list(FILTER CORE_SOURCES EXCLUDE REGEX "D3D12Device\\.cpp$")

set(BACKEND_SOURCES)
if(COMPILE_WITH_VULKAN)
    list(APPEND BACKEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/VulkanDevice.cpp")
endif()
if(COMPILE_WITH_DX12)
    list(APPEND BACKEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/D3D12Device.cpp")
endif()

add_library(Core STATIC ${CORE_SOURCES} ${BACKEND_SOURCES} ${CORE_HEADERS})

# ---------------------------------------------------------------------------
# Includes
# PUBLIC: Core/src is propagated to any target that links Core, so App
# can include engine headers without repeating the path.
# ---------------------------------------------------------------------------
target_include_directories(Core PUBLIC
    ${CMAKE_SOURCE_DIR}/Core/src
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${CORE_SOURCES} ${BACKEND_SOURCES} ${CORE_HEADERS})

# Base libraries — always required regardless of backend
target_link_libraries(Core PUBLIC
    VendorIncludes
    glfw
    nvrhi
    fastgltf
    nlohmann_json::nlohmann_json
)

# ---------------------------------------------------------------------------
# Backend-specific libraries and macros
# PUBLIC so App and any other consumer of Core sees the same
# COMPILE_WITH_VULKAN / COMPILE_WITH_DX12 macros without re-declaring them.
# ---------------------------------------------------------------------------
if(COMPILE_WITH_VULKAN)
    target_link_libraries(Core PUBLIC Vulkan::Vulkan nvrhi_vk)
    target_compile_definitions(Core PUBLIC COMPILE_WITH_VULKAN)
endif()

if(COMPILE_WITH_DX12)
    target_link_libraries(Core PUBLIC nvrhi_d3d12 dxgi)
    target_compile_definitions(Core PUBLIC COMPILE_WITH_DX12)
endif()

if(COMPILE_WITH_ASSERT)
    target_compile_definitions(Core PUBLIC COMPILE_WITH_ASSERT)
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(Core PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(Core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(Core PRIVATE /utf-8)
else()
    target_compile_options(Core PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — flat lib/ directory at workspace root, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET Core PROPERTY FOLDER "Projects")

set_target_properties(Core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY                "${CMAKE_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEVELOPMENT    "${CMAKE_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/lib"
)

# ---------------------------------------------------------------------------
# Shader compilation via DXC
#
# Naming convention:  <stem>.<stage>.hlsl
#   e.g. SkyTransmittance.cs.hlsl  ->  profile cs_6_6
#        GBuffer.vs.hlsl           ->  profile vs_6_6
#
# Outputs land next to their source in Core/shaders/ so the engine can
# locate them at runtime relative to the working directory.
# All .hlsli files are listed as extra dependencies so any include change
# triggers a full rebuild of affected shaders.
# ---------------------------------------------------------------------------
find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin"
    DOC "DirectX Shader Compiler"
)

find_path(DXC_INCLUDE_DIR dxc/dxcapi.h
    HINTS "$ENV{VULKAN_SDK}/Include" "$ENV{VK_SDK_PATH}/Include")
find_library(DXC_LIB dxcompiler
    HINTS "$ENV{VULKAN_SDK}/Lib" "$ENV{VK_SDK_PATH}/Lib")

if(DXC_INCLUDE_DIR AND DXC_LIB)
    message(STATUS "[Core] DXC runtime found: ${DXC_LIB}")
    target_include_directories(Core PRIVATE ${DXC_INCLUDE_DIR})
    target_link_libraries(Core PRIVATE ${DXC_LIB})
    target_compile_definitions(Core PUBLIC CORE_HAS_DXC)
else()
    message(WARNING "[Core] dxcapi.h or dxcompiler.lib not found — HlslImporter disabled")
endif()

if(NOT DXC_EXECUTABLE)
    message(WARNING
        "[Core] dxc not found — shaders will not be compiled automatically. "
        "Ensure dxc.exe is on PATH or that VULKAN_SDK/VK_SDK_PATH is set.")
else()
    message(STATUS "[Core] DXC found: ${DXC_EXECUTABLE}")

    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

    file(GLOB_RECURSE CORE_HLSL_SOURCES  CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsl")
    file(GLOB_RECURSE CORE_HLSLI_SOURCES CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsli")

    set(SHADER_OUTPUTS)

    foreach(HLSL IN LISTS CORE_HLSL_SOURCES)
        # Skip SkyLuts — already handled above.
        if("${HLSL}" STREQUAL "${SKYLUTS_HLSL}")
            continue()
        endif()
        get_filename_component(FULL_NAME "${HLSL}" NAME)

        # Parse <stem>.<stage>.hlsl → STEM and STAGE
        string(REGEX REPLACE "\\.hlsl$"               ""     BASE  "${FULL_NAME}")
        string(REGEX REPLACE "^.*\\.([a-z]+)$"        "\\1"  STAGE "${BASE}")
        string(REGEX REPLACE "^(.*)\\.([a-z]+)$"      "\\1"  STEM  "${BASE}")

        set(PROFILE "${STAGE}_6_6")

        if(COMPILE_WITH_VULKAN)
            set(SPV_OUT "${SHADER_DIR}/${STEM}.${STAGE}.spv")
            add_custom_command(
                OUTPUT  "${SPV_OUT}"
                COMMAND "${DXC_EXECUTABLE}"
                        -T "${PROFILE}" -E main
                        -spirv -fspv-target-env=vulkan1.1
                        -fvk-b-shift 0 0
                        -fvk-t-shift 2 0
                        -fvk-s-shift 3 0
                        -fvk-u-shift 4 0
                        -I "${SHADER_DIR}"
                        "${HLSL}"
                        -Fo "${SPV_OUT}"
                DEPENDS "${HLSL}" ${CORE_HLSLI_SOURCES}
                COMMENT "HLSL -> SPIR-V: ${FULL_NAME}"
                VERBATIM
            )
            list(APPEND SHADER_OUTPUTS "${SPV_OUT}")
        endif()

        if(COMPILE_WITH_DX12)
            set(CSO_OUT "${SHADER_DIR}/${STEM}.${STAGE}.cso")
            add_custom_command(
                OUTPUT  "${CSO_OUT}"
                COMMAND "${DXC_EXECUTABLE}"
                        -T "${PROFILE}" -E main
                        -I "${SHADER_DIR}"
                        "${HLSL}"
                        -Fo "${CSO_OUT}"
                DEPENDS "${HLSL}" ${CORE_HLSLI_SOURCES}
                COMMENT "HLSL -> DXIL: ${FULL_NAME}"
                VERBATIM
            )
            list(APPEND SHADER_OUTPUTS "${CSO_OUT}")
        endif()
    endforeach()

    # Suppress VS's built-in HLSL item handler — compilation is owned by the
    # custom commands above. VS_TOOL_OVERRIDE "None" is required; setting
    # HEADER_FILE_ONLY alone does not disable the built-in FXC integration.
    set_source_files_properties(${CORE_HLSL_SOURCES} ${CORE_HLSLI_SOURCES}
        PROPERTIES VS_TOOL_OVERRIDE "None")

    add_custom_target(CoreShaders ALL
        SOURCES ${CORE_HLSL_SOURCES} ${CORE_HLSLI_SOURCES}
        DEPENDS ${SHADER_OUTPUTS})
    source_group("shaders" FILES ${CORE_HLSL_SOURCES} ${CORE_HLSLI_SOURCES})
    set_property(TARGET CoreShaders PROPERTY FOLDER "Shaders")
    add_dependencies(Core CoreShaders)
endif()
