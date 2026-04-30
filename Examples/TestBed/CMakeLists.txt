file(GLOB_RECURSE TestBed_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE TestBed_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(TestBed ${TestBed_SOURCES} ${TestBed_HEADERS})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${TestBed_SOURCES} ${TestBed_HEADERS})

target_link_libraries(TestBed PRIVATE Core glfw)

if(COMPILE_WITH_VULKAN)
    target_link_libraries(TestBed PRIVATE Vulkan::Vulkan)
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(TestBed PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(TestBed PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(TestBed PRIVATE /utf-8)
else()
    target_compile_options(TestBed PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
set_property(TARGET TestBed PROPERTY FOLDER "Projects")

set_target_properties(TestBed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY      "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/TestBed"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------
# Shader compilation via DXC
# Naming convention: <stem>.<stage>.hlsl -> profile <stage>_6_6
# Outputs land in TestBed/shaders/ so they are found at runtime relative to
# VS_DEBUGGER_WORKING_DIRECTORY.
# ---------------------------------------------------------------------------
find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VK_SDK_PATH}/Bin"
    DOC "DirectX Shader Compiler"
)

if(NOT DXC_EXECUTABLE)
    message(WARNING
        "[TestBed] dxc not found — shaders will not be compiled automatically. "
        "Ensure dxc.exe is on PATH or that VULKAN_SDK/VK_SDK_PATH is set.")
else()
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

    file(GLOB_RECURSE TestBed_HLSL_SOURCES  CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsl")
    file(GLOB_RECURSE TestBed_HLSLI_SOURCES CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsli")

    set(SHADER_OUTPUTS)

    foreach(HLSL IN LISTS TestBed_HLSL_SOURCES)
        get_filename_component(FULL_NAME "${HLSL}" NAME)

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
                DEPENDS "${HLSL}" ${TestBed_HLSLI_SOURCES}
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
                DEPENDS "${HLSL}" ${TestBed_HLSLI_SOURCES}
                COMMENT "HLSL -> DXIL: ${FULL_NAME}"
                VERBATIM
            )
            list(APPEND SHADER_OUTPUTS "${CSO_OUT}")
        endif()
    endforeach()

    set_source_files_properties(${TestBed_HLSL_SOURCES} ${TestBed_HLSLI_SOURCES}
        PROPERTIES VS_TOOL_OVERRIDE "None")

    add_custom_target(TestBedShaders ALL
        SOURCES ${TestBed_HLSL_SOURCES} ${TestBed_HLSLI_SOURCES}
        DEPENDS ${SHADER_OUTPUTS})
    source_group("shaders" FILES ${TestBed_HLSL_SOURCES} ${TestBed_HLSLI_SOURCES})
    set_property(TARGET TestBedShaders PROPERTY FOLDER "Shaders")
    add_dependencies(TestBed TestBedShaders)

    # -------------------------------------------------------------------------
    # Copy compiled Core shaders into TestBed/shaders/ so passes like
    # SkyTransmittancePass can find their bytecode at runtime.
    #
    # Core's .hlsl sources exist at configure time, so we derive the expected
    # compiled output paths from them and copy each one with a proper dependency
    # chain: CoreShaders must finish before the copy, which must finish before
    # TestBed links.
    # -------------------------------------------------------------------------
    set(CORE_SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/Core/shaders")
    file(GLOB_RECURSE CORE_HLSL_SOURCES CONFIGURE_DEPENDS "${CORE_SHADER_SRC_DIR}/*.hlsl")

    set(CORE_SHADER_COPY_OUTPUTS)

    foreach(HLSL IN LISTS CORE_HLSL_SOURCES)
        get_filename_component(FULL_NAME "${HLSL}" NAME)

        string(REGEX REPLACE "\\.hlsl$"          ""    BASE  "${FULL_NAME}")
        string(REGEX REPLACE "^.*\\.([a-z]+)$"   "\\1" STAGE "${BASE}")
        string(REGEX REPLACE "^(.*)\\.([a-z]+)$" "\\1" STEM  "${BASE}")

        if(COMPILE_WITH_VULKAN)
            set(SRC "${CORE_SHADER_SRC_DIR}/${STEM}.${STAGE}.spv")
            set(DST "${SHADER_DIR}/${STEM}.${STAGE}.spv")
            add_custom_command(
                OUTPUT  "${DST}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC}" "${DST}"
                DEPENDS "${SRC}"
                COMMENT "Copying Core SPIR-V: ${STEM}.${STAGE}.spv"
                VERBATIM
            )
            list(APPEND CORE_SHADER_COPY_OUTPUTS "${DST}")
        endif()

        if(COMPILE_WITH_DX12)
            set(SRC "${CORE_SHADER_SRC_DIR}/${STEM}.${STAGE}.cso")
            set(DST "${SHADER_DIR}/${STEM}.${STAGE}.cso")
            add_custom_command(
                OUTPUT  "${DST}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC}" "${DST}"
                DEPENDS "${SRC}"
                COMMENT "Copying Core DXIL: ${STEM}.${STAGE}.cso"
                VERBATIM
            )
            list(APPEND CORE_SHADER_COPY_OUTPUTS "${DST}")
        endif()
    endforeach()

    if(CORE_SHADER_COPY_OUTPUTS)
        add_custom_target(TestBedCoreShaders ALL DEPENDS ${CORE_SHADER_COPY_OUTPUTS})
        add_dependencies(TestBedCoreShaders CoreShaders)
        add_dependencies(TestBed TestBedCoreShaders)
        set_property(TARGET TestBedCoreShaders PROPERTY FOLDER "Shaders")
    endif()
endif()
