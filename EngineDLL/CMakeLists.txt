file(GLOB_RECURSE ENGINEDLL_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE ENGINEDLL_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_library(EngineDLL SHARED ${ENGINEDLL_SOURCES} ${ENGINEDLL_HEADERS})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src"
    FILES ${ENGINEDLL_SOURCES} ${ENGINEDLL_HEADERS})

# ENGINE_DLL_EXPORTS is defined only for this target so dllexport is used
# when building the DLL; consumers get dllimport via the same header.
target_compile_definitions(EngineDLL PRIVATE ENGINE_DLL_EXPORTS)

# Core propagates its include paths (Core/src, VendorIncludes) and backend
# link libraries transitively, so no extra include_directories are needed.
target_link_libraries(EngineDLL PRIVATE Core)

# ---------------------------------------------------------------------------
# Compiler settings — mirror Core/App for consistency
# ---------------------------------------------------------------------------
target_compile_definitions(EngineDLL PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(EngineDLL PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(EngineDLL PRIVATE /utf-8)
else()
    target_compile_options(EngineDLL PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — alongside the App binary so it's found at runtime
# ---------------------------------------------------------------------------
set_property(TARGET EngineDLL PROPERTY FOLDER "Projects")

set_target_properties(EngineDLL PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY          "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/EngineDLL"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_SOURCE_DIR}/bin/Debug-${CMAKE_SYSTEM_NAME}-x86_64/EngineDLL"
    RUNTIME_OUTPUT_DIRECTORY_DEVELOPMENT "${CMAKE_SOURCE_DIR}/bin/Development-${CMAKE_SYSTEM_NAME}-x86_64/EngineDLL"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_SOURCE_DIR}/bin/Release-${CMAKE_SYSTEM_NAME}-x86_64/EngineDLL"
)
