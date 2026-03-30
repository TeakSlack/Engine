file(GLOB_RECURSE DR_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE DR_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(DeferredRender ${DR_SOURCES} ${DR_HEADERS})

# Mirror filesystem structure in VS filters
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${DR_SOURCES} ${DR_HEADERS})

# Core brings in Core/src and VendorIncludes transitively
target_link_libraries(DeferredRender PRIVATE
    Core
    Vulkan::Vulkan
    glfw
    nvrhi
    nvrhi_vk
)

# ---------------------------------------------------------------------------
# Shader compilation — runs the existing scripts as a pre-build step
# Note: PRE_BUILD is honoured by Visual Studio; on Makefile/Ninja generators
# it runs before linking, which is still correct for shader outputs.
# ---------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET DeferredRender PRE_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/compile-shaders.bat"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Compiling GLSL shaders to SPIR-V..."
    )
else()
    add_custom_command(TARGET DeferredRender PRE_BUILD
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/compile-shaders.sh"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Compiling GLSL shaders to SPIR-V..."
    )
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(DeferredRender PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(DeferredRender PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(DeferredRender PRIVATE /utf-8)
else()
    target_compile_options(DeferredRender PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — bin/{Config}-{OS}-x86_64/DeferredRender/, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET DeferredRender PROPERTY FOLDER "Projects")

set_target_properties(DeferredRender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/DeferredRender"
)
